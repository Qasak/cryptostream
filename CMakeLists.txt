cmake_minimum_required(VERSION 3.10)
project(cryptostream C)
set(PROJECT_VERSION "1.0.0")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Find libwebsockets
pkg_check_modules(LWS REQUIRED libwebsockets)

# Find json-c
pkg_check_modules(JSONC REQUIRED json-c)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LWS_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Source files
set(SOURCES
    src/main.c
    src/ws_client.c
    src/json_parser.c
    src/subscription.c
    src/orderbook.c
    src/binance_rest.c
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Set link directories before linking
target_link_directories(${PROJECT_NAME} PRIVATE
    ${LWS_LIBRARY_DIRS}
    ${JSONC_LIBRARY_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    websockets
    json-c
    ${OPENSSL_LIBRARIES}
    pthread
    curl
)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    ${LWS_CFLAGS_OTHER}
    ${JSONC_CFLAGS_OTHER}
    -Wall
    -Wextra
    -O2
)