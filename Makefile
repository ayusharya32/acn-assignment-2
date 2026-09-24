CXX = g++
CXXFLAGS = -std=c++17 -pthread

SERVER = server
CLIENT = client

COMMON = config.cpp util.cpp request.cpp

all: $(SERVER) $(CLIENT)

$(SERVER): server.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) server.cpp $(COMMON) -o $(SERVER)

$(CLIENT): client.cpp $(COMMON)
	$(CXX) $(CXXFLAGS) client.cpp $(COMMON) -o $(CLIENT)

clean:
	rm -f $(SERVER) $(CLIENT)

test_parser: test_parser.cpp request.cpp util.cpp
	$(CXX) $(CXXFLAGS) test_parser.cpp request.cpp util.cpp -o test_parser

test: test_parser
	./test_parser