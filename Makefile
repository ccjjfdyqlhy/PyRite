CXX      := g++
CXXFLAGS := -std=c++11 -O2 -MMD -MP
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
           $(SRC_DIR)/Serializer.cpp \
           $(SRC_DIR)/JsBytecodeSerializer.cpp \
           $(SRC_DIR)/MemPool.cpp
OBJS    := $(SRCS:.cpp=.o)
DEPS    := $(SRCS:.cpp=.d)

-include $(DEPS)

.PHONY: all clean release debug check check-opcodes

all: release

release: CXXFLAGS += -O3 -DNDEBUG
release: $(TARGET)

# Profile-guided build: run the workload once, rebuild with the profile.
pgo: CXXFLAGS += -O3 -DNDEBUG
pgo:
	$(CXX) $(CXXFLAGS) -fprofile-generate -o $(TARGET) $(SRCS)
	./$(TARGET) bench/pyrite/int_loop.pr > /dev/null
	./$(TARGET) bench/pyrite/fib.pr > /dev/null
	./$(TARGET) bench/pyrite/dict_ops.pr > /dev/null
	$(CXX) $(CXXFLAGS) -fprofile-use -fprofile-correction -o $(TARGET) $(SRCS)

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
