#include "json_parser.h"
#include "orderbook.h"
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

market_data_t* parse_market_data(const char *json_str, size_t len) {
    market_data_t *data = (market_data_t *)calloc(1, sizeof(market_data_t));
    if (!data) {
        return NULL;
    }
    
    // Parse JSON with length checking
    struct json_tokener *tok = json_tokener_new();
    struct json_object *root = json_tokener_parse_ex(tok, json_str, len);
    enum json_tokener_error jerr = json_tokener_get_error(tok);
    
    if (jerr != json_tokener_success) {
        fprintf(stderr, "JSON parse error: %s\n", json_tokener_error_desc(jerr));
        json_tokener_free(tok);
        free(data);
        return NULL;
    }
    
    if (!root) {
        json_tokener_free(tok);
        free(data);
        return NULL;
    }
    
    struct json_object *obj;
    
    // Check if it's an error response
    if (json_object_object_get_ex(root, "error", &obj)) {
        printf("Error in response: %s\n", json_object_get_string(obj));
        json_object_put(root);
        free(data);
        return NULL;
    }
    
    // Check if it's a subscription response
    if (json_object_object_get_ex(root, "result", &obj)) {
        printf("Subscription response received\n");
        json_object_put(root);
        return data;
    }
    
    // Parse event type
    if (json_object_object_get_ex(root, "e", &obj)) {
        data->event_type = strdup(json_object_get_string(obj));
    }
    
    // Parse symbol
    if (json_object_object_get_ex(root, "s", &obj)) {
        data->symbol = strdup(json_object_get_string(obj));
    }
    
    // Parse based on event type
    if (data->event_type) {
        if (strcmp(data->event_type, "aggTrade") == 0) {
            // Aggregate trade
            if (json_object_object_get_ex(root, "p", &obj)) {
                data->price = json_object_get_double(obj);
            }
            if (json_object_object_get_ex(root, "q", &obj)) {
                data->quantity = json_object_get_double(obj);
            }
            if (json_object_object_get_ex(root, "T", &obj)) {
                data->timestamp = json_object_get_int64(obj);
            }
        } else if (strcmp(data->event_type, "markPriceUpdate") == 0) {
            // Mark price
            if (json_object_object_get_ex(root, "p", &obj)) {
                data->price = json_object_get_double(obj);
            }
            if (json_object_object_get_ex(root, "T", &obj)) {
                data->timestamp = json_object_get_int64(obj);
            }
        } else if (strcmp(data->event_type, "bookTicker") == 0) {
            // Book ticker
            if (json_object_object_get_ex(root, "b", &obj)) {
                data->bid_prices = (double *)malloc(sizeof(double));
                data->bid_prices[0] = atof(json_object_get_string(obj));
                data->bid_count = 1;
            }
            if (json_object_object_get_ex(root, "B", &obj)) {
                data->bid_quantities = (double *)malloc(sizeof(double));
                data->bid_quantities[0] = atof(json_object_get_string(obj));
            }
            if (json_object_object_get_ex(root, "a", &obj)) {
                data->ask_prices = (double *)malloc(sizeof(double));
                data->ask_prices[0] = atof(json_object_get_string(obj));
                data->ask_count = 1;
            }
            if (json_object_object_get_ex(root, "A", &obj)) {
                data->ask_quantities = (double *)malloc(sizeof(double));
                data->ask_quantities[0] = atof(json_object_get_string(obj));
            }
        } else if (strcmp(data->event_type, "depthUpdate") == 0) {
            // Parse update IDs for order book synchronization
            if (json_object_object_get_ex(root, "U", &obj)) {
                data->first_update_id = json_object_get_int64(obj);
            }
            if (json_object_object_get_ex(root, "u", &obj)) {
                data->final_update_id = json_object_get_int64(obj);
            }
            
            // Parse bids and asks
            if (json_object_object_get_ex(root, "b", &obj)) {
                int bid_array_len = json_object_array_length(obj);
                data->bid_count = bid_array_len;
                data->bid_prices = (double *)malloc(sizeof(double) * bid_array_len);
                data->bid_quantities = (double *)malloc(sizeof(double) * bid_array_len);
                
                for (int i = 0; i < bid_array_len; i++) {
                    struct json_object *bid = json_object_array_get_idx(obj, i);
                    struct json_object *price_obj = json_object_array_get_idx(bid, 0);
                    struct json_object *qty_obj = json_object_array_get_idx(bid, 1);
                    data->bid_prices[i] = atof(json_object_get_string(price_obj));
                    data->bid_quantities[i] = atof(json_object_get_string(qty_obj));
                }
            }
            
            if (json_object_object_get_ex(root, "a", &obj)) {
                int ask_array_len = json_object_array_length(obj);
                data->ask_count = ask_array_len;
                data->ask_prices = (double *)malloc(sizeof(double) * ask_array_len);
                data->ask_quantities = (double *)malloc(sizeof(double) * ask_array_len);
                
                for (int i = 0; i < ask_array_len; i++) {
                    struct json_object *ask = json_object_array_get_idx(obj, i);
                    struct json_object *price_obj = json_object_array_get_idx(ask, 0);
                    struct json_object *qty_obj = json_object_array_get_idx(ask, 1);
                    data->ask_prices[i] = atof(json_object_get_string(price_obj));
                    data->ask_quantities[i] = atof(json_object_get_string(qty_obj));
                }
            }
        }
    }
    
    json_object_put(root);
    json_tokener_free(tok);
    return data;
}

