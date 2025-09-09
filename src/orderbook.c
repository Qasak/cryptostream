#include "orderbook.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <time.h>

// 获取当前时间戳（毫秒）
static uint64_t get_current_timestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX_ORDER_BOOK_LEVELS 5000
#define PRICE_EPSILON 1e-10
#define MAX_QTY_CHANGES 10.0

// 比较两个double是否相等
static int double_equal(double a, double b) {
    return fabs(a - b) < PRICE_EPSILON;
}

// Validate order book state and fix any inconsistencies
static bool validate_order_book(OrderBook *book) {
    if (!book) return false;
    
    // Check counts are within bounds
    if (book->bid_count < 0 || book->bid_count > book->capacity) {
        printf("Error: Invalid bid count: %d\n", book->bid_count);
        return false;
    }
    if (book->ask_count < 0 || book->ask_count > book->capacity) {
        printf("Error: Invalid ask count: %d\n", book->ask_count);
        return false;
    }
    
    // Check bids are in descending order and quantities are valid
    for (int i = 1; i < book->bid_count; i++) {
        if (book->bids[i].price > book->bids[i-1].price) {
            printf("Warning: Bids out of order at index %d: %.8f > %.8f\n", 
                  i, book->bids[i].price, book->bids[i-1].price);
            return false;
        }
        if (book->bids[i].quantity <= 0.0 || !isfinite(book->bids[i].quantity)) {
            printf("Warning: Invalid bid quantity at index %d: %.8f\n", 
                  i, book->bids[i].quantity);
            return false;
        }
    }
    
    // Check asks are in ascending order and quantities are valid
    for (int i = 1; i < book->ask_count; i++) {
        if (book->asks[i].price < book->asks[i-1].price) {
            printf("Warning: Asks out of order at index %d: %.8f < %.8f\n", 
                  i, book->asks[i].price, book->asks[i-1].price);
            return false;
        }
        if (book->asks[i].quantity <= 0.0 || !isfinite(book->asks[i].quantity)) {
            printf("Warning: Invalid ask quantity at index %d: %.8f\n", 
                  i, book->asks[i].quantity);
            return false;
        }
    }
    
    // Check for crossed book
    if (book->bid_count > 0 && book->ask_count > 0) {
        if (book->bids[0].price >= book->asks[0].price) {
            return false;
        }
    }
    
    return true;
}

// 比较函数，用于排序
static int compare(const void *a, const void *b) {
    const PriceLevel *pa = (const PriceLevel *)a;
    const PriceLevel *pb = (const PriceLevel *)b;
    
    // 降序排列
    if (pa->price < pb->price) return 1;
    if (pa->price > pb->price) return -1;
    // 如果价格相同，数量大的优先
    if (pa->quantity < pb->quantity) return 1;
    if (pa->quantity > pb->quantity) return -1;
    return 0;
}

int compare_bids(const void *a, const void *b) {
    const PriceLevel *pa = (const PriceLevel *)a;
    const PriceLevel *pb = (const PriceLevel *)b;
    // 买单按价格降序排列
    if (pa->price > pb->price) return -1;
    if (pa->price < pb->price) return 1;
    // 如果价格相同，数量大的优先
    if (pa->quantity < pb->quantity) return 1;
    if (pa->quantity > pb->quantity) return -1;
    return 0;
}

int compare_asks(const void *a, const void *b) {
    const PriceLevel *pa = (const PriceLevel *)a;
    const PriceLevel *pb = (const PriceLevel *)b;
    // 卖单按价格升序排列
    if (pa->price < pb->price) return -1;
    if (pa->price > pb->price) return 1;
    // 如果价格相同，数量大的优先
    if (pa->quantity < pb->quantity) return 1;
    if (pa->quantity > pb->quantity) return -1;
    return 0;
}

OrderBook* create_order_book() {
    OrderBook *book = (OrderBook*)calloc(1, sizeof(OrderBook));
    if (!book) return NULL;
    
    book->capacity = MAX_ORDER_BOOK_LEVELS;
    book->bids = (PriceLevel*)calloc(book->capacity, sizeof(PriceLevel));
    book->asks = (PriceLevel*)calloc(book->capacity, sizeof(PriceLevel));
    
    if (!book->bids || !book->asks) {
        if (book->bids) free(book->bids);
        if (book->asks) free(book->asks);
        free(book);
        return NULL;
    }
    
    // Initialize all fields
    book->bid_count = 0;
    book->ask_count = 0;
    book->is_initialized = false;
    book->last_update_id = 0;
    
    return book;
}

