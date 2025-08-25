# CryptoStream 🚀

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)](https://github.com)

A high-performance WebSocket client for real-time cryptocurrency market data streaming from Binance Futures, written in pure C.

[English](#english) | [中文](#中文)

## English

### ✨ Features

- 🔐 **Secure WebSocket Connection** - SSL/TLS encrypted connection to Binance Futures
- 📊 **Multiple Data Streams** - Support for trades, klines, depth, ticker and more
- 🌐 **Proxy Support** - HTTP/SOCKS proxy with authentication
- 📦 **Zero Dependencies** - Minimal external dependencies, easy to compile
- ⚡ **High Performance** - Written in C for maximum efficiency
- 🔄 **Auto Reconnection** - Automatic reconnection on connection loss
- 🎯 **Real-time Parsing** - JSON parsing and formatted output

### 📋 Requirements

- libwebsockets (WebSocket communication)
- json-c (JSON parsing)
- OpenSSL (SSL/TLS encryption)
- CMake (Build system)

#### Install on macOS
```bash
brew install libwebsockets json-c openssl cmake
```

#### Install on Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libwebsockets-dev libjson-c-dev libssl-dev cmake build-essential
```

### 🚀 Quick Start

```bash
# Clone the repository
git clone https://github.com/yourusername/cryptostream.git
cd cryptostream

# Build
mkdir build && cd build
cmake ..
make

# Run without proxy
./cryptostream

# Run with proxy
./cryptostream -p                        # Default proxy 127.0.0.1:7890
./cryptostream -p -a 192.168.1.100 -P 8080  # Custom proxy
```

### 🔧 Configuration

#### Command Line Options

```bash
./cryptostream [OPTIONS]

Options:
  -h, --help                Show help message
  -p, --proxy               Enable proxy connection
  -a, --proxy-addr ADDRESS  Proxy server address (default: 127.0.0.1)
  -P, --proxy-port PORT     Proxy server port (default: 7890)
  -u, --proxy-user USERNAME Proxy username (optional)
  -w, --proxy-pass PASSWORD Proxy password (optional)
  -c, --config FILE         Load configuration from file
```

#### Configuration File

Create `config.txt`:

```ini
# Proxy Settings
use_proxy=true
proxy_address=127.0.0.1
proxy_port=7890
# proxy_username=user
# proxy_password=pass
```

### 📡 Supported Data Streams

- `@aggTrade` - Aggregate trade streams
- `@markPrice` - Mark price streams
- `@kline_<interval>` - Kline/candlestick streams
- `@ticker` - 24hr ticker streams
- `@bookTicker` - Best bid/ask streams
- `@depth<levels>` - Order book depth streams

### 🛠️ Development

#### Project Structure
```
cryptostream/
├── CMakeLists.txt      # Build configuration
├── README.md           # Documentation
├── config.txt          # Configuration file
├── include/            # Header files
│   ├── ws_client.h     # WebSocket client
│   ├── json_parser.h   # JSON parser
│   └── subscription.h  # Subscription management
└── src/                # Source files
    ├── main.c          # Main entry point
    ├── ws_client.c     # WebSocket implementation
    ├── json_parser.c   # JSON parsing
    └── subscription.c  # Subscription logic
```

---

## 中文

### ✨ 功能特性

- 🔐 **安全的WebSocket连接** - SSL/TLS加密连接到币安期货
- 📊 **多种数据流** - 支持交易、K线、深度、行情等多种数据
- 🌐 **代理支持** - 支持HTTP/SOCKS代理及认证
- 📦 **零依赖** - 最小化外部依赖，编译简单
- ⚡ **高性能** - 纯C语言编写，性能卓越
- 🔄 **自动重连** - 连接断开自动重连
- 🎯 **实时解析** - JSON数据实时解析和格式化输出

### 📋 环境要求

- libwebsockets (WebSocket通信库)
- json-c (JSON解析库)
- OpenSSL (SSL/TLS加密)
- CMake (构建工具)

#### macOS 安装依赖
```bash
brew install libwebsockets json-c openssl cmake
```

#### Ubuntu/Debian 安装依赖
```bash
sudo apt-get update
sudo apt-get install libwebsockets-dev libjson-c-dev libssl-dev cmake build-essential
```

### 🚀 快速开始

```bash
# 克隆仓库
git clone https://github.com/qasak/cryptostream.git
cd cryptostream

# 编译
mkdir build && cd build
cmake ..
make

# 直接运行（不使用代理）
./cryptostream

# 使用代理运行
./cryptostream -p                        # 默认代理 127.0.0.1:7890
./cryptostream -p -a 192.168.1.100 -P 8080  # 自定义代理
```

### 🔧 配置说明

#### 命令行参数

```bash
./cryptostream [选项]

选项：
  -h, --help                显示帮助信息
  -p, --proxy               启用代理连接
  -a, --proxy-addr ADDRESS  代理服务器地址（默认：127.0.0.1）
  -P, --proxy-port PORT     代理服务器端口（默认：7890）
  -u, --proxy-user USERNAME 代理用户名（可选）
  -w, --proxy-pass PASSWORD 代理密码（可选）
  -c, --config FILE         从配置文件加载设置
```

#### 配置文件

创建 `config.txt`:

```ini
# 代理设置
use_proxy=true
proxy_address=127.0.0.1
proxy_port=7890
# proxy_username=user
# proxy_password=pass
```

### 📡 支持的数据流

- `@aggTrade` - 归集交易流
- `@markPrice` - 标记价格流
- `@kline_<interval>` - K线数据流
- `@ticker` - 24小时ticker数据流
- `@bookTicker` - 最优挂单信息
- `@depth<levels>` - 订单簿深度数据

### 🛠️ 开发

#### 项目结构
```
cryptostream/
├── CMakeLists.txt      # 构建配置
├── README.md           # 项目文档
├── config.txt          # 配置文件
├── include/            # 头文件
│   ├── ws_client.h     # WebSocket客户端
│   ├── json_parser.h   # JSON解析器
│   └── subscription.h  # 订阅管理
└── src/                # 源代码
    ├── main.c          # 主程序入口
    ├── ws_client.c     # WebSocket实现
    ├── json_parser.c   # JSON解析
    └── subscription.c  # 订阅逻辑
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

