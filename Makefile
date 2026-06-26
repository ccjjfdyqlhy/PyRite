CXX      := g++
CXXFLAGS := -std=c++11 -O2
LDFLAGS  :=
TARGET   := PyRite

SRC_DIR := src
SRCS    := $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/Value.cpp \
           $(SRC_DIR)/Tokenizer.cpp \
           $(SRC_DIR)/Parser.cpp \
           $(SRC_DIR)/Compiler.cpp \
           $(SRC_DIR)/Environment.cpp \
           $(SRC_DIR)/Interpreter.cpp \
           $(SRC_DIR)/VM.cpp \
           $(SRC_DIR)/Serializer.cpp
OBJS    := $(SRCS:.cpp=.o)

.PHONY: all clean release debug

all: release

release: CXXFLAGS += -O2 -DNDEBUG
release: $(TARGET)

debug: CXXFLAGS += -O0 -g -DDEBUG
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