void free_order_book(OrderBook *book) {
    if (!book) return;
    
    if (book->bids) {
        free(book->bids);
        book->bids = NULL;
    }
    
    if (book->asks) {
        free(book->asks);
        book->asks = NULL;
    }
    
    if (book->prev_bids) {
        free(book->prev_bids);
        book->prev_bids = NULL;
    }
    
    if (book->prev_asks) {
        free(book->prev_asks);
        book->prev_asks = NULL;
    }
    
    if (book->prev_bids) {
        free(book->prev_bids);
        book->prev_bids = NULL;
    }
    
    if (book->prev_asks) {
        free(book->prev_asks);
        book->prev_asks = NULL;
    }
    
    free(book);
}

void reset_order_book(OrderBook *book) {
    if (!book) return;
    
    book->bid_count = 0;
    book->ask_count = 0;
    book->prev_bid_count = 0;
    book->prev_ask_count = 0;
    book->is_initialized = false;
    book->first_update_id = 0;
    book->last_update_id = 0;
    book->last_update_timestamp = 0;
}

// 内部函数：更新订单簿的一边（买单或卖单）
static void update_side(OrderBook *book, PriceLevel *levels, int *count, 
                       const double *prices, const double *quantities, 
                       int num_updates, int (*compare)(const void*, const void*)) {
    // Input validation
    if (!levels || !count || !prices || !quantities || num_updates <= 0) {
        printf("Error: Invalid parameters to update_side\n");
        return;
    }
    
    // Validate count is within bounds
    if (*count < 0 || *count > MAX_ORDER_BOOK_LEVELS) {
        printf("Error: Invalid count in update_side: %d\n", *count);
        *count = 0;  // Reset to prevent further corruption
        return;
    }
    
    // Validate price and quantity data
    for (int i = 0; i < num_updates; i++) {
        if (prices[i] <= 0.0 || quantities[i] < 0.0 || !isfinite(prices[i]) || !isfinite(quantities[i])) {
            printf("Error: Invalid price/quantity in update: price=%.8f, qty=%.8f\n", 
                   prices[i], quantities[i]);
            return;
        }
    }
    
    // Calculate new count after updates with bounds checking
    size_t new_count = (size_t)(*count) + (size_t)num_updates;
    if (new_count > (size_t)MAX_ORDER_BOOK_LEVELS) {
        printf("Warning: Exceeded maximum order book levels (%d), truncating updates\n", 
               MAX_ORDER_BOOK_LEVELS);
        new_count = (size_t)MAX_ORDER_BOOK_LEVELS;
    }
    
    // Allocate temporary array with bounds checking
    PriceLevel *temp = (PriceLevel*)calloc(new_count, sizeof(PriceLevel));
    if (!temp) {
        printf("Error: Failed to allocate memory for order book update\n");
        return;
    }
    
    size_t i = 0, j = 0, k = 0;
    const size_t max_levels = (size_t)MAX_ORDER_BOOK_LEVELS;
    
    // Merge updates with existing levels
    while (i < (size_t)(*count) && j < (size_t)num_updates && k < max_levels) {
        double price = prices[j];
        double qty = quantities[j];
        
        // Skip invalid prices
        if (price <= 0.0) {
            j++;
            continue;
        }
        
        // Compare current level with update
        int cmp = compare(&levels[i], &(PriceLevel){price, qty});
        
        if (cmp < 0) {
            // Keep existing price level if quantity > 0
            if (levels[i].quantity > 0.0) {
                if (k < max_levels) {
                    memcpy(&temp[k++], &levels[i], sizeof(PriceLevel));
                } else {
                    break;
                }
            }
            i++;
        } else if (cmp > 0) {
            // Add new price level if quantity > 0
            if (qty > 0.0) {
                if (k < max_levels) {
                    temp[k].price = price;
                    temp[k].quantity = qty;
                    k++;
                } else {
                    break;
                }
            }
            j++;
        } else {
            // Same price, update quantity if there's space
            if (k < max_levels) {
                temp[k].price = price;
                temp[k].quantity = qty;  // Replace quantity
                k++;
            } else {
                break;
            }
            i++;
            j++;
        }
    }
    
    // Process remaining existing levels
    while (i < (size_t)(*count) && k < max_levels) {
        if (levels[i].quantity > 0.0) {
            if (k < max_levels) {
                memcpy(&temp[k++], &levels[i], sizeof(PriceLevel));
            } else {
                break;
            }
        }
        i++;
    }
    
    // Process remaining updates
    while (j < (size_t)num_updates && k < max_levels) {
        double price = prices[j];
        double qty = quantities[j];
        
        if (price > 0.0 && qty > 0.0) {
            if (k < max_levels) {
                temp[k].price = price;
                temp[k].quantity = qty;
                k++;
            } else {
                break;
            }
        }
        j++;
    }
    
    // Ensure we don't exceed capacity
    size_t max_allowed = (size_t)MIN(book->capacity, MAX_ORDER_BOOK_LEVELS);
    if (k > max_allowed) {
        k = max_allowed;
    }
    
    // 清空原有数据
    memset(levels, 0, (*count) * sizeof(PriceLevel));
    
    // 更新订单簿
    if (k > 0) {
        // Sort the temporary array to ensure proper ordering
        qsort(temp, k, sizeof(PriceLevel), compare);
        
        // Remove any duplicate prices (keep the latest one)
        size_t write_idx = 0;
        for (size_t i = 1; i < k; i++) {
            if (!double_equal(temp[write_idx].price, temp[i].price)) {
                write_idx++;
                if (write_idx != i) {
                    memcpy(&temp[write_idx], &temp[i], sizeof(PriceLevel));
                }
            }
        }
        k = write_idx + 1;  // Update count after removing duplicates
        
        // Ensure we don't exceed capacity after removing duplicates
        if (k > max_allowed) {
            k = max_allowed;
        }
        
        // Copy the sorted and deduplicated data back to the levels array
        size_t bytes_to_copy = k * sizeof(PriceLevel);
        if (bytes_to_copy <= (size_t)book->capacity * sizeof(PriceLevel)) {
            memcpy(levels, temp, bytes_to_copy);
            *count = (int)k;
        } else {
            free(temp);
            return;
        }
    } else {
        *count = 0;
    }
    
    // 验证订单簿状态
    if (!validate_order_book(book)) {
        // 尝试修复交叉订单
        if (book->bid_count > 0 && book->ask_count > 0) {
            // 找到不交叉的买单和卖单分界点
            int valid_bid_count = 0;
            int valid_ask_count = 0;
            
            // 确保买单按价格降序排列
            qsort(book->bids, book->bid_count, sizeof(PriceLevel), compare_bids);
            
            // 确保卖单按价格升序排列
            qsort(book->asks, book->ask_count, sizeof(PriceLevel), compare_asks);
            
            // 移除交叉的订单
            while (valid_bid_count < book->bid_count && 
                   valid_ask_count < book->ask_count &&
                   book->bids[valid_bid_count].price >= book->asks[valid_ask_count].price) {
                // 移除价格较高的买单或价格较低的卖单
                if (valid_bid_count < book->bid_count - 1 && 
                    (valid_ask_count == book->ask_count - 1 || 
                     book->bids[valid_bid_count + 1].price > book->asks[valid_ask_count].price)) {
                    valid_bid_count++;
                } else {
                    valid_ask_count++;
                }
            }
            
            // 更新订单数量
            if (valid_bid_count > 0 || valid_ask_count > 0) {  
                // 移动有效订单到数组前面
                if (valid_bid_count > 0) {
                    memmove(book->bids, book->bids + valid_bid_count, 
                           (book->bid_count - valid_bid_count) * sizeof(PriceLevel));
                    book->bid_count -= valid_bid_count;
                }
                
                if (valid_ask_count > 0) {
                    memmove(book->asks, book->asks + valid_ask_count, 
                           (book->ask_count - valid_ask_count) * sizeof(PriceLevel));
                    book->ask_count -= valid_ask_count;
                }
                
                // 再次验证
                if (!validate_order_book(book)) {
                    *count = 0;
                    book->is_initialized = false;
                }
            } else {
                *count = 0;
                book->is_initialized = false;
            }
        } else {
            // 如果无法修复，重置订单簿
            printf("Cannot fix order book, resetting...\n");
            *count = 0;
            book->is_initialized = false;
        }
    }
    
    // 释放临时内存
    free(temp);
}

