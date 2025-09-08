#ifndef BINANCE_REST_H
#define BINANCE_REST_H

#include <stdint.h>
#include <stdbool.h>

// Structure to hold order book snapshot structure (aligned with OrderBook in orderbook.h)
typedef struct {
    double **bids;       // 2D array of [price, quantity] pairs
    double **asks;       // 2D array of [price, quantity] pairs
    int bid_count;      // Number of bid levels
    int ask_count;      // Number of ask levels
    int capacity;       // Current capacity
    uint64_t last_update_id;  // Last processed update ID
    uint64_t first_update_id; // First processed update ID
    bool is_initialized;      // Whether the order book is initialized
} orderbook_snapshot_t;

/**
 * @brief Fetches the order book snapshot from Binance REST API
 * 
 * @param symbol The trading pair symbol (e.g., "BNBBTC")
 * @param limit The maximum number of price levels to return (1-5000)
 * @param snapshot Output parameter to store the snapshot data
 * @return int 0 on success, non-zero on error
 */
int fetch_orderbook_snapshot(const char *symbol, int limit, orderbook_snapshot_t *snapshot);

/**
 * @brief Frees the memory allocated for the order book snapshot
 * 
 * @param snapshot The snapshot to free
 */
void free_orderbook_snapshot(orderbook_snapshot_t *snapshot);

#endif // BINANCE_REST_H
