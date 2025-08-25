#include "ws_client.h"
#include "json_parser.h"
#include "subscription.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

static ws_client_t *global_client = NULL;

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    if (global_client) {
        ws_client_stop(global_client);
    }
}

void on_message(const char *data, size_t len) {
    printf("\nReceived message: %s\n", data);
    
    // Parse and print market data
    market_data_t *market_data = parse_market_data(data, len);
    if (market_data) {
        print_market_data(market_data);
        free_market_data(market_data);
    }
}

void on_connect(void) {
    printf("Connected to Binance WebSocket\n");
    
    if (global_client) {
        // Subscribe to multiple streams after connection
        printf("Subscribing to streams...\n");
        
        // Example 1: Subscribe to BTC/USDT aggregate trades
        ws_client_subscribe(global_client, "btcusdt@aggTrade");
        
        // Example 2: Subscribe to ETH/USDT mark price
        ws_client_subscribe(global_client, "ethusdt@markPrice");
        
        // Example 3: Subscribe to BTC/USDT 1m klines
        ws_client_subscribe(global_client, "btcusdt@kline_1m");
        
        // Example 4: Subscribe to BTC/USDT ticker
        ws_client_subscribe(global_client, "btcusdt@ticker");
        
        // Example 5: Subscribe to BTC/USDT book ticker
        ws_client_subscribe(global_client, "btcusdt@bookTicker");
    }
}

void on_disconnect(void) {
    printf("Disconnected from Binance WebSocket\n");
}

void on_error(const char *error) {
    fprintf(stderr, "WebSocket error: %s\n", error);
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help                Show this help message\n");
    printf("  -p, --proxy               Enable proxy connection\n");
    printf("  -a, --proxy-addr ADDRESS  Proxy server address (default: 127.0.0.1)\n");
    printf("  -P, --proxy-port PORT     Proxy server port (default: 7890)\n");
    printf("  -u, --proxy-user USERNAME Proxy username (optional)\n");
    printf("  -w, --proxy-pass PASSWORD Proxy password (optional)\n");
    printf("  -c, --config FILE         Load configuration from file\n");
    printf("\nExamples:\n");
    printf("  %s                        # Direct connection\n", program_name);
    printf("  %s -p                     # Use proxy at 127.0.0.1:7890\n", program_name);
    printf("  %s -p -a 192.168.1.100 -P 8080  # Custom proxy\n", program_name);
    printf("  %s -c config.txt          # Load from config file\n", program_name);
}

void load_config_file(const char *filename, bool *use_proxy, char **proxy_addr, 
                     int *proxy_port, char **proxy_user, char **proxy_pass) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Warning: Cannot open config file: %s\n", filename);
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        
        char key[64], value[192];
        if (sscanf(line, "%63[^=]=%191s", key, value) == 2) {
            if (strcmp(key, "use_proxy") == 0) {
                *use_proxy = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            } else if (strcmp(key, "proxy_address") == 0) {
                *proxy_addr = strdup(value);
            } else if (strcmp(key, "proxy_port") == 0) {
                *proxy_port = atoi(value);
            } else if (strcmp(key, "proxy_username") == 0) {
                *proxy_user = strdup(value);
            } else if (strcmp(key, "proxy_password") == 0) {
                *proxy_pass = strdup(value);
            }
        }
    }
    
    fclose(file);
    printf("Configuration loaded from %s\n", filename);
}

int main(int argc, char *argv[]) {
    printf("Binance WebSocket Client\n");
    printf("========================\n\n");
    
    // Default proxy settings
    bool use_proxy = false;
    char *proxy_address = NULL;
    int proxy_port = 7890;
    char *proxy_username = NULL;
    char *proxy_password = NULL;
    char *config_file = NULL;
    
    // Parse command line options
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"proxy", no_argument, 0, 'p'},
        {"proxy-addr", required_argument, 0, 'a'},
        {"proxy-port", required_argument, 0, 'P'},
        {"proxy-user", required_argument, 0, 'u'},
        {"proxy-pass", required_argument, 0, 'w'},
        {"config", required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hpa:P:u:w:c:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'p':
                use_proxy = true;
                if (!proxy_address) {
                    proxy_address = strdup("127.0.0.1");
                }
                break;
            case 'a':
                proxy_address = strdup(optarg);
                break;
            case 'P':
                proxy_port = atoi(optarg);
                break;
            case 'u':
                proxy_username = strdup(optarg);
                break;
            case 'w':
                proxy_password = strdup(optarg);
                break;
            case 'c':
                config_file = strdup(optarg);
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Load config file if specified
    if (config_file) {
        load_config_file(config_file, &use_proxy, &proxy_address, 
                        &proxy_port, &proxy_username, &proxy_password);
        free(config_file);
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create WebSocket client
    const char *server = "fstream.binance.com";
    int port = 443;
    const char *path = "/ws";
    
    global_client = ws_client_create(server, port, path);
    if (!global_client) {
        fprintf(stderr, "Failed to create WebSocket client\n");
        return 1;
    }
    
    // Configure proxy if enabled
    if (use_proxy) {
        printf("\n=== Proxy Configuration ===\n");
        ws_client_set_proxy(global_client, proxy_address, proxy_port, 
                           proxy_username, proxy_password);
        printf("===========================\n\n");
    } else {
        printf("Using direct connection (no proxy)\n\n");
    }
    
    // Set callbacks
    global_client->on_message = on_message;
    global_client->on_connect = on_connect;
    global_client->on_disconnect = on_disconnect;
    global_client->on_error = on_error;
    
    // Connect to server
    printf("Connecting to %s:%d%s\n", server, port, path);
    if (ws_client_connect(global_client) < 0) {
        fprintf(stderr, "Failed to connect to WebSocket server\n");
        ws_client_destroy(global_client);
        return 1;
    }
    
    // Run event loop
    printf("Starting event loop (Press Ctrl+C to stop)...\n\n");
    ws_client_run(global_client);
    
    // Cleanup
    printf("Cleaning up...\n");
    ws_client_destroy(global_client);
    
    // Free proxy settings
    free(proxy_address);
    free(proxy_username);
    free(proxy_password);
    
    return 0;
}