#include <float.h>  // For DBL_EPSILON

// Helper function to validate price and quantity arrays
static int validate_price_quantity_arrays(const double *prices, const double *quantities, int count) {
    if (count <= 0) return 1;  // Empty array is valid
    if (!prices || !quantities) return 0;
    
    for (int i = 0; i < count; i++) {
        if (prices[i] <= 0.0 || quantities[i] < 0.0 || !isfinite(prices[i]) || !isfinite(quantities[i])) {
            return 0;
        }
        // Check for duplicate prices (should be unique in order book)
        for (int j = i + 1; j < count; j++) {
            if (fabs(prices[i] - prices[j]) < DBL_EPSILON) {
                return 0;
            }
        }
    }
    return 1;
}

// 记录显著的数量变化
static void log_significant_changes(OrderBook *book, 
                                  const double *prices, 
                                  const double *quantities, 
                                  int count,
                                  bool is_bid) {
    if (!book || !prices || !quantities || count <= 0) {
        return;
    }
    
    const char *side = is_bid ? "BID" : "ASK";
    
    for (int i = 0; i < count; i++) {
        double price = prices[i];
        double new_qty = quantities[i];
        double old_qty = 0.0;
        
        // 查找该价格在订单簿中的当前数量
        if (is_bid) {
            for (int j = 0; j < book->bid_count; j++) {
                if (fabs(book->bids[j].price - price) < 0.00000001) {
                    old_qty = book->bids[j].quantity;
                    break;
                }
            }
        } else {
            for (int j = 0; j < book->ask_count; j++) {
                if (fabs(book->asks[j].price - price) < 0.00000001) {
                    old_qty = book->asks[j].quantity;
                    break;
                }
            }
        }
        
        // 计算数量变化
        double qty_change = new_qty - old_qty;
        
        // 如果数量变化超过5，记录日志
        if (fabs(qty_change) > MAX_QTY_CHANGES) {
            printf("[%s] 价格: %.8f, 数量变化: %+.8f (%.8f -> %.8f)\n",
                   side, price, qty_change, old_qty, new_qty);
        }
    }
}

