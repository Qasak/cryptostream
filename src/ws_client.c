#include "ws_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static int callback_binance(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len) {
    ws_client_t *client = (ws_client_t *)user;
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("WebSocket connection established\n");
            client->connected = true;
            if (client->on_connect) {
                client->on_connect();
            }
            lws_callback_on_writable(wsi);
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (in && len > 0) {
                char *msg = (char *)malloc(len + 1);
                memcpy(msg, in, len);
                msg[len] = '\0';
                
                if (client->on_message) {
                    client->on_message(msg, len);
                }
                free(msg);
            }
            break;
            
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            // Handle pending subscriptions
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            if (in && len > 0) {
                char *error = (char *)malloc(len + 1);
                memcpy(error, in, len);
                error[len] = '\0';
                printf("Connection error: %s\n", error);
                if (client->on_error) {
                    client->on_error(error);
                }
                free(error);
            }
            client->connected = false;
            break;
            
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("WebSocket connection closed\n");
            client->connected = false;
            if (client->on_disconnect) {
                client->on_disconnect();
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "binance-protocol",
        callback_binance,
        sizeof(ws_client_t),
        MAX_PAYLOAD_SIZE,
    },
    { NULL, NULL, 0, 0 }
};

ws_client_t* ws_client_create(const char *server, int port, const char *path) {
    ws_client_t *client = (ws_client_t *)calloc(1, sizeof(ws_client_t));
    if (!client) {
        return NULL;
    }
    
    client->server_address = strdup(server);
    client->port = port;
    client->path = strdup(path);
    client->connected = false;
    client->running = false;
    client->subscription_count = 0;
    client->use_proxy = false;
    client->proxy_address = NULL;
    client->proxy_port = 0;
    client->proxy_username = NULL;
    client->proxy_password = NULL;
    
    return client;
}

void ws_client_set_proxy(ws_client_t *client, const char *proxy_address, int proxy_port,
                        const char *username, const char *password) {
    if (!client) {
        return;
    }
    
    client->use_proxy = true;
    
    if (client->proxy_address) {
        free(client->proxy_address);
    }
    client->proxy_address = proxy_address ? strdup(proxy_address) : NULL;
    client->proxy_port = proxy_port;
    
    if (client->proxy_username) {
        free(client->proxy_username);
    }
    client->proxy_username = username ? strdup(username) : NULL;
    
    if (client->proxy_password) {
        free(client->proxy_password);
    }
    client->proxy_password = password ? strdup(password) : NULL;
    
    printf("Proxy configured: %s:%d\n", proxy_address, proxy_port);
    if (username) {
        printf("Proxy authentication enabled\n");
    }
}

int ws_client_connect(ws_client_t *client) {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.user = client;
    
    // Configure proxy if enabled
    if (client->use_proxy && client->proxy_address) {
        info.http_proxy_address = client->proxy_address;
        info.http_proxy_port = client->proxy_port;
        printf("Using proxy: %s:%d\n", client->proxy_address, client->proxy_port);
    }
    
    client->context = lws_create_context(&info);
    if (!client->context) {
        fprintf(stderr, "Failed to create WebSocket context\n");
        return -1;
    }
    
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    
    ccinfo.context = client->context;
    ccinfo.address = client->server_address;
    ccinfo.port = client->port;
    ccinfo.path = client->path;
    ccinfo.host = client->server_address;
    ccinfo.origin = client->server_address;
    ccinfo.protocol = protocols[0].name;
    ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    ccinfo.userdata = client;
    
    client->wsi = lws_client_connect_via_info(&ccinfo);
    if (!client->wsi) {
        fprintf(stderr, "Failed to connect to WebSocket server\n");
        lws_context_destroy(client->context);
        return -1;
    }
    
    client->running = true;
    return 0;
}

int ws_client_send(ws_client_t *client, const char *message) {
    if (!client->connected) {
        fprintf(stderr, "WebSocket not connected\n");
        return -1;
    }
    
    size_t len = strlen(message);
    unsigned char *buf = (unsigned char *)malloc(LWS_PRE + len);
    if (!buf) {
        return -1;
    }
    
    memcpy(&buf[LWS_PRE], message, len);
    
    int n = lws_write(client->wsi, &buf[LWS_PRE], len, LWS_WRITE_TEXT);
    free(buf);
    
    if (n < 0) {
        fprintf(stderr, "Failed to send message\n");
        return -1;
    }
    
    return 0;
}

int ws_client_subscribe(ws_client_t *client, const char *stream) {
    if (client->subscription_count >= MAX_SUBSCRIPTIONS) {
        fprintf(stderr, "Maximum subscriptions reached\n");
        return -1;
    }
    
    // Build subscription JSON
    char message[1024];
    snprintf(message, sizeof(message),
             "{\"method\":\"SUBSCRIBE\",\"params\":[\"%s\"],\"id\":%d}",
             stream, client->subscription_count + 1);
    
    int result = ws_client_send(client, message);
    if (result == 0) {
        client->subscriptions[client->subscription_count] = strdup(stream);
        client->subscription_count++;
        printf("Subscribed to: %s\n", stream);
    }
    
    return result;
}

int ws_client_unsubscribe(ws_client_t *client, const char *stream) {
    // Find the subscription
    int index = -1;
    for (int i = 0; i < client->subscription_count; i++) {
        if (strcmp(client->subscriptions[i], stream) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        fprintf(stderr, "Stream not found in subscriptions\n");
        return -1;
    }
    
    // Build unsubscribe JSON
    char message[1024];
    snprintf(message, sizeof(message),
             "{\"method\":\"UNSUBSCRIBE\",\"params\":[\"%s\"],\"id\":%d}",
             stream, 100 + index);
    
    int result = ws_client_send(client, message);
    if (result == 0) {
        free(client->subscriptions[index]);
        // Shift remaining subscriptions
        for (int i = index; i < client->subscription_count - 1; i++) {
            client->subscriptions[i] = client->subscriptions[i + 1];
        }
        client->subscription_count--;
        printf("Unsubscribed from: %s\n", stream);
    }
    
    return result;
}

void ws_client_run(ws_client_t *client) {
    while (client->running && client->context) {
        lws_service(client->context, 50);
    }
}

void ws_client_stop(ws_client_t *client) {
    client->running = false;
}

void ws_client_destroy(ws_client_t *client) {
    if (!client) {
        return;
    }
    
    client->running = false;
    
    if (client->context) {
        lws_context_destroy(client->context);
    }
    
    // Free subscriptions
    for (int i = 0; i < client->subscription_count; i++) {
        free(client->subscriptions[i]);
    }
    
    // Free proxy settings
    free(client->proxy_address);
    free(client->proxy_username);
    free(client->proxy_password);
    
    free(client->server_address);
    free(client->path);
    free(client);
}