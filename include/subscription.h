#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

// Build subscription message for streams
char* build_subscribe_message(const char **streams, int count, int id);

// Build unsubscribe message for streams
char* build_unsubscribe_message(const char **streams, int count, int id);

// Common stream types
#define STREAM_AGGR_TRADE(symbol) symbol "@aggTrade"
#define STREAM_MARK_PRICE(symbol) symbol "@markPrice"
#define STREAM_KLINE(symbol, interval) symbol "@kline_" interval
#define STREAM_TICKER(symbol) symbol "@ticker"
#define STREAM_BOOK_TICKER(symbol) symbol "@bookTicker"
#define STREAM_DEPTH(symbol, levels) symbol "@depth" levels

#endif // SUBSCRIPTION_H