void handle_depth_update(OrderBook *book, 
                        double *bid_prices, 
                        double *bid_quantities, 
                        int bid_count,
                        double *ask_prices, 
                        double *ask_quantities, 
                        int ask_count,
                        uint64_t first_update_id,
                        uint64_t final_update_id) {
    // Validate input parameters
    if (!book) {
        printf("Error: NULL order book\n");
        return;
    }
    
    // Validate update IDs
    if (final_update_id < first_update_id) {
        printf("Error: Invalid update ID range: first=%llu, final=%llu\n", 
              (unsigned long long)first_update_id, (unsigned long long)final_update_id);
        return;
    }
    
    // Validate price and quantity arrays
    if ((bid_count > 0 && !validate_price_quantity_arrays(bid_prices, bid_quantities, bid_count)) ||
        (ask_count > 0 && !validate_price_quantity_arrays(ask_prices, ask_quantities, ask_count))) {
        printf("Error: Invalid price/quantity data in update\n");
        return;
    }
    if (!book) {
        printf("Error: NULL order book\n");
        return;
    }
    
    // Validate input arrays
    if ((bid_count > 0 && (!bid_prices || !bid_quantities)) || 
        (ask_count > 0 && (!ask_prices || !ask_quantities))) {
        printf("Error: Invalid price or quantity arrays\n");
        return;
    }
    
    // Handle update ID synchronization
    if (!book->is_initialized) {
        // For the first update after snapshot
        if (final_update_id <= book->last_update_id) {
            printf("Skipping old update - last_update_id: %llu, final_update_id: %llu\n",
                  (unsigned long long)book->last_update_id,
                  (unsigned long long)final_update_id);
            return;
        }
        
        // If we missed a lot of updates, consider re-syncing the order book
        if (first_update_id > book->last_update_id + 1000) {
            printf("Warning: Large gap in updates - last_update_id: %llu, first_update_id: %llu. Consider re-syncing.\n",
                  (unsigned long long)book->last_update_id,
                  (unsigned long long)first_update_id);
        }
        
        printf("First update after snapshot - last_update_id: %llu, first_update_id: %llu, final_update_id: %llu\n",
              (unsigned long long)book->last_update_id,
              (unsigned long long)first_update_id,
              (unsigned long long)final_update_id);

        book->is_initialized = true;
    } 
    else if (first_update_id <= book->last_update_id) {
        // Skip old updates
        if (final_update_id > book->last_update_id) {
            // This update contains some new information
            printf("Processing partial update - last_update_id: %llu, first_update_id: %llu, final_update_id: %llu\n",
                  (unsigned long long)book->last_update_id,
                  (unsigned long long)first_update_id,
                  (unsigned long long)final_update_id);
        } else {
            printf("Skipping old update - last_update_id: %llu, first_update_id: %llu\n",
                  (unsigned long long)book->last_update_id,
                  (unsigned long long)first_update_id);
            return;
        }
    }
    
    // Update the last seen update ID
    book->last_update_id = final_update_id;
    
    // 更新时间戳
    book->last_update_timestamp = get_current_timestamp();
    
    // 记录显著变化（在更新前记录）
    if (bid_count > 0) {
        log_significant_changes(book, bid_prices, bid_quantities, bid_count, true);
        update_side(book, book->bids, &book->bid_count, 
                   bid_prices, bid_quantities, bid_count, compare_bids);
    }
    
    // 处理卖单（价格从低到高）
    if (ask_count > 0) {
        log_significant_changes(book, ask_prices, ask_quantities, ask_count, false);
        update_side(book, book->asks, &book->ask_count, 
                   ask_prices, ask_quantities, ask_count, compare_asks);
    }
    
    // Check for crossed order book and handle it
    if (book->bid_count > 0 && book->ask_count > 0) {
        double best_bid = book->bids[0].price;
        double best_ask = book->asks[0].price;
        
        if (best_bid >= best_ask) {
            // More aggressive fix: remove all crossed levels
            int valid_ask_index = 0;
            for (int i = 0; i < book->ask_count; i++) {
                if (book->asks[i].price > best_bid) {
                    valid_ask_index = i;
                    break;
                }
            }
            
            if (valid_ask_index > 0) {
                size_t new_ask_count = (size_t)book->ask_count - (size_t)valid_ask_index;
                printf("Removing %d crossed ask levels (prices <= %.8f), keeping %zu levels\n", 
                       valid_ask_index, best_bid, new_ask_count);
                
                // Move valid asks to the beginning
                if (valid_ask_index < book->ask_count) {
                    size_t bytes_to_move = new_ask_count * sizeof(PriceLevel);
                    if (bytes_to_move > 0 && bytes_to_move <= (size_t)book->capacity * sizeof(PriceLevel)) {
                        memmove(book->asks, 
                               &book->asks[valid_ask_index], 
                               bytes_to_move);
                    } else {
                        book->is_initialized = false;
                        book->ask_count = 0;
                        return;
                    }
                }
                book->ask_count = (int)new_ask_count;
                
                // Verify the fix
                if (book->ask_count > 0 && book->bids[0].price >= book->asks[0].price) {
                    // Consider resetting the order book if we can't fix it
                    if (book->bids[0].price >= book->asks[0].price) {
                        book->is_initialized = false;
                        book->bid_count = 0;
                        book->ask_count = 0;
                        return;
                    }
                }
            }
        }
    }
    
    // The order book updates and crossing checks are handled above
    
    // Log the best bid and ask prices with quantities after update
    if (book->bid_count > 0 && book->ask_count > 0) {
        double best_bid = book->bids[0].price;
        double best_bid_qty = book->bids[0].quantity;
        double best_ask = book->asks[0].price;
        double best_ask_qty = book->asks[0].quantity;
        // 将时间戳转换为可读格式
        time_t seconds = book->last_update_timestamp / 1000;
        int milliseconds = book->last_update_timestamp % 1000;
        struct tm *tm_info = localtime(&seconds);
        char time_str[24];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
            
        printf("[%s.%03d] OrderBook Update - Bid: %.8f x %.8f | Ask: %.8f x %.8f\n",
            time_str, milliseconds, best_bid, best_bid_qty, best_ask, best_ask_qty);
    }
}

double get_best_bid(const OrderBook *book) {
    if (!book || book->bid_count == 0) return 0.0;
    return book->bids[0].price;
}

double get_best_ask(const OrderBook *book) {
    if (!book || book->ask_count == 0) return 0.0;
    return book->asks[0].price;
}
