#include "Serializer.hpp"
#include <stdexcept>

// ============================================================================
// Binary I/O helpers
// ============================================================================
void BinWriter::u8(uint8_t v) { out.put(static_cast<char>(v)); }
void BinWriter::u16(uint16_t v) {
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>(v & 0xFF));
}
void BinWriter::u32(uint32_t v) {
    out.put(static_cast<char>((v >> 24) & 0xFF));
    out.put(static_cast<char>((v >> 16) & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>(v & 0xFF));
}
void BinWriter::i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
void BinWriter::raw(const char* data, size_t len) { out.write(data, static_cast<std::streamsize>(len)); }
void BinWriter::string(const std::string& s) {
    u32(static_cast<uint32_t>(s.size()));
    raw(s.data(), s.size());
}
void BinWriter::bool8(bool v) { u8(v ? 1 : 0); }

uint8_t BinReader::u8() {
    char c;
    if (!in.get(c)) throw std::runtime_error("Unexpected end of data");
    return static_cast<uint8_t>(c);
}
uint16_t BinReader::u16() {
    uint8_t hi = u8(), lo = u8();
    return (static_cast<uint16_t>(hi) << 8) | lo;
}
uint32_t BinReader::u32() {
    uint8_t b0 = u8(), b1 = u8(), b2 = u8(), b3 = u8();
    return (static_cast<uint32_t>(b0) << 24) |
           (static_cast<uint32_t>(b1) << 16) |
           (static_cast<uint32_t>(b2) << 8)  |
           static_cast<uint32_t>(b3);
}
int32_t BinReader::i32() { return static_cast<int32_t>(u32()); }
std::string BinReader::string() {
    uint32_t len = u32();
    std::string s;
    s.resize(len);
    if (len > 0) raw(&s[0], len);
    return s;
}
bool BinReader::bool8() { return u8() != 0; }
void BinReader::raw(char* data, size_t len) {
    in.read(data, static_cast<std::streamsize>(len));
    if (in.gcount() != static_cast<std::streamsize>(len))
        throw std::runtime_error("Unexpected end of data");
}

// ============================================================================
// AST node type tags
// ============================================================================
enum class AstTag : uint16_t {
    NULL_NODE = 0,
    LITERAL = 1,
    LIST_LITERAL = 2,
    DIM_LITERAL = 3,
    VARIABLE = 4,
    UNARY_OP = 5,
    BINARY_OP = 6,
    LOGICAL_OP = 7,
    TYPE_CONVERSION = 8,
    ASSIGNMENT = 9,
    VAR_DECLARATION = 10,
    USING = 11,
    IF_STATEMENT = 12,
    WHILE_STATEMENT = 13,
    LOOP_FOR = 14,
    FOR_IN = 15,
    LOOP_UNTIL = 16,
    BREAK = 17,
    CONTINUE = 18,
    AWAIT_STATEMENT = 19,
    SAY = 20,
    INP = 21,
    FN_DEF = 22,
    CALL = 23,
    SUBSCRIPT = 24,
    RETURN = 25,
    RAISE = 26,
    TRY_CATCH = 27,
    CLASS_DEF = 28,
    GET = 29,
    SET = 30,
    LAMBDA = 31,
    STRUCT_DEF = 32,
    SWAP = 33,
    REQUIRE = 34,
    EXPRESSION_STATEMENT = 35,
};

// ============================================================================
// Value serialization tags
// ============================================================================
enum class ValueTag : uint8_t {
    NULL_VAL = 0,
    NUMBER = 1,
    STRING = 2,
    BINARY = 3,
    LN = 4,
    DIM = 5,
};

// ============================================================================
// Token serialization
// ============================================================================
static void write_token(BinWriter& w, const Token& tk) {
    w.u32(static_cast<uint32_t>(tk.type));
    w.string(tk.lexeme);
    w.i32(tk.line);
}

static Token read_token(BinReader& r) {
    Token tk;
    tk.type = static_cast<TokenType>(r.u32());
    tk.lexeme = r.string();
    tk.line = r.i32();
    return tk;
}

// ============================================================================
// ParameterDefinition serialization
// ============================================================================
void write_param_def(BinWriter& w, const ParameterDefinition& pd) {
    w.u32(static_cast<uint32_t>(pd.type_keyword));
    w.string(pd.name);
    w.bool8(pd.has_default);
    if (pd.has_default) {
        write_value(w, pd.default_value);
        write_ast_node(w, pd.default_expr);
    }
}

ParameterDefinition read_param_def(BinReader& r) {
    TokenType tk = static_cast<TokenType>(r.u32());
    std::string name = r.string();
    bool has_default = r.bool8();
    if (!has_default) {
        return ParameterDefinition(tk, name, ValuePtr(nullptr));
    }
    ValuePtr dv = read_value(r);
    AstNodePtr de = read_ast_node(r);
    if (dv) {
        ParameterDefinition pd(tk, name, dv);
        pd.default_expr = de;
        return pd;
    }
    // default_expr gives us the expression default
    return ParameterDefinition(tk, name, de);
}

// ============================================================================
// Value serialization
// ============================================================================
void write_value(BinWriter& w, ValuePtr v) {
    if (!v || dynamic_cast<NullValue*>(v.get())) {
        w.u8(static_cast<uint8_t>(ValueTag::NULL_VAL));
    } else if (auto nv = dynamic_cast<NumberValue*>(v.get())) {
        w.u8(static_cast<uint8_t>(ValueTag::NUMBER));
        w.string(nv->value.toString());
    } else if (auto sv = dynamic_cast<StringValue*>(v.get())) {
        w.u8(static_cast<uint8_t>(ValueTag::STRING));
        w.string(sv->value);
    } else if (auto bv = dynamic_cast<BinaryValue*>(v.get())) {
        w.u8(static_cast<uint8_t>(ValueTag::BINARY));
        w.u32(static_cast<uint32_t>(bv->value.size()));
        if (!bv->value.empty())
            w.raw(reinterpret_cast<const char*>(bv->value.data()), bv->value.size());
    } else if (auto lv = dynamic_cast<LnValue*>(v.get())) {
        w.u8(static_cast<uint8_t>(ValueTag::LN));
        w.u32(static_cast<uint32_t>(lv->elements.size()));
        for (const auto& e : lv->elements) write_value(w, e);
    } else if (auto dv = dynamic_cast<DimValue*>(v.get())) {
        w.u8(static_cast<uint8_t>(ValueTag::DIM));
        w.u32(static_cast<uint32_t>(dv->dict.size()));
        for (const auto& kv : dv->dict) {
            w.string(kv.first);
            write_value(w, kv.second);
        }
    } else {
        w.u8(static_cast<uint8_t>(ValueTag::NULL_VAL));
    }
}

ValuePtr read_value(BinReader& r) {
    ValueTag tag = static_cast<ValueTag>(r.u8());
    switch (tag) {
        case ValueTag::NULL_VAL:
            return nullptr;
        case ValueTag::NUMBER:
            return std::make_shared<NumberValue>(BigNumber(r.string()));
        case ValueTag::STRING:
            return std::make_shared<StringValue>(r.string());
        case ValueTag::BINARY: {
            uint32_t len = r.u32();
            std::vector<uint8_t> data(len);
            if (len > 0) r.raw(reinterpret_cast<char*>(data.data()), len);
            return std::make_shared<BinaryValue>(data);
        }
        case ValueTag::LN: {
            uint32_t n = r.u32();
            std::vector<ValuePtr> elems;
            elems.reserve(n);
            for (uint32_t i = 0; i < n; ++i) elems.push_back(read_value(r));
            return std::make_shared<LnValue>(elems);
        }
        case ValueTag::DIM: {
            uint32_t n = r.u32();
            std::map<std::string, ValuePtr> dict;
            for (uint32_t i = 0; i < n; ++i) {
                std::string key = r.string();
                dict[key] = read_value(r);
            }
            return std::make_shared<DimValue>(dict);
        }
    }
    return nullptr;
}

// ============================================================================
// AST node serialization
// ============================================================================
void write_ast_node(BinWriter& w, AstNodePtr node) {
    if (!node) {
        w.u16(static_cast<uint16_t>(AstTag::NULL_NODE));
        return;
    }
    if (auto n = dynamic_cast<LiteralNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::LITERAL));
        w.i32(n->line);
        write_value(w, n->value);
    } else if (auto n = dynamic_cast<ListLiteralNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::LIST_LITERAL));
        w.i32(n->line);
        w.u32(static_cast<uint32_t>(n->elements.size()));
        for (const auto& e : n->elements) write_ast_node(w, e);
    } else if (auto n = dynamic_cast<DimLiteralNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::DIM_LITERAL));
        w.i32(n->line);
        w.u32(static_cast<uint32_t>(n->entries.size()));
        for (const auto& entry : n->entries) {
            write_ast_node(w, entry.first);
            write_ast_node(w, entry.second);
        }
    } else if (auto n = dynamic_cast<VariableNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::VARIABLE));
        w.i32(n->line);
        w.string(n->name);
    } else if (auto n = dynamic_cast<UnaryOpNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::UNARY_OP));
        w.i32(n->line);
        write_token(w, n->op);
        write_ast_node(w, n->right);
    } else if (auto n = dynamic_cast<BinaryOpNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::BINARY_OP));
        w.i32(n->line);
        write_ast_node(w, n->left);
        write_token(w, n->op);
        write_ast_node(w, n->right);
    } else if (auto n = dynamic_cast<LogicalOpNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::LOGICAL_OP));
        w.i32(n->line);
        write_ast_node(w, n->left);
        write_token(w, n->op);
        write_ast_node(w, n->right);
    } else if (auto n = dynamic_cast<TypeConversionNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::TYPE_CONVERSION));
        w.i32(n->line);
        write_ast_node(w, n->expression);
        write_token(w, n->type_keyword);
    } else if (auto n = dynamic_cast<AssignmentNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::ASSIGNMENT));
        w.i32(n->line);
        write_ast_node(w, n->target);
        write_ast_node(w, n->value);
    } else if (auto n = dynamic_cast<VarDeclarationNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::VAR_DECLARATION));
        w.i32(n->line);
        write_token(w, n->keyword);
        w.string(n->name);
        write_ast_node(w, n->initializer);
        w.bool8(n->is_exposed);
    } else if (auto n = dynamic_cast<UsingNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::USING));
        w.i32(n->line);
        w.string(n->original_name);
        w.string(n->alias_name);
    } else if (auto n = dynamic_cast<IfStatementNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::IF_STATEMENT));
        w.i32(n->line);
        write_ast_node(w, n->condition);
        w.u32(static_cast<uint32_t>(n->then_branch.size()));
        for (const auto& s : n->then_branch) write_ast_node(w, s);
        w.u32(static_cast<uint32_t>(n->else_branch.size()));
        for (const auto& s : n->else_branch) write_ast_node(w, s);
    } else if (auto n = dynamic_cast<WhileStatementNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::WHILE_STATEMENT));
        w.i32(n->line);
        write_ast_node(w, n->condition);
        w.u32(static_cast<uint32_t>(n->do_branch.size()));
        for (const auto& s : n->do_branch) write_ast_node(w, s);
        w.u32(static_cast<uint32_t>(n->finally_branch.size()));
        for (const auto& s : n->finally_branch) write_ast_node(w, s);
    } else if (auto n = dynamic_cast<LoopForNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::LOOP_FOR));
        w.i32(n->line);
        w.string(n->index_var_name);
        w.u32(static_cast<uint32_t>(n->body.size()));
        for (const auto& s : n->body) write_ast_node(w, s);
        write_ast_node(w, n->count_expr);
    } else if (auto n = dynamic_cast<ForInNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::FOR_IN));
        w.i32(n->line);
        w.string(n->var_name);
        write_ast_node(w, n->iterable);
        w.u32(static_cast<uint32_t>(n->body.size()));
        for (const auto& s : n->body) write_ast_node(w, s);
    } else if (auto n = dynamic_cast<LoopUntilNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::LOOP_UNTIL));
        w.i32(n->line);
        w.string(n->index_var_name);
        w.u32(static_cast<uint32_t>(n->body.size()));
        for (const auto& s : n->body) write_ast_node(w, s);
        write_ast_node(w, n->condition);
    } else if (auto n = dynamic_cast<BreakNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::BREAK));
        w.i32(n->line);
    } else if (auto n = dynamic_cast<ContinueNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::CONTINUE));
        w.i32(n->line);
    } else if (auto n = dynamic_cast<AwaitStatementNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::AWAIT_STATEMENT));
        w.i32(n->line);
        write_ast_node(w, n->condition);
        w.u32(static_cast<uint32_t>(n->then_branch.size()));
        for (const auto& s : n->then_branch) write_ast_node(w, s);
    } else if (auto n = dynamic_cast<SayNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::SAY));
        w.i32(n->line);
        write_ast_node(w, n->expression);
    } else if (auto n = dynamic_cast<InpNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::INP));
        w.i32(n->line);
        write_ast_node(w, n->expression);
    } else if (auto n = dynamic_cast<FnDefNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::FN_DEF));
        w.i32(n->line);
        w.string(n->name);
        w.u32(static_cast<uint32_t>(n->params.size()));
        for (const auto& p : n->params) write_param_def(w, p);
        w.u32(static_cast<uint32_t>(n->body.size()));
        for (const auto& s : n->body) write_ast_node(w, s);
        w.bool8(n->is_method);
        w.bool8(n->is_exposed);
    } else if (auto n = dynamic_cast<CallNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::CALL));
        w.i32(n->line);
        write_ast_node(w, n->callee);
        w.u32(static_cast<uint32_t>(n->arguments.size()));
        for (const auto& a : n->arguments) write_ast_node(w, a);
    } else if (auto n = dynamic_cast<SubscriptNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::SUBSCRIPT));
        w.i32(n->line);
        write_ast_node(w, n->object);
        write_ast_node(w, n->start);
        write_ast_node(w, n->end);
        write_ast_node(w, n->step);
        w.bool8(n->is_slice);
    } else if (auto n = dynamic_cast<ReturnNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::RETURN));
        w.i32(n->line);
        write_ast_node(w, n->value);
    } else if (auto n = dynamic_cast<RaiseNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::RAISE));
        w.i32(n->line);
        write_ast_node(w, n->expression);
    } else if (auto n = dynamic_cast<TryCatchNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::TRY_CATCH));
        w.i32(n->line);
        w.u32(static_cast<uint32_t>(n->try_branch.size()));
        for (const auto& s : n->try_branch) write_ast_node(w, s);
        w.string(n->exception_var);
        w.u32(static_cast<uint32_t>(n->catch_branch.size()));
        for (const auto& s : n->catch_branch) write_ast_node(w, s);
        w.u32(static_cast<uint32_t>(n->finally_branch.size()));
        for (const auto& s : n->finally_branch) write_ast_node(w, s);
    } else if (auto n = dynamic_cast<ClassDefNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::CLASS_DEF));
        w.i32(n->line);
        w.string(n->name);
        w.u32(static_cast<uint32_t>(n->fields.size()));
        for (const auto& f : n->fields) write_param_def(w, f);
        w.u32(static_cast<uint32_t>(n->initializer_body.size()));
        for (const auto& s : n->initializer_body) write_ast_node(w, s);
        w.u32(static_cast<uint32_t>(n->methods.size()));
        for (const auto& s : n->methods) write_ast_node(w, s);
    } else if (auto n = dynamic_cast<GetNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::GET));
        w.i32(n->line);
        write_ast_node(w, n->object);
        w.string(n->name);
    } else if (auto n = dynamic_cast<SetNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::SET));
        w.i32(n->line);
        write_ast_node(w, n->object);
        w.string(n->name);
        write_ast_node(w, n->value);
    } else if (auto n = dynamic_cast<LambdaNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::LAMBDA));
        w.i32(n->line);
        w.u32(static_cast<uint32_t>(n->params.size()));
        for (const auto& p : n->params) write_param_def(w, p);
        w.u32(static_cast<uint32_t>(n->body.size()));
        for (const auto& s : n->body) write_ast_node(w, s);
    } else if (auto n = dynamic_cast<StructDefNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::STRUCT_DEF));
        w.i32(n->line);
        w.string(n->name);
        w.u32(static_cast<uint32_t>(n->fields.size()));
        for (const auto& f : n->fields) write_param_def(w, f);
    } else if (auto n = dynamic_cast<SwapNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::SWAP));
        w.i32(n->line);
        write_ast_node(w, n->left);
        write_ast_node(w, n->right);
    } else if (auto n = dynamic_cast<RequireNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::REQUIRE));
        w.i32(n->line);
        w.string(n->module_path);
        w.string(n->alias_name);
    } else if (auto n = dynamic_cast<ExpressionStatementNode*>(node.get())) {
        w.u16(static_cast<uint16_t>(AstTag::EXPRESSION_STATEMENT));
        w.i32(n->line);
        write_ast_node(w, n->expression);
    } else {
        w.u16(static_cast<uint16_t>(AstTag::NULL_NODE));
    }
}

