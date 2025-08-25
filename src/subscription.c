#include "subscription.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* build_subscribe_message(const char **streams, int count, int id) {
    if (!streams || count <= 0) {
        return NULL;
    }
    
    // Calculate buffer size
    size_t buffer_size = 1024;
    for (int i = 0; i < count; i++) {
        buffer_size += strlen(streams[i]) + 10;
    }
    
    char *message = (char *)malloc(buffer_size);
    if (!message) {
        return NULL;
    }
    
    // Start building JSON
    strcpy(message, "{\"method\":\"SUBSCRIBE\",\"params\":[");
    
    // Add streams
    for (int i = 0; i < count; i++) {
        strcat(message, "\"");
        strcat(message, streams[i]);
        strcat(message, "\"");
        if (i < count - 1) {
            strcat(message, ",");
        }
    }
    
    // Complete JSON
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "],\"id\":%d}", id);
    strcat(message, id_str);
    
    return message;
}

char* build_unsubscribe_message(const char **streams, int count, int id) {
    if (!streams || count <= 0) {
        return NULL;
    }
    
    // Calculate buffer size
    size_t buffer_size = 1024;
    for (int i = 0; i < count; i++) {
        buffer_size += strlen(streams[i]) + 10;
    }
    
    char *message = (char *)malloc(buffer_size);
    if (!message) {
        return NULL;
    }
    
    // Start building JSON
    strcpy(message, "{\"method\":\"UNSUBSCRIBE\",\"params\":[");
    
    // Add streams
    for (int i = 0; i < count; i++) {
        strcat(message, "\"");
        strcat(message, streams[i]);
        strcat(message, "\"");
        if (i < count - 1) {
            strcat(message, ",");
        }
    }
    
    // Complete JSON
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "],\"id\":%d}", id);
    strcat(message, id_str);
    
    return message;
}