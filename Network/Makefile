SERVER_CXX = g++
CLIENT_CXX = aarch64-linux-gnu-g++
CXXFLAGS = -Wall -O2 -std=c++17

SERVER_DIR = Server
CLIENT_DIR = Client
COMMON_DIR = Common

GPIO_USER_DIR = ../gpio/user
GPIO_INCLUDE_DIR = ../gpio/include
GPIO_USER_SRC = $(GPIO_USER_DIR)/gpio_control.cpp
GPIO_USER_HDR = $(GPIO_USER_DIR)/gpio_control.h
GPIO_INC_HDR = $(GPIO_INCLUDE_DIR)/custom_ioctl.h

SERVER_SRC = $(wildcard $(SERVER_DIR)/*.cpp)
CLIENT_SRC = $(CLIENT_DIR)/main.cpp

SERVER_HDR = $(SERVER_DIR)/server.h
CLIENT_HDR = $(CLIENT_DIR)/client.h
COMMON_HDR = $(COMMON_DIR)/protocol.h

SERVER_BIN = server_app
CLIENT_BIN = client_app

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC) $(SERVER_HDR) $(COMMON_HDR) $(GPIO_USER_SRC) $(GPIO_USER_HDR) $(GPIO_INC_HDR)
	$(SERVER_CXX) $(CXXFLAGS) -I$(GPIO_USER_DIR) -I$(GPIO_INCLUDE_DIR) -o $@ $(SERVER_SRC) $(GPIO_USER_SRC) -lpthread


clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)

.PHONY: all clean