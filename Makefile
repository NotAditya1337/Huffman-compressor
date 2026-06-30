CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Iinclude
TARGET   = huffman
SRCS     = src/main.cpp src/Huffman.cpp
OBJS     = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

test: $(TARGET)
	./$(TARGET) -c examples/sample.txt examples/compressed.bin
	./$(TARGET) -d examples/compressed.bin examples/restored.txt

.PHONY: all clean test
