SERVER_CXX = g++
CLIENT_CXX = aarch64-linux-gnu-g++
CXXFLAGS = -Wall -O2 -std=c++17

SERVER_DIR = Server
CLIENT_DIR = Client
COMMON_DIR = Common

SERVER_SRC = $(SERVER_DIR)/main.cpp
CLIENT_SRC = $(CLIENT_DIR)/main.cpp

SERVER_HDR = $(SERVER_DIR)/server.h
CLIENT_HDR = $(CLIENT_DIR)/client.h
COMMON_HDR = $(COMMON_DIR)/protocol.h

SERVER_BIN = server_app
CLIENT_BIN = client_app

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC) $(SERVER_HDR) $(COMMON_HDR)
	$(SERVER_CXX) $(CXXFLAGS) -o $@ $(SERVER_SRC) -lpthread

$(CLIENT_BIN): $(CLIENT_SRC) $(CLIENT_HDR) $(COMMON_HDR)
	$(CLIENT_CXX) $(CXXFLAGS) -o $@ $(CLIENT_SRC) -lpthread

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)

.PHONY: all clean