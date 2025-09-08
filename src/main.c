#include "ws_client.h"
#include "json_parser.h"
#include "subscription.h"
#include "orderbook.h"
#include "binance_rest.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <curl/curl.h>
#include <stdarg.h>
#include <time.h>
#include <stdint.h>

static ws_client_t *global_client = NULL;
static OrderBook *orderbook = NULL;

// 处理订单的辅助函数
static void process_orders(OrderBook *book, double **orders, int count, 
                         PriceLevel *side, int *side_count, 
                         int (*compare)(const void *, const void *)) {
    if (!book || !orders || !side || !side_count) return;
    
    // 预分配内存
    if (*side_count + count > MAX_ORDER_BOOK_SIZE) {
        count = MAX_ORDER_BOOK_SIZE - *side_count;
        if (count <= 0) return;
    }
    
    // 批量添加订单
    for (int i = 0; i < count; i++) {
        if (orders[i] && orders[i][1] > 0) {  // 只添加数量大于0的订单
            side[*side_count].price = orders[i][0];
            side[*side_count].quantity = orders[i][1];
            (*side_count)++;
        }
    }
    
    // 排序
    if (*side_count > 1) {
        qsort(side, *side_count, sizeof(PriceLevel), compare);
    }
}

// 错误报告
#define LOG_ERROR(fmt, ...) do { \
    fprintf(stderr, "[ERROR] "); \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
    fputc('\n', stderr); \
} while(0)

#define LOG_DEBUG(fmt, ...) do { \
    fprintf(stderr, "[DEBUG] "); \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
    fputc('\n', stderr); \
} while(0)

// 日志记录函数
void log_message(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    // 打印到控制台
    vprintf(fmt, args);
    printf("\n");
    
    // 写入日志文件
    FILE *log_file = fopen("orderbook.log", "a");
    if (log_file) {
        time_t now;
        time(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        
        fprintf(log_file, "[%s] ", time_str);
        vfprintf(log_file, fmt, args);
        fputc('\n', log_file);
        fclose(log_file);
    }
    
    va_end(args);
}

// 初始化订单簿
int init_orderbook(const char *symbol) {
    orderbook_snapshot_t snapshot = {0};
    int ret;
    
    // 使用编译时常量替代魔数
    const int SNAPSHOT_LIMIT = 1000;
    
    // 检查输入参数
    if (!symbol || !*symbol) {
        LOG_ERROR("Invalid symbol");
        return -1;
    }
    
    // 获取订单簿快照
    ret = fetch_orderbook_snapshot(symbol, SNAPSHOT_LIMIT, &snapshot);
    if (ret != 0) {
        LOG_ERROR("Failed to fetch order book snapshot for %s", symbol);
        return -1;
    }
    
    // 创建订单簿
    orderbook = create_order_book();
    if (!orderbook) {
        LOG_ERROR("Failed to create order book");
        free_orderbook_snapshot(&snapshot);
        return -1;
    }
    
    // 设置更新ID
    orderbook->last_update_id = snapshot.last_update_id;
    orderbook->first_update_id = snapshot.first_update_id;
    orderbook->is_initialized = false; 
    
    LOG_DEBUG("Initializing order book with lastUpdateId: %llu, firstUpdateId: %llu", 
             (unsigned long long)snapshot.last_update_id,
             (unsigned long long)snapshot.first_update_id);
    
    // 处理买单（按价格降序）
    if (snapshot.bid_count > 0) {
        process_orders(orderbook, snapshot.bids, snapshot.bid_count, 
                      orderbook->bids, &orderbook->bid_count, 
                      compare_bids);
    } else {
        LOG_DEBUG("No bid orders in snapshot");
    }
    
    // 处理卖单（按价格升序）
    if (snapshot.ask_count > 0) {
        process_orders(orderbook, snapshot.asks, snapshot.ask_count, 
                      orderbook->asks, &orderbook->ask_count, 
                      compare_asks);
    } else {
        LOG_DEBUG("No ask orders in snapshot");
    }
    
    // 检查订单簿初始化是否成功
    if (orderbook->bid_count == 0 || orderbook->ask_count == 0) {
        LOG_ERROR("Order book initialization failed - No valid orders");
        return -1;
    }
    
    free_orderbook_snapshot(&snapshot);
    return 0;
}

void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    if (global_client) {
        ws_client_stop(global_client);
    }
}

void on_message(const char *data, size_t len) {
    market_data_t *market_data = parse_market_data(data, len);
    if (!market_data) {
        log_message("Failed to parse market data");
        return;
    }
    
    if (market_data->event_type) {
        if (strcmp(market_data->event_type, "depthUpdate") == 0) {
            handle_depth_update(orderbook,
                             market_data->bid_prices,
                             market_data->bid_quantities,
                             market_data->bid_count,
                             market_data->ask_prices,
                             market_data->ask_quantities,
                             market_data->ask_count,
                             market_data->first_update_id,
                             market_data->final_update_id);
        } else {
            printf("Received event: %s\n", market_data->event_type);
        }
    }
    
    free_market_data(market_data);
}

void on_connect(void) {
    printf("Connected to Binance WebSocket\n");

    if (global_client) {
        // 初始化订单簿
        printf("Initializing order book...\n");
        if (init_orderbook("BTCUSDT") != 0) {
            fprintf(stderr, "Failed to initialize order book\n");
            ws_client_stop(global_client);
            return;
        }
    
        // 订阅深度更新流
        printf("Subscribing to depth update stream...\n");
        ws_client_subscribe(global_client, "btcusdt@depth@100ms");
        
        // 也可以订阅其他交易对
        // ws_client_subscribe(global_client, "btcusdt@depth@100ms");
        // ws_client_subscribe(global_client, "ethusdt@depth@100ms");
    }
}

void on_disconnect(void) {
    printf("Disconnected from Binance WebSocket\n");
}

void on_error(const char *error) {
    fprintf(stderr, "WebSocket error: %s\n", error);
}

void print_usage(const char *program_name) {
    printf("Usage: %s\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help    Show this help message\n");
    printf("\nExample:\n");
    printf("  %s            # Start the Binance WebSocket client\n", program_name);
}

int main(int argc, char *argv[]) {
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // 打开日志文件
    FILE *log_file = fopen("orderbook.log", "a");
    if (log_file) {
        time_t now = time(NULL);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(log_file, "\n=== Application started at %s ===\n", time_str);
        fclose(log_file);
    }
    printf("Binance WebSocket Client\n");
    printf("========================\n\n");
    
    
    // Parse command line options
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create WebSocket client
    const char *server = "fstream.binance.com";
    int port = 443;
    const char *path = "/ws";

    curl_global_cleanup();
    global_client = ws_client_create(server, port, path);
    if (!global_client) {
        fprintf(stderr, "Failed to create WebSocket client\n");
        return 1;
    }
    
    printf("Using direct connection (no proxy)\n\n");
    
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
    
    // 保存最后的状态
    if (orderbook) {
        log_message("Final order book state - Best Bid: %.8f, Best Ask: %.8f",
                   get_best_bid(orderbook), get_best_ask(orderbook));
        free_order_book(orderbook);
    }
    
    // 清理WebSocket客户端
    ws_client_destroy(global_client);
    
    
    // 清理CURL
    curl_global_cleanup();
    
    return 0;
}