AstNodePtr read_ast_node(BinReader& r) {
    AstTag tag = static_cast<AstTag>(r.u16());
    switch (tag) {
        case AstTag::NULL_NODE:
            return nullptr;
        case AstTag::LITERAL: {
            int line = r.i32();
            return std::make_shared<LiteralNode>(line, read_value(r));
        }
        case AstTag::LIST_LITERAL: {
            int line = r.i32();
            uint32_t n = r.u32();
            std::vector<AstNodePtr> elems;
            elems.reserve(n);
            for (uint32_t i = 0; i < n; ++i) elems.push_back(read_ast_node(r));
            return std::make_shared<ListLiteralNode>(line, elems);
        }
        case AstTag::DIM_LITERAL: {
            int line = r.i32();
            uint32_t n = r.u32();
            std::vector<std::pair<AstNodePtr, AstNodePtr>> entries;
            entries.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                AstNodePtr k = read_ast_node(r);
                AstNodePtr v = read_ast_node(r);
                entries.push_back({k, v});
            }
            return std::make_shared<DimLiteralNode>(line, entries);
        }
        case AstTag::VARIABLE: {
            int line = r.i32();
            return std::make_shared<VariableNode>(line, r.string());
        }
        case AstTag::UNARY_OP: {
            int line = r.i32();
            Token op = read_token(r);
            AstNodePtr right = read_ast_node(r);
            return std::make_shared<UnaryOpNode>(line, op, right);
        }
        case AstTag::BINARY_OP: {
            int line = r.i32();
            AstNodePtr left = read_ast_node(r);
            Token op = read_token(r);
            AstNodePtr right = read_ast_node(r);
            return std::make_shared<BinaryOpNode>(line, left, op, right);
        }
        case AstTag::LOGICAL_OP: {
            int line = r.i32();
            AstNodePtr left = read_ast_node(r);
            Token op = read_token(r);
            AstNodePtr right = read_ast_node(r);
            return std::make_shared<LogicalOpNode>(line, left, op, right);
        }
        case AstTag::TYPE_CONVERSION: {
            int line = r.i32();
            AstNodePtr expr = read_ast_node(r);
            Token tk = read_token(r);
            return std::make_shared<TypeConversionNode>(line, expr, tk);
        }
        case AstTag::ASSIGNMENT: {
            int line = r.i32();
            AstNodePtr target = read_ast_node(r);
            AstNodePtr value = read_ast_node(r);
            return std::make_shared<AssignmentNode>(line, target, value);
        }
        case AstTag::VAR_DECLARATION: {
            int line = r.i32();
            Token kw = read_token(r);
            std::string name = r.string();
            AstNodePtr init = read_ast_node(r);
            bool exposed = r.bool8();
            return std::make_shared<VarDeclarationNode>(line, kw, name, init, exposed);
        }
        case AstTag::USING: {
            int line = r.i32();
            std::string orig = r.string();
            std::string alias = r.string();
            return std::make_shared<UsingNode>(line, orig, alias);
        }
        case AstTag::IF_STATEMENT: {
            int line = r.i32();
            AstNodePtr cond = read_ast_node(r);
            uint32_t tn = r.u32();
            std::vector<AstNodePtr> then_branch;
            then_branch.reserve(tn);
            for (uint32_t i = 0; i < tn; ++i) then_branch.push_back(read_ast_node(r));
            uint32_t en = r.u32();
            std::vector<AstNodePtr> else_branch;
            else_branch.reserve(en);
            for (uint32_t i = 0; i < en; ++i) else_branch.push_back(read_ast_node(r));
            return std::make_shared<IfStatementNode>(line, cond, then_branch, else_branch);
        }
        case AstTag::WHILE_STATEMENT: {
            int line = r.i32();
            AstNodePtr cond = read_ast_node(r);
            uint32_t dn = r.u32();
            std::vector<AstNodePtr> do_branch;
            do_branch.reserve(dn);
            for (uint32_t i = 0; i < dn; ++i) do_branch.push_back(read_ast_node(r));
            uint32_t fn = r.u32();
            std::vector<AstNodePtr> finally_branch;
            finally_branch.reserve(fn);
            for (uint32_t i = 0; i < fn; ++i) finally_branch.push_back(read_ast_node(r));
            return std::make_shared<WhileStatementNode>(line, cond, do_branch, finally_branch);
        }
        case AstTag::LOOP_FOR: {
            int line = r.i32();
            std::string ivn = r.string();
            uint32_t bn = r.u32();
            std::vector<AstNodePtr> body;
            body.reserve(bn);
            for (uint32_t i = 0; i < bn; ++i) body.push_back(read_ast_node(r));
            AstNodePtr count = read_ast_node(r);
            return std::make_shared<LoopForNode>(line, ivn, body, count);
        }
        case AstTag::FOR_IN: {
            int line = r.i32();
            std::string vn = r.string();
            AstNodePtr iter = read_ast_node(r);
            uint32_t bn = r.u32();
            std::vector<AstNodePtr> body;
            body.reserve(bn);
            for (uint32_t i = 0; i < bn; ++i) body.push_back(read_ast_node(r));
            return std::make_shared<ForInNode>(line, vn, iter, body);
        }
        case AstTag::LOOP_UNTIL: {
            int line = r.i32();
            std::string ivn = r.string();
            uint32_t bn = r.u32();
            std::vector<AstNodePtr> body;
            body.reserve(bn);
            for (uint32_t i = 0; i < bn; ++i) body.push_back(read_ast_node(r));
            AstNodePtr cond = read_ast_node(r);
            return std::make_shared<LoopUntilNode>(line, ivn, body, cond);
        }
        case AstTag::BREAK: {
            int line = r.i32();
            return std::make_shared<BreakNode>(line);
        }
        case AstTag::CONTINUE: {
            int line = r.i32();
            return std::make_shared<ContinueNode>(line);
        }
        case AstTag::AWAIT_STATEMENT: {
            int line = r.i32();
            AstNodePtr cond = read_ast_node(r);
            uint32_t tn = r.u32();
            std::vector<AstNodePtr> then_branch;
            then_branch.reserve(tn);
            for (uint32_t i = 0; i < tn; ++i) then_branch.push_back(read_ast_node(r));
            return std::make_shared<AwaitStatementNode>(line, cond, then_branch);
        }
        case AstTag::SAY: {
            int line = r.i32();
            return std::make_shared<SayNode>(line, read_ast_node(r));
        }
        case AstTag::INP: {
            int line = r.i32();
            return std::make_shared<InpNode>(line, read_ast_node(r));
        }
        case AstTag::FN_DEF: {
            int line = r.i32();
            std::string name = r.string();
            uint32_t pn = r.u32();
            std::vector<ParameterDefinition> params;
            params.reserve(pn);
            for (uint32_t i = 0; i < pn; ++i) params.push_back(read_param_def(r));
            uint32_t bn = r.u32();
            std::vector<AstNodePtr> body;
            body.reserve(bn);
            for (uint32_t i = 0; i < bn; ++i) body.push_back(read_ast_node(r));
            bool method = r.bool8();
            bool exposed = r.bool8();
            return std::make_shared<FnDefNode>(line, name, params, body, method, exposed);
        }
        case AstTag::CALL: {
            int line = r.i32();
            AstNodePtr callee = read_ast_node(r);
            uint32_t an = r.u32();
            std::vector<AstNodePtr> args;
            args.reserve(an);
            for (uint32_t i = 0; i < an; ++i) args.push_back(read_ast_node(r));
            return std::make_shared<CallNode>(line, callee, args);
        }
        case AstTag::SUBSCRIPT: {
            int line = r.i32();
            AstNodePtr obj = read_ast_node(r);
            AstNodePtr start = read_ast_node(r);
            AstNodePtr end = read_ast_node(r);
            AstNodePtr step = read_ast_node(r);
            bool slice = r.bool8();
            return std::make_shared<SubscriptNode>(line, obj, start, end, step, slice);
        }
        case AstTag::RETURN: {
            int line = r.i32();
            return std::make_shared<ReturnNode>(line, read_ast_node(r));
        }
        case AstTag::RAISE: {
            int line = r.i32();
            return std::make_shared<RaiseNode>(line, read_ast_node(r));
        }
        case AstTag::TRY_CATCH: {
            int line = r.i32();
            uint32_t tn = r.u32();
            std::vector<AstNodePtr> try_branch;
            try_branch.reserve(tn);
            for (uint32_t i = 0; i < tn; ++i) try_branch.push_back(read_ast_node(r));
            std::string ev = r.string();
            uint32_t cn = r.u32();
            std::vector<AstNodePtr> catch_branch;
            catch_branch.reserve(cn);
            for (uint32_t i = 0; i < cn; ++i) catch_branch.push_back(read_ast_node(r));
            uint32_t fn = r.u32();
            std::vector<AstNodePtr> finally_branch;
            finally_branch.reserve(fn);
            for (uint32_t i = 0; i < fn; ++i) finally_branch.push_back(read_ast_node(r));
            return std::make_shared<TryCatchNode>(line, try_branch, ev, catch_branch, finally_branch);
        }
        case AstTag::CLASS_DEF: {
            int line = r.i32();
            std::string name = r.string();
            uint32_t fn = r.u32();
            std::vector<ParameterDefinition> fields;
            fields.reserve(fn);
            for (uint32_t i = 0; i < fn; ++i) fields.push_back(read_param_def(r));
            uint32_t in = r.u32();
            std::vector<AstNodePtr> init_body;
            init_body.reserve(in);
            for (uint32_t i = 0; i < in; ++i) init_body.push_back(read_ast_node(r));
            uint32_t mn = r.u32();
            std::vector<AstNodePtr> methods;
            methods.reserve(mn);
            for (uint32_t i = 0; i < mn; ++i) methods.push_back(read_ast_node(r));
            return std::make_shared<ClassDefNode>(line, name, fields, init_body, methods);
        }
        case AstTag::GET: {
            int line = r.i32();
            AstNodePtr obj = read_ast_node(r);
            std::string n = r.string();
            return std::make_shared<GetNode>(line, obj, n);
        }
        case AstTag::SET: {
            int line = r.i32();
            AstNodePtr obj = read_ast_node(r);
            std::string n = r.string();
            AstNodePtr val = read_ast_node(r);
            return std::make_shared<SetNode>(line, obj, n, val);
        }
        case AstTag::LAMBDA: {
            int line = r.i32();
            uint32_t pn = r.u32();
            std::vector<ParameterDefinition> params;
            params.reserve(pn);
            for (uint32_t i = 0; i < pn; ++i) params.push_back(read_param_def(r));
            uint32_t bn = r.u32();
            std::vector<AstNodePtr> body;
            body.reserve(bn);
            for (uint32_t i = 0; i < bn; ++i) body.push_back(read_ast_node(r));
            return std::make_shared<LambdaNode>(line, params, body);
        }
        case AstTag::STRUCT_DEF: {
            int line = r.i32();
            std::string name = r.string();
            uint32_t fn = r.u32();
            std::vector<ParameterDefinition> fields;
            fields.reserve(fn);
            for (uint32_t i = 0; i < fn; ++i) fields.push_back(read_param_def(r));
            return std::make_shared<StructDefNode>(line, name, fields);
        }
        case AstTag::SWAP: {
            int line = r.i32();
            AstNodePtr left = read_ast_node(r);
            AstNodePtr right = read_ast_node(r);
            return std::make_shared<SwapNode>(line, left, right);
        }
        case AstTag::REQUIRE: {
            int line = r.i32();
            std::string path = r.string();
            std::string alias = r.string();
            return std::make_shared<RequireNode>(line, path, alias);
        }
        case AstTag::EXPRESSION_STATEMENT: {
            int line = r.i32();
            return std::make_shared<ExpressionStatementNode>(line, read_ast_node(r));
        }
    }
    return nullptr;
}