void free_market_data(market_data_t *data) {
    if (!data) {
        return;
    }
    
    free(data->event_type);
    free(data->symbol);
    free(data->bid_prices);
    free(data->bid_quantities);
    free(data->ask_prices);
    free(data->ask_quantities);
    free(data);
}

void print_market_data(const market_data_t *data) {
    if (!data || !data->event_type) {
        return;
    }
    
    printf("\n=== Market Data ===\n");
    printf("Event: %s\n", data->event_type);
    
    if (data->symbol) {
        printf("Symbol: %s\n", data->symbol);
    }
    
    if (strcmp(data->event_type, "aggTrade") == 0) {
        printf("Price: %.8f\n", data->price);
        printf("Quantity: %.8f\n", data->quantity);
        printf("Timestamp: %ld\n", data->timestamp);
    } else if (strcmp(data->event_type, "markPriceUpdate") == 0) {
        printf("Mark Price: %.8f\n", data->price);
        printf("Timestamp: %ld\n", data->timestamp);
    } else if (strcmp(data->event_type, "kline") == 0) {
        printf("Open: %.8f\n", data->open);
        printf("High: %.8f\n", data->high);
        printf("Low: %.8f\n", data->low);
        printf("Close: %.8f\n", data->close);
        printf("Volume: %.8f\n", data->volume);
        printf("Open Time: %ld\n", data->open_time);
        printf("Close Time: %ld\n", data->close_time);
    } else if (strcmp(data->event_type, "24hrTicker") == 0) {
        printf("Last Price: %.8f\n", data->price);
        printf("Volume: %.8f\n", data->volume);
    } else if (strcmp(data->event_type, "bookTicker") == 0) {
        if (data->bid_count > 0) {
            printf("Best Bid: %.8f @ %.8f\n", data->bid_prices[0], data->bid_quantities[0]);
        }
        if (data->ask_count > 0) {
            printf("Best Ask: %.8f @ %.8f\n", data->ask_prices[0], data->ask_quantities[0]);
        }
    } else if (strcmp(data->event_type, "depthUpdate") == 0) {
        printf("Bids (%d):\n", data->bid_count);
        for (int i = 0; i < data->bid_count && i < 5; i++) {
            printf("  %.8f @ %.8f\n", data->bid_prices[i], data->bid_quantities[i]);
        }
        printf("Asks (%d):\n", data->ask_count);
        for (int i = 0; i < data->ask_count && i < 5; i++) {
            printf("  %.8f @ %.8f\n", data->ask_prices[i], data->ask_quantities[i]);
        }
    }
    
    printf("==================\n");
}

