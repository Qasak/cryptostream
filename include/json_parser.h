#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

typedef struct {
    char *event_type;
    char *symbol;
    double price;
    double quantity;
    long timestamp;
    
    // For depth updates
    double *bid_prices;
    double *bid_quantities;
    double *ask_prices;
    double *ask_quantities;
    int bid_count;
    int ask_count;
    
    // For kline data
    long open_time;
    double open;
    double high;
    double low;
    double close;
    double volume;
    long close_time;
} market_data_t;

// Parse market data from JSON
market_data_t* parse_market_data(const char *json_str, size_t len);

// Free market data
void free_market_data(market_data_t *data);

// Print market data
void print_market_data(const market_data_t *data);

#endif // JSON_PARSER_H