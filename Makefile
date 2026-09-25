CXX = g++
CXXFLAGS = -std=c++17 -pthread -Iinclude

SERVER = build/server
CLIENT = build/client

COMMON = src/config.cpp \
         src/util.cpp \
         src/request.cpp \
         src/framing.cpp \
         src/response.cpp \
         src/cli.cpp

all: $(SERVER) $(CLIENT)

$(SERVER): src/server.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) src/server.cpp $(COMMON) -o $(SERVER)

$(CLIENT): src/client.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) src/client.cpp $(COMMON) -o $(CLIENT)

clean:
	rm -f $(SERVER) $(CLIENT)

test_parser: tests/test_parser.cpp src/request.cpp src/util.cpp
	$(CXX) $(CXXFLAGS) tests/test_parser.cpp src/request.cpp src/util.cpp -o build/test_parser

test_framing: tests/test_framing.cpp src/framing.cpp
	$(CXX) $(CXXFLAGS) tests/test_framing.cpp src/framing.cpp -o build/test_framing

test_response: tests/test_response.cpp src/response.cpp
	$(CXX) $(CXXFLAGS) tests/test_response.cpp src/response.cpp -o build/test_response
	
test_cli: tests/test_cli.cpp src/cli.cpp src/util.cpp
	$(CXX) $(CXXFLAGS) tests/test_cli.cpp src/cli.cpp src/util.cpp -o build/test_cli

test: test_parser test_framing test_response
	./build/test_parser
	./build/test_framing
	./build/test_response