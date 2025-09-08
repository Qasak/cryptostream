#include "binance_rest.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// Define a structure to hold our parsing context
typedef struct {
    orderbook_snapshot_t *snapshot;
    char *buffer;  // For accumulating data
    size_t size;
} parse_context_t;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    parse_context_t *ctx = (parse_context_t *)userp;
    
    // Reallocate buffer to hold new data
    char *ptr = realloc(ctx->buffer, ctx->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Failed to reallocate memory in write_callback\n");
        return 0;
    }
    
    ctx->buffer = ptr;
    memcpy(ctx->buffer + ctx->size, contents, realsize);
    ctx->size += realsize;
    ctx->buffer[ctx->size] = '\0';  // Null-terminate
    
    return realsize;
}

// Callback function for CURL to write response data
static int parse_complete_response(parse_context_t *ctx) {
    if (!ctx || !ctx->snapshot || !ctx->buffer || ctx->size == 0) {
        fprintf(stderr, "Invalid context or empty buffer\n");
        return -1;
    }

    int ret = -1;
    struct json_tokener *tok = NULL;
    struct json_object *jobj = NULL;
    
    tok = json_tokener_new();
    if (!tok) {
        fprintf(stderr, "Failed to create JSON tokener\n");
        return -1;
    }
    
    jobj = json_tokener_parse_ex(tok, ctx->buffer, ctx->size);
    
    if (tok->err != json_tokener_success) {
        fprintf(stderr, "JSON parse error: %s (at offset %zu)\n", 
               json_tokener_error_desc(tok->err),
               tok->char_offset);
        fprintf(stderr, "Context around error: ");
        size_t start = (tok->char_offset > 20) ? tok->char_offset - 20 : 0;
        size_t length = (ctx->size - start > 40) ? 40 : ctx->size - start;
        fprintf(stderr, "%.*s\n", (int)length, ctx->buffer + start);
        goto cleanup;
    }
    
    // We have a complete JSON object, process it
    orderbook_snapshot_t *snapshot = ctx->snapshot;
    struct json_object *bids = NULL, *asks = NULL, *item = NULL, *price_obj = NULL, *qty_obj = NULL;

    // Get lastUpdateId
    if (!json_object_object_get_ex(jobj, "lastUpdateId", &item) || !item) {
        fprintf(stderr, "Missing lastUpdateId in response. Available keys in response: ");
        json_object_object_foreach(jobj, key, val) {
            fprintf(stderr, "%s ", key);
        }
        fprintf(stderr, "\n");
        goto cleanup;
    }
    
    // Initialize the snapshot
    snapshot->last_update_id = json_object_get_uint64(item);
    snapshot->first_update_id = snapshot->last_update_id;
    snapshot->bid_count = 0;
    snapshot->ask_count = 0;
    snapshot->capacity = 0;
    snapshot->is_initialized = false;
    ret = 0;  // Success

    // Get bids
    if (json_object_object_get_ex(jobj, "bids", &bids)) {
        int num_bids = json_object_array_length(bids);
        if (num_bids > 0) {
            snapshot->bids = (double **)malloc(num_bids * sizeof(double *));
            if (!snapshot->bids) {
                fprintf(stderr, "Failed to allocate memory for bids\n");
                goto cleanup;
            }
            snapshot->bid_count = num_bids;
            snapshot->capacity = num_bids;
            
            for (int i = 0; i < num_bids; i++) {
                // Allocate memory for price and quantity pair
                snapshot->bids[i] = (double *)malloc(2 * sizeof(double));
                if (!snapshot->bids[i]) {
                    fprintf(stderr, "Failed to allocate memory for bid %d\n", i);
                    ret = -1;
                    goto cleanup;
                }
                
                struct json_object *bid = json_object_array_get_idx(bids, i);
                
                // Price
                price_obj = json_object_array_get_idx(bid, 0);
                snapshot->bids[i][0] = atof(json_object_get_string(price_obj));
                
                // Quantity
                qty_obj = json_object_array_get_idx(bid, 1);
                snapshot->bids[i][1] = atof(json_object_get_string(qty_obj));
            }
            snapshot->is_initialized = true;
        }
    }
    
    // Get asks
    if (json_object_object_get_ex(jobj, "asks", &asks)) {
        int num_asks = json_object_array_length(asks);
        if (num_asks > 0) {
            snapshot->asks = (double **)malloc(num_asks * sizeof(double *));
            if (!snapshot->asks) {
                fprintf(stderr, "Failed to allocate memory for asks\n");
                goto cleanup;
            }
            snapshot->ask_count = num_asks;
            
            for (int i = 0; i < num_asks; i++) {
                snapshot->asks[i] = (double *)malloc(2 * sizeof(double));
                if (!snapshot->asks[i]) {
                    fprintf(stderr, "Failed to allocate memory for ask %d\n", i);
                    ret = -1;
                    goto cleanup;
                }
                
                struct json_object *ask = json_object_array_get_idx(asks, i);
                
                // Price
                price_obj = json_object_array_get_idx(ask, 0);
                snapshot->asks[i][0] = atof(json_object_get_string(price_obj));
                
                // Quantity
                qty_obj = json_object_array_get_idx(ask, 1);
                snapshot->asks[i][1] = atof(json_object_get_string(qty_obj));
            }
        }
    }
cleanup:
    if (tok) {
        json_tokener_free(tok);
    }
    if (jobj) {
        json_object_put(jobj);
    }
    return ret;
}

int fetch_orderbook_snapshot(const char *symbol, int limit, orderbook_snapshot_t *snapshot) {
    CURL *curl;
    CURLcode res;
    char url[256];

    // Initialize CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl) {
        return -1;
    }
    
    // Construct the URL
    snprintf(url, sizeof(url), "https://api.binance.com/api/v3/depth?symbol=%s&limit=%d", 
             symbol, limit);

    parse_context_t ctx = {
        .snapshot = snapshot,
        .buffer = NULL,
        .size = 0
    };
    
    if (!snapshot) {
        fprintf(stderr, "Invalid snapshot pointer\n");
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }
    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // Perform the request
    res = curl_easy_perform(curl);
    // Check for errors
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }
    
    // Clean up
    if (ctx.buffer) {
        free(ctx.buffer);
    }
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    
    return (res == CURLE_OK && parse_complete_response(&ctx) == 0) ? 0 : -1;
}

void free_orderbook_snapshot(orderbook_snapshot_t *snapshot) {
    if (!snapshot) return;
    
    if (snapshot->bids) {
        free(snapshot->bids);
    }
    
    if (snapshot->asks) {
        for (int i = 0; i < snapshot->ask_count; i++) {
            if (snapshot->asks[i]) {
                free(snapshot->asks[i]);
            }
        }
        free(snapshot->asks);
    }
    
    memset(snapshot, 0, sizeof(orderbook_snapshot_t));
}
