CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g

all: server client

server: server.cpp shared.cpp shared.h
	$(CXX) $(CXXFLAGS) -o $@ server.cpp shared.cpp

client: client.cpp shared.cpp shared.h
	$(CXX) $(CXXFLAGS) -o $@ client.cpp shared.cpp

clean:
	rm -f server client

.PHONY: all clean
