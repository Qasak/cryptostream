#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <libwebsockets.h>
#include <stdbool.h>

#define MAX_PAYLOAD_SIZE 65536
#define MAX_SUBSCRIPTIONS 10

typedef struct {
    struct lws_context *context;
    struct lws *wsi;
    char *server_address;
    int port;
    char *path;
    bool connected;
    bool running;
    
    // Proxy settings
    bool use_proxy;
    char *proxy_address;
    int proxy_port;
    char *proxy_username;
    char *proxy_password;
    
    // Subscription management
    char *subscriptions[MAX_SUBSCRIPTIONS];
    int subscription_count;
    
    // Callback for received data
    void (*on_message)(const char *data, size_t len);
    void (*on_connect)(void);
    void (*on_disconnect)(void);
    void (*on_error)(const char *error);
} ws_client_t;

// Initialize WebSocket client
ws_client_t* ws_client_create(const char *server, int port, const char *path);

// Set proxy configuration
void ws_client_set_proxy(ws_client_t *client, const char *proxy_address, int proxy_port, 
                        const char *username, const char *password);

// Connect to WebSocket server
int ws_client_connect(ws_client_t *client);

// Send message to server
int ws_client_send(ws_client_t *client, const char *message);

// Subscribe to stream
int ws_client_subscribe(ws_client_t *client, const char *stream);

// Unsubscribe from stream
int ws_client_unsubscribe(ws_client_t *client, const char *stream);

// Run event loop
void ws_client_run(ws_client_t *client);

// Stop client
void ws_client_stop(ws_client_t *client);

// Destroy client
void ws_client_destroy(ws_client_t *client);

#endif // WS_CLIENT_H