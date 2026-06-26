#pragma once
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include "Ast.hpp"
#include "Value.hpp"
#include "BytecodeChunk.hpp"

// ============================================================================
// Binary serialization/deserialization for PyRite bytecode (.prc) and AST (.prt)
//
// .prc format: compiled bytecode chunk, directly loadable by VM::run()
// .prt format: serialized AST, directly loadable by Interpreter::interpret()
// ============================================================================

class BinWriter {
public:
    BinWriter(std::ostream& out) : out(out) {}
    void u8(uint8_t v);
    void u16(uint16_t v);
    void u32(uint32_t v);
    void i32(int32_t v);
    void raw(const char* data, size_t len);
    void string(const std::string& s);
    void bool8(bool v);
private:
    std::ostream& out;
};

class BinReader {
public:
    BinReader(std::istream& in) : in(in) {}
    uint8_t u8();
    uint16_t u16();
    uint32_t u32();
    int32_t i32();
    std::string string();
    bool bool8();
    void raw(char* data, size_t len);
    bool eof() { return in.eof(); }
    bool good() { return in.good(); }
private:
    std::istream& in;
};

// ============================================================================
// Value serialization
// ============================================================================
void write_value(BinWriter& w, ValuePtr v);
ValuePtr read_value(BinReader& r);

// ============================================================================
// AST serialization
// ============================================================================
void write_ast_node(BinWriter& w, AstNodePtr node);
AstNodePtr read_ast_node(BinReader& r);

void write_ast(BinWriter& w, const std::vector<AstNodePtr>& nodes);
std::vector<AstNodePtr> read_ast(BinReader& r);

void write_param_def(BinWriter& w, const ParameterDefinition& pd);
ParameterDefinition read_param_def(BinReader& r);

// ============================================================================
// Bytecode chunk serialization (.prc)
// ============================================================================
void write_compiled_function(BinWriter& w, CompiledFunctionPtr fn);
CompiledFunctionPtr read_compiled_function(BinReader& r);

void write_bytecode_chunk(BinWriter& w, BytecodeChunkPtr chunk);
BytecodeChunkPtr read_bytecode_chunk(BinReader& r);

// ============================================================================
// High-level file I/O
// ============================================================================
void save_bytecode(const std::string& path, BytecodeChunkPtr chunk);
BytecodeChunkPtr load_bytecode(const std::string& path);

void save_ast(const std::string& path, const std::vector<AstNodePtr>& nodes);
std::vector<AstNodePtr> load_ast(const std::string& path);
