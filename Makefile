CXX       := g++
CXXFLAGS  := -std=c++17 -Wall -Wextra -g

SRC_DIR   := src
BUILD_DIR := build

# headers are included unqualified (e.g. #include "buffer.hpp"), so every
# source subdirectory goes on the include path
INCLUDES  := -I$(SRC_DIR)/shared -I$(SRC_DIR)/datastructures
# -MMD -MP emits a .d file per object so headers are tracked automatically
CPPFLAGS  := $(INCLUDES) -MMD -MP

SHARED_SRCS := $(SRC_DIR)/shared/shared.cpp
DS_SRCS     := $(SRC_DIR)/datastructures/buffer.cpp
HM_SRCS     := $(SRC_DIR)/datastructures/hashtable.cpp

SERVER_SRCS := $(SRC_DIR)/server.cpp $(SHARED_SRCS) $(DS_SRCS) $(HM_SRCS)
CLIENT_SRCS := $(SRC_DIR)/client.cpp $(SHARED_SRCS)

SERVER_OBJS := $(SERVER_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
CLIENT_OBJS := $(CLIENT_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

all: server client

# optimised build for benchmarking, e.g. `make clean release`
release: CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -DNDEBUG
release: all

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR) server client

-include $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)

.PHONY: all release clean