// ============================================================================
// Vector<AstNodePtr> serialization (.prt top-level)
// ============================================================================
void write_ast(BinWriter& w, const std::vector<AstNodePtr>& nodes) {
    w.u32(static_cast<uint32_t>(nodes.size()));
    for (const auto& n : nodes) write_ast_node(w, n);
}

std::vector<AstNodePtr> read_ast(BinReader& r) {
    uint32_t n = r.u32();
    std::vector<AstNodePtr> nodes;
    nodes.reserve(n);
    for (uint32_t i = 0; i < n; ++i) nodes.push_back(read_ast_node(r));
    return nodes;
}

// ============================================================================
// CompiledFunction serialization
// ============================================================================
void write_compiled_function(BinWriter& w, CompiledFunctionPtr fn) {
    w.string(fn->name);
    w.u32(static_cast<uint32_t>(fn->code.size()));
    if (!fn->code.empty())
        w.raw(reinterpret_cast<const char*>(fn->code.data()), fn->code.size());
    w.u32(static_cast<uint32_t>(fn->constants.size()));
    for (const auto& c : fn->constants) write_value(w, c);
    w.u32(static_cast<uint32_t>(fn->lines.size()));
    for (int l : fn->lines) w.i32(l);
    w.u32(static_cast<uint32_t>(fn->ast_nodes.size()));
    for (const auto& n : fn->ast_nodes) write_ast_node(w, n);
    w.i32(fn->arity);
    w.i32(fn->num_params);
    w.i32(fn->upvalue_count);
    w.i32(fn->max_locals);
    w.u32(static_cast<uint32_t>(fn->upvalue_descriptors.size()));
    for (const auto& ud : fn->upvalue_descriptors) {
        w.bool8(ud.is_local);
        w.i32(ud.index);
    }
    w.u32(static_cast<uint32_t>(fn->params.size()));
    for (const auto& p : fn->params) {
        w.u32(static_cast<uint32_t>(p.type_keyword));
        w.string(p.name);
        write_value(w, p.default_value);
        write_ast_node(w, p.default_expr);
        w.bool8(p.has_default);
    }
    w.u32(static_cast<uint32_t>(fn->local_names.size()));
    for (const auto& ln : fn->local_names) w.string(ln);
    w.bool8(fn->is_method);
    w.bool8(fn->is_lambda);
}

