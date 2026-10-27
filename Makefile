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

.PHONY: all clean release debug check check-opcodes

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

# Verify that the generated opcode tables are in sync with opcodes.def.
# The bytecode contract lives in opcodes.def; src/OpCode.hpp and the
# JavaScript VM's opcodes.js are generated from it. Run 'make
# check-opcodes' in CI to catch a half-applied change.
check: check-opcodes

check-opcodes:
	python3 tools/gen_opcodes.py --check
