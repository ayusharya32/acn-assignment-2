CXX = g++
CXXFLAGS = -std=c++17 -pthread -Iinclude

SERVER = build/server
CLIENT = build/client

COMMON = src/config.cpp \
         src/util.cpp \
         src/request.cpp \
         src/framing.cpp \
         src/response.cpp \
         src/cli.cpp \
		 src/scheduler.cpp \
		 src/request_handler.cpp

all: $(SERVER) $(CLIENT)

$(SERVER): src/server.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) src/server.cpp $(COMMON) -o $(SERVER)

$(CLIENT): src/client.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) src/client.cpp $(COMMON) -o $(CLIENT)

clean:
	rm -f $(SERVER) $(CLIENT)