CompiledFunctionPtr read_compiled_function(BinReader& r) {
    auto fn = std::make_shared<CompiledFunction>();
    fn->name = r.string();
    uint32_t code_len = r.u32();
    fn->code.resize(code_len);
    if (code_len > 0) r.raw(reinterpret_cast<char*>(fn->code.data()), code_len);
    uint32_t const_count = r.u32();
    fn->constants.reserve(const_count);
    for (uint32_t i = 0; i < const_count; ++i) fn->constants.push_back(read_value(r));
    uint32_t line_count = r.u32();
    fn->lines.reserve(line_count);
    for (uint32_t i = 0; i < line_count; ++i) fn->lines.push_back(r.i32());
    uint32_t ast_count = r.u32();
    fn->ast_nodes.reserve(ast_count);
    for (uint32_t i = 0; i < ast_count; ++i) fn->ast_nodes.push_back(read_ast_node(r));
    fn->arity = r.i32();
    fn->num_params = r.i32();
    fn->upvalue_count = r.i32();
    fn->max_locals = r.i32();
    uint32_t updesc_count = r.u32();
    fn->upvalue_descriptors.reserve(updesc_count);
    for (uint32_t i = 0; i < updesc_count; ++i) {
        UpvalueDesc ud;
        ud.is_local = r.bool8();
        ud.index = r.i32();
        fn->upvalue_descriptors.push_back(ud);
    }
    uint32_t params_count = r.u32();
    fn->params.reserve(params_count);
    for (uint32_t i = 0; i < params_count; ++i) {
        CompiledFunction::ParamInfo pi;
        pi.type_keyword = static_cast<TokenType>(r.u32());
        pi.name = r.string();
        pi.default_value = read_value(r);
        pi.default_expr = read_ast_node(r);
        pi.has_default = r.bool8();
        fn->params.push_back(pi);
    }
    uint32_t local_name_count = r.u32();
    fn->local_names.reserve(local_name_count);
    for (uint32_t i = 0; i < local_name_count; ++i)
        fn->local_names.push_back(r.string());
    fn->is_method = r.bool8();
    fn->is_lambda = r.bool8();
    return fn;
}

