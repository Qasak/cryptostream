#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <stdint.h>
#include <stdbool.h>

// 订单簿最大容量
#define MAX_ORDER_BOOK_SIZE 5000

// 价格水平
typedef struct {
    double price;
    double quantity;
} PriceLevel;

// 订单簿
typedef struct {
    PriceLevel *bids;        // 买单，按价格降序排列
    PriceLevel *asks;        // 卖单，按价格升序排列
    PriceLevel *prev_bids;   // 前一个状态的买单
    PriceLevel *prev_asks;   // 前一个状态的卖单
    int bid_count;
    int ask_count;
    int prev_bid_count;
    int prev_ask_count;
    int capacity;           // 当前分配的内存大小
    bool is_initialized;    // 是否已初始化
    uint64_t first_update_id; // 最后处理的更新ID
    uint64_t last_update_id; // 最后处理的更新ID
    uint64_t last_update_timestamp;  // 最后更新时间戳（毫秒）
} OrderBook;

// 初始化订单簿
OrderBook* create_order_book();

// 释放订单簿
void free_order_book(OrderBook *book);

// 清空订单簿
void reset_order_book(OrderBook *book);

// 处理深度更新
void handle_depth_update(OrderBook *book, 
                        double *bid_prices, 
                        double *bid_quantities, 
                        int bid_count,
                        double *ask_prices, 
                        double *ask_quantities, 
                        int ask_count,
                        uint64_t first_update_id,
                        uint64_t final_update_id);

// 获取最佳买卖价
double get_best_bid(const OrderBook *book);
double get_best_ask(const OrderBook *book);

// 比较函数声明
int compare_bids(const void *a, const void *b);
int compare_asks(const void *a, const void *b);

#endif // ORDERBOOK_H