// ============================================================================
// BytecodeChunk serialization (.prc)
// ============================================================================
void write_bytecode_chunk(BinWriter& w, BytecodeChunkPtr chunk) {
    w.u32(static_cast<uint32_t>(chunk->all_functions.size()));
    for (const auto& fn : chunk->all_functions) write_compiled_function(w, fn);
}

BytecodeChunkPtr read_bytecode_chunk(BinReader& r) {
    auto chunk = std::make_shared<BytecodeChunk>();
    uint32_t fn_count = r.u32();
    chunk->all_functions.clear();
    chunk->all_functions.reserve(fn_count);
    for (uint32_t i = 0; i < fn_count; ++i) {
        auto fn = read_compiled_function(r);
        chunk->all_functions.push_back(fn);
        if (i == 0) chunk->script = fn;
    }
    return chunk;
}

// ============================================================================
// High-level file I/O
// ============================================================================
static const char* PRC_MAGIC = "PyRc";
static const char* PRT_MAGIC = "PyRt";
static const uint32_t FORMAT_VERSION = 1;

void save_bytecode(const std::string& path, BytecodeChunkPtr chunk) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file for writing: " + path);
    BinWriter w(file);
    w.raw(PRC_MAGIC, 4);
    w.u32(FORMAT_VERSION);
    write_bytecode_chunk(w, chunk);
    file.close();
}

BytecodeChunkPtr load_bytecode(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    BinReader r(file);
    char magic[4];
    r.raw(magic, 4);
    if (magic[0] != PRC_MAGIC[0] || magic[1] != PRC_MAGIC[1] ||
        magic[2] != PRC_MAGIC[2] || magic[3] != PRC_MAGIC[3])
        throw std::runtime_error("Invalid .prc file: bad magic number");
    uint32_t ver = r.u32();
    if (ver != FORMAT_VERSION)
        throw std::runtime_error("Unsupported .prc format version");
    auto chunk = read_bytecode_chunk(r);
    file.close();
    return chunk;
}

void save_ast(const std::string& path, const std::vector<AstNodePtr>& nodes) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file for writing: " + path);
    BinWriter w(file);
    w.raw(PRT_MAGIC, 4);
    w.u32(FORMAT_VERSION);
    write_ast(w, nodes);
    file.close();
}

std::vector<AstNodePtr> load_ast(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    BinReader r(file);
    char magic[4];
    r.raw(magic, 4);
    if (magic[0] != PRT_MAGIC[0] || magic[1] != PRT_MAGIC[1] ||
        magic[2] != PRT_MAGIC[2] || magic[3] != PRT_MAGIC[3])
        throw std::runtime_error("Invalid .prt file: bad magic number");
    uint32_t ver = r.u32();
    if (ver != FORMAT_VERSION)
        throw std::runtime_error("Unsupported .prt format version");
    auto nodes = read_ast(r);
    file.close();
    return nodes;
}
