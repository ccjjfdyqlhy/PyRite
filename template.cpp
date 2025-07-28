#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <thread>
#include <cmath>
#include <functional>
#include <fstream>
#include <exception>
#include <cstdlib> // For system()
#include <cstdio>  // For remove()
#include <unordered_set> // For type checking
#include "BigNumber.hpp"

struct Value; struct Function; class Interpreter; class Environment; class NumberValue; class BinaryValue; class StringValue; class ListValue; class NativeFnValue; class ExceptionValue;
using ValuePtr = std::shared_ptr<Value>;

// --- 辅助函数：分割字符串为行 ---
std::vector<std::string> split_lines(const std::string& source) {
    std::vector<std::string> lines;
    std::string line;
    std::istringstream stream(source);
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string trim(const std::string& str);


struct Value {
    int origin_line = 0; // 新增：追踪值的来源行号
    virtual ~Value() {}
    virtual std::string toString() const = 0;
    virtual std::string repr() const = 0; 
    virtual bool isTruthy() const = 0;
    virtual ValuePtr clone() const = 0;
    virtual ValuePtr add(const Value& other) const; virtual ValuePtr subtract(const Value& other) const; virtual ValuePtr multiply(const Value& other) const; virtual ValuePtr divide(const Value& other) const; virtual ValuePtr power(const Value& other) const;
    virtual bool isEqualTo(const Value& other) const; virtual bool isLessThan(const Value& other) const;
    // For lists
    virtual ValuePtr getSubscript(const Value& index) const; virtual void setSubscript(const Value& index, ValuePtr value);
};

// --- 派生类 ---
class NullValue : public Value {
public:
    std::string toString() const override { return "null"; }
    std::string repr() const override { std::stringstream ss; ss << "<NullObject at " << static_cast<const void*>(this) << ">"; return ss.str(); }
    bool isTruthy() const override { return false; }
    ValuePtr clone() const override { auto c = std::make_shared<NullValue>(); c->origin_line = origin_line; return c; }
    bool isEqualTo(const Value& other) const override { return dynamic_cast<const NullValue*>(&other) != nullptr; }
};

class NumberValue : public Value {
public:
    BigNumber value;
    NumberValue(const BigNumber& n) : value(n) {}
    std::string toString() const override { return value.toString(); }
    std::string repr() const override { return value.toString(); }
    bool isTruthy() const override { return value != BigNumber(0); }
    ValuePtr clone() const override { auto c = std::make_shared<NumberValue>(value); c->origin_line = origin_line; return c; }
    ValuePtr add(const Value& other) const override; ValuePtr subtract(const Value& other) const override; ValuePtr multiply(const Value& other) const override; ValuePtr divide(const Value& other) const override; ValuePtr power(const Value& other) const override;
    bool isEqualTo(const Value& other) const override; bool isLessThan(const Value& other) const override;
};

class BinaryValue : public Value {
public:
    std::vector<uint8_t> value;
    BinaryValue(const std::vector<uint8_t>& v) : value(v) {}
    BinaryValue(const std::string& hex_str);
    std::string toString() const override;
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { for (auto byte : value) { if (byte != 0) return true; } return false; }
    ValuePtr clone() const override { auto c = std::make_shared<BinaryValue>(value); c->origin_line = origin_line; return c; }
    ValuePtr add(const Value& other) const override;
    bool isEqualTo(const Value& other) const override;
    BigNumber toBigNumber() const;
};

class StringValue : public Value {
public:
    std::string value;
    StringValue(const std::string& s) : value(s) {}
    std::string toString() const override { return value; }
    std::string repr() const override { std::stringstream ss; ss << "'" << value << "'"; return ss.str(); }
    bool isTruthy() const override { return !value.empty(); }
    ValuePtr clone() const override { auto c = std::make_shared<StringValue>(value); c->origin_line = origin_line; return c; }
    ValuePtr add(const Value& other) const override;
    bool isEqualTo(const Value& other) const override; bool isLessThan(const Value& other) const override;
};

class ListValue : public Value {
public:
    std::vector<ValuePtr> elements;
    ListValue(const std::vector<ValuePtr>& e) : elements(e) {}
    std::string toString() const override;
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return !elements.empty(); }
    ValuePtr clone() const override { auto c = std::make_shared<ListValue>(elements); c->origin_line = origin_line; return c; }
    ValuePtr add(const Value& other) const override;
    ValuePtr multiply(const Value& other) const override;
    bool isEqualTo(const Value& other) const override;
    ValuePtr getSubscript(const Value& index) const override;
    void setSubscript(const Value& index, ValuePtr value) override;
};

class FunctionValue : public Value {
public:
    std::shared_ptr<Function> value;
    FunctionValue(const std::shared_ptr<Function>& f) : value(f) {}
    std::string toString() const override;
    std::string repr() const override;
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { auto c = std::make_shared<FunctionValue>(value); c->origin_line = origin_line; return c; }
};

class NativeFnValue : public Value {
public:
    using NativeFn = std::function<ValuePtr(const std::vector<ValuePtr>&)>;
    NativeFn fn;
    std::string name;
    NativeFnValue(const std::string& n, NativeFn f) : name(n), fn(f) {}
    std::string toString() const override { return "<native function " + name + ">"; }
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { auto c = std::make_shared<NativeFnValue>(name, fn); c->origin_line = origin_line; return c; }
    ValuePtr call(const std::vector<ValuePtr>& args) const { return fn(args); }
};

class ExceptionValue : public Value {
public:
    ValuePtr payload;
    ExceptionValue(ValuePtr p) : payload(p) {}
    std::string toString() const override { return "<Exception: " + payload->toString() + ">"; }
    std::string repr() const override { std::stringstream ss; ss << "<ExceptionObject at " << static_cast<const void*>(this) << " payload=" << payload->repr() << ">"; return ss.str(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { auto c = std::make_shared<ExceptionValue>(payload->clone()); c->origin_line = origin_line; return c; }
    bool isEqualTo(const Value& other) const override { if (auto o = dynamic_cast<const ExceptionValue*>(&other)) { return payload->isEqualTo(*(o->payload)); } return false; }
};


// --- 运算符和转换实现 ---
ValuePtr Value::add(const Value&) const { throw std::runtime_error("Unsupported operand types for +."); }
ValuePtr Value::subtract(const Value&) const { throw std::runtime_error("Unsupported operand types for -."); }
ValuePtr Value::multiply(const Value&) const { throw std::runtime_error("Unsupported operand types for *."); }
ValuePtr Value::divide(const Value&) const { throw std::runtime_error("Unsupported operand types for /."); }
ValuePtr Value::power(const Value&) const { throw std::runtime_error("Unsupported operand types for ^."); }
bool Value::isEqualTo(const Value&) const { return false; }
bool Value::isLessThan(const Value&) const { throw std::runtime_error("Unsupported operand types for comparison."); }
ValuePtr Value::getSubscript(const Value&) const { throw std::runtime_error("Object is not subscriptable."); }
void Value::setSubscript(const Value&, ValuePtr) { throw std::runtime_error("Object does not support item assignment."); }

ValuePtr NumberValue::add(const Value& other) const {
    if (auto o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value + o->value);
    if (auto o = dynamic_cast<const BinaryValue*>(&other)) return std::make_shared<NumberValue>(this->value + o->toBigNumber());
    return std::make_shared<StringValue>(this->toString() + other.toString()); 
}
ValuePtr NumberValue::subtract(const Value& other) const { if (auto o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value - o->value); return Value::subtract(other); }
ValuePtr NumberValue::multiply(const Value& other) const { if (auto o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value * o->value); return Value::multiply(other); }
ValuePtr NumberValue::divide(const Value& other) const { if (auto o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value / o->value); return Value::divide(other); }
ValuePtr NumberValue::power(const Value& other) const { if (auto o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value ^ o->value); return Value::power(other); }
bool NumberValue::isEqualTo(const Value& other) const { if (auto o = dynamic_cast<const NumberValue*>(&other)) return this->value == o->value; if (auto o = dynamic_cast<const BinaryValue*>(&other)) return this->value == o->toBigNumber(); return false; }
bool NumberValue::isLessThan(const Value& other) const { if (auto o = dynamic_cast<const NumberValue*>(&other)) return this->value < o->value; return Value::isLessThan(other); }

BinaryValue::BinaryValue(const std::string& hex_str) {
    if (hex_str.rfind("0x", 0) != 0) throw std::invalid_argument("Hex string must start with '0x'.");
    std::string hex = hex_str.substr(2);
    if (hex.length() % 2 != 0) hex = "0" + hex;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t)std::stoul(byteString, nullptr, 16);
        value.push_back(byte);
    }
}
std::string BinaryValue::toString() const {
    std::stringstream ss;
    ss << "0x";
    for (uint8_t byte : value) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
}
BigNumber BinaryValue::toBigNumber() const {
    BigNumber result(0);
    BigNumber power_of_256(1);
    for (auto it = value.rbegin(); it != value.rend(); ++it) {
        result = result + BigNumber((long long)*it) * power_of_256;
        power_of_256 = power_of_256 * BigNumber(256);
    }
    return result;
}
ValuePtr BinaryValue::add(const Value& other) const {
    if (auto o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->toBigNumber() + o->value);
    return std::make_shared<StringValue>(this->toString() + other.toString());
}
bool BinaryValue::isEqualTo(const Value& other) const {
    if (auto o = dynamic_cast<const BinaryValue*>(&other)) return this->value == o->value;
    if (auto o = dynamic_cast<const NumberValue*>(&other)) return this->toBigNumber() == o->value;
    return false;
}

ValuePtr StringValue::add(const Value& other) const { return std::make_shared<StringValue>(this->value + other.toString()); }
bool StringValue::isEqualTo(const Value& other) const { if (auto o = dynamic_cast<const StringValue*>(&other)) return this->value == o->value; return false; }
bool StringValue::isLessThan(const Value& other) const { if (auto o = dynamic_cast<const StringValue*>(&other)) return this->value < o->value; return Value::isLessThan(other); }

std::string ListValue::toString() const {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < elements.size(); ++i) {
        ss << elements[i]->repr();
        if (i < elements.size() - 1) ss << ", ";
    }
    ss << "]";
    return ss.str();
}
ValuePtr ListValue::add(const Value& other) const {
    if (auto o = dynamic_cast<const ListValue*>(&other)) {
        auto new_elements = this->elements;
        new_elements.insert(new_elements.end(), o->elements.begin(), o->elements.end());
        return std::make_shared<ListValue>(new_elements);
    }
    return Value::add(other);
}
ValuePtr ListValue::multiply(const Value& other) const {
    if (auto o = dynamic_cast<const NumberValue*>(&other)) {
        try {
            long long times = o->value.toLongLong();
            if (times < 0) times = 0;
            std::vector<ValuePtr> new_elements;
            for (long long i = 0; i < times; ++i) {
                for (const auto& elem : this->elements) {
                    new_elements.push_back(elem->clone());
                }
            }
            return std::make_shared<ListValue>(new_elements);
        } catch (...) {
            throw std::runtime_error("List repetition count must be an integer.");
        }
    }
    return Value::multiply(other);
}
bool ListValue::isEqualTo(const Value& other) const {
    auto o = dynamic_cast<const ListValue*>(&other);
    if (!o || this->elements.size() != o->elements.size()) return false;
    for (size_t i = 0; i < this->elements.size(); ++i) {
        if (!this->elements[i]->isEqualTo(*o->elements[i])) return false;
    }
    return true;
}
ValuePtr ListValue::getSubscript(const Value& index) const {
    auto num_val = dynamic_cast<const NumberValue*>(&index);
    if (!num_val) throw std::runtime_error("List index must be a number.");
    try {
        long long i = num_val->value.toLongLong();
        long long size = elements.size();
        if (i < 0) i += size;
        if (i >= 0 && i < size) {
            return elements[i];
        }
        throw std::runtime_error("List index out of range.");
    } catch (...) {
        throw std::runtime_error("Invalid list index.");
    }
}
void ListValue::setSubscript(const Value& index, ValuePtr value) {
    auto num_val = dynamic_cast<const NumberValue*>(&index);
    if (!num_val) throw std::runtime_error("List index must be a number.");
     try {
        long long i = num_val->value.toLongLong();
        long long size = elements.size();
        if (i < 0) i += size;
        if (i >= 0 && i < size) {
            elements[i] = value;
            return;
        }
        throw std::runtime_error("List index out of range.");
    } catch (...) {
        throw std::runtime_error("Invalid list index.");
    }
}

struct AstNode; class Environment; using AstNodePtr = std::shared_ptr<AstNode>;

// 用户定义函数
struct Function {
    std::string name; std::vector<std::string> params; std::vector<AstNodePtr> body; std::shared_ptr<Environment> closure; int definition_line;
    Function(const std::string& n, const std::vector<std::string>& p, const std::vector<AstNodePtr>& b, const std::shared_ptr<Environment>& c, int def_line) : name(n), params(p), body(b), closure(c), definition_line(def_line) {}
};
std::string FunctionValue::toString() const { return "<function " + value->name + ">"; }
std::string FunctionValue::repr() const {
    std::stringstream ss;
    ss << "<FuncObject '" << value->name << "' at " << static_cast<const void*>(this);
    if (value->closure) {
        ss << " enclosed in <Environment at " << static_cast<const void*>(value->closure.get()) << ">";
    }
    ss << ">";
    return ss.str();
}

// --- C++ 异常定义 ---
class RuntimeError : public std::runtime_error { 
public: 
    int line; 
    int context_line; // 新增：用于追踪导致错误的 "值" 的来源行
    RuntimeError(int l, const std::string& msg, int ctx_l = 0) : std::runtime_error(msg), line(l), context_line(ctx_l) {} 
};
class ReturnValueException : public std::runtime_error { public: ValuePtr value; ReturnValueException(ValuePtr v) : std::runtime_error(""), value(v) {} };
class SimPyRaiseException : public std::runtime_error { public: ValuePtr value; SimPyRaiseException(ValuePtr v) : std::runtime_error(""), value(v) {} };


enum class TokenType {
    DEC, STR, BIN, LIST, 
    IF, THEN, ELSE, ENDIF, WHILE, DO, FINALLY, ENDWHILE, DEF, ENDDEF, RETURN, SAY, INP, MARK, JUMP, HALT, RUN,
    TRY, CATCH, ENDTRY, RAISE, // 新增
    AWAIT, ENDAWAIT,
    IDENTIFIER, NUMBER, STRING, HEX_LITERAL, 
    EQUAL, EQUAL_EQUAL, BANG_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    PLUS, MINUS, STAR, SLASH, LPAREN, RPAREN, COMMA, CARET,
    LBRACKET, RBRACKET,
    END_OF_FILE, UNKNOWN
};
struct Token { TokenType type; std::string lexeme; int line; };

class Tokenizer {
public:
    Tokenizer(const std::string& source) : source(source), start(0), current(0), line(1) {
        keywords["dec"] = TokenType::DEC; keywords["str"] = TokenType::STR; keywords["bin"] = TokenType::BIN; keywords["list"] = TokenType::LIST;
        keywords["if"] = TokenType::IF; keywords["then"] = TokenType::THEN; keywords["else"] = TokenType::ELSE; keywords["endif"] = TokenType::ENDIF; keywords["while"] = TokenType::WHILE; keywords["do"] = TokenType::DO; keywords["finally"] = TokenType::FINALLY; keywords["endwhile"] = TokenType::ENDWHILE; keywords["def"] = TokenType::DEF; keywords["enddef"] = TokenType::ENDDEF; keywords["return"] = TokenType::RETURN; keywords["say"] = TokenType::SAY; keywords["inp"] = TokenType::INP; keywords["mark"] = TokenType::MARK; keywords["jump"] = TokenType::JUMP; keywords["halt"] = TokenType::HALT; keywords["run"] = TokenType::RUN;
        keywords["await"] = TokenType::AWAIT; keywords["endawait"] = TokenType::ENDAWAIT;
        keywords["try"] = TokenType::TRY; keywords["catch"] = TokenType::CATCH; keywords["endtry"] = TokenType::ENDTRY; keywords["raise"] = TokenType::RAISE;
    }
    Token next_token() {
        skip_whitespace(); start = current; if (is_at_end()) return make_token(TokenType::END_OF_FILE);
        char c = advance(); if (isalpha(c) || c == '_') return identifier();
        if (isdigit(c)) {
            if (c == '0' && (peek() == 'x' || peek() == 'X')) return hex_literal();
            return number();
        }
        switch (c) {
            case '(': return make_token(TokenType::LPAREN); case ')': return make_token(TokenType::RPAREN); case ',': return make_token(TokenType::COMMA);
            case '+': return make_token(TokenType::PLUS); case '-': return make_token(TokenType::MINUS); case '*': return make_token(TokenType::STAR); case '/': return make_token(TokenType::SLASH); case '^': return make_token(TokenType::CARET);
            case '=': return make_token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL); case '!': return make_token(match('=') ? TokenType::BANG_EQUAL : TokenType::UNKNOWN);
            case '<': return make_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS); case '>': return make_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
            case '"': case '\'': return string(c);
            case '[': return make_token(TokenType::LBRACKET); case ']': return make_token(TokenType::RBRACKET);
        }
        return make_token(TokenType::UNKNOWN, "Unexpected character.");
    }
private:
    const std::string& source; size_t start, current; int line; std::map<std::string, TokenType> keywords;
    bool is_at_end() { return current >= source.length(); } char advance() { return source[current++]; } char peek() { return is_at_end() ? '\0' : source[current]; } char peek_next() { return current + 1 >= source.length() ? '\0' : source[current + 1]; }
    bool match(char expected) { if (is_at_end() || source[current] != expected) return false; current++; return true; }
    void skip_whitespace() {
        while (true) {
            char c = peek();
            switch (c) {
                case ' ': case '\r': case '\t': advance(); break;
                case '\n': line++; advance(); break;
                case '#': { advance(); while (peek() != '#' && !is_at_end()) { if (peek() == '\n') line++; advance(); } if (!is_at_end()) { advance(); } break; }
                default: return;
            }
        }
    }
    Token make_token(TokenType type, const std::string& msg = "") { return Token{type, msg.empty() ? source.substr(start, current - start) : msg, line}; }
    Token identifier() { while (isalnum(peek()) || peek() == '_') advance(); std::string text = source.substr(start, current - start); auto it = keywords.find(text); return make_token(it != keywords.end() ? it->second : TokenType::IDENTIFIER); }
    Token number() { while (isdigit(peek())) advance(); if (peek() == '.' && isdigit(peek_next())) { advance(); while (isdigit(peek())) advance(); } return make_token(TokenType::NUMBER); }
    Token hex_literal() { advance(); while (isxdigit(peek())) advance(); return make_token(TokenType::HEX_LITERAL); }
    Token string(char quote) { while (peek() != quote && !is_at_end()) { if (peek() == '\n') line++; advance(); } if (is_at_end()) return make_token(TokenType::UNKNOWN, "Unterminated string."); advance(); return Token{TokenType::STRING, source.substr(start + 1, current - start - 2), line}; }
};

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment(std::shared_ptr<Environment> enc = nullptr) : enclosing(enc) {}
    void define(const std::string& name, ValuePtr value) { values[name] = value; }
    void assign(const std::string& name, ValuePtr value) { if (values.count(name)) { values[name] = value; return; } if (enclosing) { enclosing->assign(name, value); return; } throw RuntimeError(0, "Undefined variable '" + name + "'."); }
    ValuePtr get(const std::string& name) { if (values.count(name)) return values[name]; if (enclosing) return enclosing->get(name); throw RuntimeError(0, "Undefined variable '" + name + "'."); }
    ValuePtr get_type(const std::string& name) { 
        ValuePtr val = get(name);
        if (dynamic_cast<NumberValue*>(val.get())) return std::make_shared<StringValue>("dec");
        if (dynamic_cast<StringValue*>(val.get())) return std::make_shared<StringValue>("str");
        if (dynamic_cast<BinaryValue*>(val.get())) return std::make_shared<StringValue>("bin");
        if (dynamic_cast<ListValue*>(val.get())) return std::make_shared<StringValue>("list");
        if (dynamic_cast<ExceptionValue*>(val.get())) return std::make_shared<StringValue>("exception");
        return std::make_shared<StringValue>("unknown");
    }
private:
    std::shared_ptr<Environment> enclosing; std::map<std::string, ValuePtr> values;
};

// --- AST 节点定义 ---
struct AstNode { int line; AstNode(int l) : line(l) {} virtual ~AstNode() = default; virtual ValuePtr accept(Interpreter& visitor) = 0; };
struct LiteralNode : AstNode { ValuePtr value; LiteralNode(int l, ValuePtr v) : AstNode(l), value(v) {} ValuePtr accept(Interpreter& visitor) override; };
struct ListLiteralNode : AstNode { std::vector<AstNodePtr> elements; ListLiteralNode(int l, std::vector<AstNodePtr> e) : AstNode(l), elements(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct VariableNode : AstNode { std::string name; VariableNode(int l, std::string n) : AstNode(l), name(n) {} ValuePtr accept(Interpreter& visitor) override; };
struct BinaryOpNode : AstNode { AstNodePtr left; Token op; AstNodePtr right; BinaryOpNode(int l, AstNodePtr lt, Token o, AstNodePtr rt) : AstNode(l), left(lt), op(o), right(rt) {} ValuePtr accept(Interpreter& visitor) override; };
struct AssignmentNode : AstNode { AstNodePtr target; AstNodePtr value; AssignmentNode(int l, AstNodePtr t, AstNodePtr v) : AstNode(l), target(t), value(v) {} ValuePtr accept(Interpreter& visitor) override; };
struct VarDeclarationNode : AstNode { Token keyword; std::string name; AstNodePtr initializer; VarDeclarationNode(int l, Token kw, std::string n, AstNodePtr init) : AstNode(l), keyword(kw), name(n), initializer(init) {} ValuePtr accept(Interpreter& visitor) override; };
struct IfStatementNode : AstNode { AstNodePtr condition; std::vector<AstNodePtr> then_branch, else_branch; IfStatementNode(int l, AstNodePtr c, std::vector<AstNodePtr> t, std::vector<AstNodePtr> e) : AstNode(l), condition(c), then_branch(t), else_branch(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct WhileStatementNode : AstNode { AstNodePtr condition; std::vector<AstNodePtr> do_branch, finally_branch; WhileStatementNode(int l, AstNodePtr c, std::vector<AstNodePtr> d, std::vector<AstNodePtr> f) : AstNode(l), condition(c), do_branch(d), finally_branch(f) {} ValuePtr accept(Interpreter& visitor) override; };
struct AwaitStatementNode : AstNode { AstNodePtr condition; std::vector<AstNodePtr> then_branch; AwaitStatementNode(int l, AstNodePtr c, std::vector<AstNodePtr> t) : AstNode(l), condition(c), then_branch(t) {} ValuePtr accept(Interpreter& visitor) override; };
struct SayNode : AstNode { AstNodePtr expression; SayNode(int l, AstNodePtr e) : AstNode(l), expression(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct InpNode : AstNode { AstNodePtr expression; InpNode(int l, AstNodePtr e) : AstNode(l), expression(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct FunctionDefNode : AstNode { std::string name; std::vector<std::string> params; std::vector<AstNodePtr> body; FunctionDefNode(int l, std::string n, std::vector<std::string> p, std::vector<AstNodePtr> b) : AstNode(l), name(n), params(p), body(b) {} ValuePtr accept(Interpreter& visitor) override; };
struct CallNode : AstNode { AstNodePtr callee; std::vector<AstNodePtr> arguments; CallNode(int l, AstNodePtr c, std::vector<AstNodePtr> a) : AstNode(l), callee(c), arguments(a) {} ValuePtr accept(Interpreter& visitor) override; };
struct SubscriptNode : AstNode { AstNodePtr object; AstNodePtr index; SubscriptNode(int l, AstNodePtr o, AstNodePtr i) : AstNode(l), object(o), index(i) {} ValuePtr accept(Interpreter& visitor) override; };
struct ReturnNode : AstNode { AstNodePtr value; ReturnNode(int l, AstNodePtr v) : AstNode(l), value(v) {} ValuePtr accept(Interpreter& visitor) override; };
struct RaiseNode : AstNode { AstNodePtr expression; RaiseNode(int l, AstNodePtr e) : AstNode(l), expression(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct TryCatchNode : AstNode { std::vector<AstNodePtr> try_branch; std::string exception_var; std::vector<AstNodePtr> catch_branch; std::vector<AstNodePtr> finally_branch; TryCatchNode(int l, std::vector<AstNodePtr> t, std::string ev, std::vector<AstNodePtr> c, std::vector<AstNodePtr> f) : AstNode(l), try_branch(t), exception_var(ev), catch_branch(c), finally_branch(f) {} ValuePtr accept(Interpreter& visitor) override; };


class Parser {
public:
    Parser(const std::string& source) : tokenizer(source), had_error(false) {}
    std::vector<AstNodePtr> parse() { std::vector<AstNodePtr> statements; current_token = tokenizer.next_token(); while (current_token.type != TokenType::END_OF_FILE && current_token.type != TokenType::HALT && current_token.type != TokenType::RUN) { try { statements.push_back(declaration()); } catch (const std::runtime_error& e) { std::cerr << "[解析错误] 行 " << current_token.line << ": " << e.what() << std::endl; had_error = true; synchronize(); } } return statements; }
    bool has_error() const { return had_error; }
private:
    Tokenizer tokenizer; Token current_token, previous_token; bool had_error;
    void advance() { previous_token = current_token; current_token = tokenizer.next_token(); }
    void consume(TokenType type, const std::string& msg) { if (current_token.type == type) { advance(); return; } throw std::runtime_error(msg); }
    bool check(TokenType type) { return current_token.type == type; }
    bool match(const std::vector<TokenType>& types) { for (TokenType type : types) { if (check(type)) { advance(); return true; } } return false; }
    void synchronize() { advance(); while(current_token.type != TokenType::END_OF_FILE) { switch(current_token.type) { case TokenType::DEC: case TokenType::STR: case TokenType::IF: case TokenType::WHILE: case TokenType::DEF: case TokenType::SAY: case TokenType::RETURN: case TokenType::TRY: return; default: advance(); } } }
    AstNodePtr declaration() { if (match({TokenType::DEC, TokenType::STR, TokenType::BIN, TokenType::LIST})) return var_declaration(); if (match({TokenType::DEF})) return function_definition("function"); return statement(); }
    AstNodePtr var_declaration() { Token keyword = previous_token; consume(TokenType::IDENTIFIER, "需要变量名."); std::string name = previous_token.lexeme; AstNodePtr initializer = nullptr; if (match({TokenType::EQUAL})) { initializer = expression(); } return std::make_shared<VarDeclarationNode>(keyword.line, keyword, name, initializer); }
    AstNodePtr function_definition(const std::string& kind) { int line = previous_token.line; consume(TokenType::IDENTIFIER, "需要 " + kind + " 名称."); std::string name = previous_token.lexeme; consume(TokenType::LPAREN, "名称后需要 '('."); std::vector<std::string> params; if (!check(TokenType::RPAREN)) { do { if (params.size() >= 255) throw std::runtime_error("函数参数不能超过255个."); consume(TokenType::IDENTIFIER, "需要参数名."); params.push_back(previous_token.lexeme); } while (match({TokenType::COMMA})); } consume(TokenType::RPAREN, "参数后需要 ')'."); consume(TokenType::DO, "函数体前需要 'do'."); std::vector<AstNodePtr> body; while (!check(TokenType::ENDDEF) && !check(TokenType::END_OF_FILE)) { body.push_back(declaration()); } consume(TokenType::ENDDEF, "函数体后需要 'enddef'."); return std::make_shared<FunctionDefNode>(line, name, params, body); }
    AstNodePtr statement() { if (match({TokenType::IF})) return if_statement(); if (match({TokenType::WHILE})) return while_statement(); if (match({TokenType::AWAIT})) return await_statement(); if (match({TokenType::TRY})) return try_statement(); if (match({TokenType::RAISE})) return raise_statement(); if (match({TokenType::SAY})) return say_statement(); if (match({TokenType::RETURN})) return return_statement(); if (check(TokenType::ELSE) || check(TokenType::ENDIF) || check(TokenType::FINALLY) || check(TokenType::ENDWHILE) || check(TokenType::ENDDEF) || check(TokenType::ENDAWAIT) || check(TokenType::CATCH) || check(TokenType::ENDTRY)) { throw std::runtime_error("关键字 '" + current_token.lexeme + "' 位置错误."); } return expression_statement(); }
    AstNodePtr if_statement() { int line = previous_token.line; AstNodePtr condition = expression(); consume(TokenType::THEN, "if 条件后需要 'then'."); std::vector<AstNodePtr> then_branch; while(!check(TokenType::ELSE) && !check(TokenType::ENDIF) && !check(TokenType::END_OF_FILE)) { then_branch.push_back(declaration()); } std::vector<AstNodePtr> else_branch; if (match({TokenType::ELSE})) { while(!check(TokenType::ENDIF) && !check(TokenType::END_OF_FILE)) { else_branch.push_back(declaration()); } } consume(TokenType::ENDIF, "if 语句后需要 'endif'."); return std::make_shared<IfStatementNode>(line, condition, then_branch, else_branch); }
    AstNodePtr while_statement() { int line = previous_token.line; AstNodePtr condition = expression(); consume(TokenType::DO, "while 条件后需要 'do'."); std::vector<AstNodePtr> do_branch; while(!check(TokenType::FINALLY) && !check(TokenType::ENDWHILE) && !check(TokenType::END_OF_FILE)) { do_branch.push_back(declaration()); } std::vector<AstNodePtr> finally_branch; if (match({TokenType::FINALLY})) { while(!check(TokenType::ENDWHILE) && !check(TokenType::END_OF_FILE)) { finally_branch.push_back(declaration()); } } consume(TokenType::ENDWHILE, "while 循环后需要 'endwhile'."); return std::make_shared<WhileStatementNode>(line, condition, do_branch, finally_branch); }
    AstNodePtr await_statement() { int line = previous_token.line; AstNodePtr condition = expression(); consume(TokenType::THEN, "await 条件后需要 'then'."); std::vector<AstNodePtr> then_branch; while (!check(TokenType::ENDAWAIT) && !check(TokenType::END_OF_FILE)) { then_branch.push_back(declaration()); } consume(TokenType::ENDAWAIT, "await 语句后需要 'endawait'."); return std::make_shared<AwaitStatementNode>(line, condition, then_branch); }
    AstNodePtr try_statement() { int line = previous_token.line; std::vector<AstNodePtr> try_branch; while (!check(TokenType::CATCH) && !check(TokenType::END_OF_FILE)) { try_branch.push_back(declaration()); } consume(TokenType::CATCH, "'try' 块后需要 'catch'."); consume(TokenType::IDENTIFIER, "'catch' 后需要一个变量名."); std::string exception_var = previous_token.lexeme; std::vector<AstNodePtr> catch_branch; while (!check(TokenType::FINALLY) && !check(TokenType::ENDTRY) && !check(TokenType::END_OF_FILE)) { catch_branch.push_back(declaration()); } std::vector<AstNodePtr> finally_branch; if (match({TokenType::FINALLY})) { while (!check(TokenType::ENDTRY) && !check(TokenType::END_OF_FILE)) { finally_branch.push_back(declaration()); } } consume(TokenType::ENDTRY, "try/catch/finally 结构后需要 'endtry'."); return std::make_shared<TryCatchNode>(line, try_branch, exception_var, catch_branch, finally_branch); }
    AstNodePtr raise_statement() { int line = previous_token.line; AstNodePtr expr = expression(); return std::make_shared<RaiseNode>(line, expr); }
    AstNodePtr say_statement() { int line = previous_token.line; consume(TokenType::LPAREN, "'say' 后需要 '('."); AstNodePtr value = expression(); consume(TokenType::RPAREN, "表达式后需要 ')'."); return std::make_shared<SayNode>(line, value); }
    AstNodePtr return_statement() { int line = previous_token.line; ValuePtr v = std::make_shared<NullValue>(); AstNodePtr val_node = std::make_shared<LiteralNode>(line, v); if (!check(TokenType::ENDDEF) && !check(TokenType::ENDIF) && !check(TokenType::ENDWHILE) && !check(TokenType::ENDTRY)) { val_node = expression(); } return std::make_shared<ReturnNode>(line, val_node); }
    AstNodePtr expression_statement() { return expression(); }
    AstNodePtr expression() { return assignment(); }
    AstNodePtr assignment() { AstNodePtr expr = equality(); if (match({TokenType::EQUAL})) { int line = previous_token.line; AstNodePtr value = assignment(); if (dynamic_cast<VariableNode*>(expr.get()) || dynamic_cast<SubscriptNode*>(expr.get())) { return std::make_shared<AssignmentNode>(line, expr, value); } throw std::runtime_error("无效的赋值目标."); } return expr; }
    AstNodePtr equality() { AstNodePtr expr = comparison(); while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) { Token op = previous_token; AstNodePtr right = comparison(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr comparison() { AstNodePtr expr = term(); while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL})) { Token op = previous_token; AstNodePtr right = term(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr term() { AstNodePtr expr = factor(); while (match({TokenType::PLUS, TokenType::MINUS})) { Token op = previous_token; AstNodePtr right = factor(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr factor() { AstNodePtr expr = power(); while (match({TokenType::STAR, TokenType::SLASH})) { Token op = previous_token; AstNodePtr right = power(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr power() { AstNodePtr expr = unary(); while(match({TokenType::CARET})) { Token op = previous_token; AstNodePtr right = unary(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr unary() { if (match({TokenType::MINUS})) { Token op = previous_token; AstNodePtr right = unary(); AstNodePtr left = std::make_shared<LiteralNode>(op.line, std::make_shared<NumberValue>(0)); return std::make_shared<BinaryOpNode>(op.line, left, op, right); } return call(); }
    AstNodePtr call() { AstNodePtr expr = primary(); while (true) { if (match({TokenType::LPAREN})) { expr = finish_call(expr); } else if (match({TokenType::LBRACKET})) { expr = finish_subscript(expr); } else { break; } } return expr; }
    AstNodePtr finish_call(AstNodePtr callee) { int line = previous_token.line; std::vector<AstNodePtr> arguments; if (!check(TokenType::RPAREN)) { do { if (arguments.size() >= 255) throw std::runtime_error("函数参数不能超过255个."); arguments.push_back(expression()); } while (match({TokenType::COMMA})); } consume(TokenType::RPAREN, "参数后需要 ')'."); return std::make_shared<CallNode>(line, callee, arguments); }
    AstNodePtr finish_subscript(AstNodePtr object) { int line = previous_token.line; AstNodePtr index = expression(); consume(TokenType::RBRACKET, "下标后需要 ']'."); return std::make_shared<SubscriptNode>(line, object, index); }
    AstNodePtr list_literal() { int line = previous_token.line; std::vector<AstNodePtr> elements; if (!check(TokenType::RBRACKET)) { do { elements.push_back(expression()); } while (match({TokenType::COMMA})); } consume(TokenType::RBRACKET, "列表后需要 ']'."); return std::make_shared<ListLiteralNode>(line, elements); }
    AstNodePtr primary() { int line = current_token.line; if (match({TokenType::NUMBER})) return std::make_shared<LiteralNode>(line, std::make_shared<NumberValue>(BigNumber(previous_token.lexeme))); if (match({TokenType::STRING})) return std::make_shared<LiteralNode>(line, std::make_shared<StringValue>(previous_token.lexeme)); if (match({TokenType::HEX_LITERAL})) return std::make_shared<LiteralNode>(line, std::make_shared<BinaryValue>(previous_token.lexeme)); if (match({TokenType::LBRACKET})) return list_literal(); if (match({TokenType::IDENTIFIER})) return std::make_shared<VariableNode>(line, previous_token.lexeme); if (match({TokenType::INP})) { consume(TokenType::LPAREN, "'inp' 后需要 '('."); AstNodePtr prompt = expression(); consume(TokenType::RPAREN, "提示符后需要 ')'."); return std::make_shared<InpNode>(line, prompt); } if (match({TokenType::LPAREN})) { AstNodePtr expr = expression(); consume(TokenType::RPAREN, "表达式后需要 ')'."); return expr; } throw std::runtime_error("需要表达式."); }
};


class Interpreter {
public:
    Interpreter(); // Constructor defined after helper methods
    void interpret(const std::vector<AstNodePtr>& statements, const std::string& source) {
        this->source_lines = split_lines(source);
        try { 
            for (const auto& stmt : statements) { 
                check_timeout(stmt->line); 
                execute(stmt);
            } 
        } 
        catch (const SimPyRaiseException& ex) { 
            std::cerr << "[未捕获的异常] " << ex.value->repr() << std::endl; 
            print_stack_trace(); 
        }
        catch (const RuntimeError& error) { 
            report_runtime_error(error);
        }
    }

    void execute(const AstNodePtr& stmt) { stmt->accept(*this); }
    ValuePtr evaluate(const AstNodePtr& expr) { return expr->accept(*this); }
    void execute_block(const std::vector<AstNodePtr>& statements, std::shared_ptr<Environment> block_env) {
        std::shared_ptr<Environment> previous = this->environment;
        try { this->environment = block_env; for(const auto& stmt : statements) { check_timeout(stmt->line); execute(stmt); } } catch (...) { this->environment = previous; throw; }
        this->environment = previous;
    }
    
    // 超时检查相关
    void check_timeout(int line) {
        if (time_limit_ms > 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            if (elapsed >= time_limit_ms) {
                throw RuntimeError(line, "执行超时 (" + std::to_string(time_limit_ms) + "ms).");
            }
        }
    }
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    long long time_limit_ms;

    struct CallInfo { std::string function_name; int call_site_line; int definition_line; }; 
    std::vector<CallInfo> call_stack;
    std::shared_ptr<Environment> globals; 
    std::shared_ptr<Environment> environment;
    std::vector<std::string> source_lines; // 新增：保存源代码的所有行

private:
    void define_native_functions();
    void report_runtime_error(const RuntimeError& error) {
        std::cerr << "[运行时错误] 行 " << error.line;
        if (error.line > 0 && error.line <= (int)source_lines.size()) {
            std::cerr << ": " << trim(source_lines[error.line - 1]);
        }
        std::cerr << std::endl;
        std::cerr << " - " << error.what() << std::endl;
        
        // 打印调用栈
        if (!call_stack.empty()) {
             for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it) {
                if (it->definition_line > 0 && it->definition_line <= (int)source_lines.size()) {
                    std::cerr << " 在 行 " << it->definition_line << ": " << trim(source_lines[it->definition_line - 1]) << std::endl;
                } else { // 本机函数
                    std::cerr << " 在 " << it->function_name << " (native function called at line " << it->call_site_line << ")" << std::endl;
                }
            }
        }

        // 打印导致错误的 "值" 的来源
        if (error.context_line > 0 && error.context_line <= (int)source_lines.size()) {
            std::cerr << " 在 行 " << error.context_line << ": " << trim(source_lines[error.context_line - 1]) << std::endl;
        }
        call_stack.clear();
    }

    void print_stack_trace() { 
        if (call_stack.empty()) return; 
        std::cerr << "堆栈追踪:" << std::endl; 
        for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it) { 
             std::cerr << "  在 " << it->function_name << " (行 " << it->call_site_line << ")" << std::endl; 
        } 
        call_stack.clear(); 
    }
};

// --- AST 节点 accept 方法的实现 ---
ValuePtr LiteralNode::accept(Interpreter& visitor) { 
    auto cloned_val = value->clone();
    cloned_val->origin_line = line; // 标记值的来源
    return cloned_val;
}
ValuePtr ListLiteralNode::accept(Interpreter& visitor) { 
    std::vector<ValuePtr> evaluated_elements; 
    for (const auto& elem : elements) { 
        ValuePtr v = visitor.evaluate(elem);
        evaluated_elements.push_back(v); 
        if (dynamic_cast<CallNode*>(elem.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
    } 
    auto list_val = std::make_shared<ListValue>(evaluated_elements);
    list_val->origin_line = line;
    return list_val;
}
ValuePtr VariableNode::accept(Interpreter& visitor) { try { return visitor.environment->get(name); } catch(RuntimeError& e) { throw RuntimeError(line, e.what()); } }
ValuePtr AssignmentNode::accept(Interpreter& visitor) {
    auto val = visitor.evaluate(value);
    // 成功求值后，清理调用栈
    if (dynamic_cast<CallNode*>(value.get())) {
        if (!visitor.call_stack.empty()) visitor.call_stack.pop_back();
    }
    
    if (auto var_node = dynamic_cast<VariableNode*>(target.get())) {
        try { visitor.environment->assign(var_node->name, val); }
        catch (RuntimeError& e) { throw RuntimeError(line, e.what(), val->origin_line); }
    } else if (auto sub_node = dynamic_cast<SubscriptNode*>(target.get())) {
        try {
            auto object = visitor.evaluate(sub_node->object);
            if (dynamic_cast<CallNode*>(sub_node->object.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
            
            auto index = visitor.evaluate(sub_node->index);
            if (dynamic_cast<CallNode*>(sub_node->index.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }

            object->setSubscript(*index, val);
        } catch (const std::runtime_error& e) {
            throw RuntimeError(line, e.what(), val->origin_line);
        }
    } else {
        throw RuntimeError(line, "Invalid assignment target.");
    }
    return val;
}
ValuePtr VarDeclarationNode::accept(Interpreter& visitor) {
    ValuePtr val = std::make_shared<NullValue>();
    if (initializer) { 
        val = visitor.evaluate(initializer);
        // 注意：这里暂时不弹出调用栈，因为接下来的类型转换可能失败
    }
    
    try {
        if (keyword.type == TokenType::DEC) {
            if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { val = std::make_shared<NumberValue>(BigNumber(s_val->value)); } catch (const std::invalid_argument&) { throw RuntimeError(line, "无法将字符串 '" + s_val->value + "' 转换为数字.", val->origin_line); } }
            else if (auto b_val = dynamic_cast<BinaryValue*>(val.get())) { val = std::make_shared<NumberValue>(b_val->toBigNumber()); }
            else if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<NumberValue>(0); }
        } else if (keyword.type == TokenType::STR) {
            val = std::make_shared<StringValue>(val->toString());
        } else if (keyword.type == TokenType::BIN) {
            if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { val = std::make_shared<BinaryValue>(s_val->value); } catch(...) { throw RuntimeError(line, "无法将字符串 '" + s_val->value + "' 转换为二进制对象. 必须是 '0x...' 格式.", val->origin_line); } }
            else if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<BinaryValue>(std::vector<uint8_t>{0}); }
        } else if (keyword.type == TokenType::LIST) {
            if (!dynamic_cast<ListValue*>(val.get()) && !dynamic_cast<NullValue*>(val.get())) { throw RuntimeError(line, "只能用列表初始化列表变量.", val->origin_line); }
            if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<ListValue>(std::vector<ValuePtr>{}); }
        }
    } catch (...) {
        // 如果转换失败，异常已经被抛出，调用栈被保留。直接再次抛出。
        throw;
    }

    // 如果所有操作都成功了，现在可以安全地弹出调用栈了
    if (initializer && dynamic_cast<CallNode*>(initializer.get())) {
        if (!visitor.call_stack.empty()) visitor.call_stack.pop_back();
    }
    
    visitor.environment->define(name, val);
    return std::make_shared<NullValue>();
}
ValuePtr BinaryOpNode::accept(Interpreter& visitor) { 
    auto left_val = visitor.evaluate(left); 
    if (dynamic_cast<CallNode*>(left.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
    
    auto right_val = visitor.evaluate(right);
    if (dynamic_cast<CallNode*>(right.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }

    try {
        switch(op.type) {
            case TokenType::PLUS: return left_val->add(*right_val); case TokenType::MINUS: return left_val->subtract(*right_val);
            case TokenType::STAR: return left_val->multiply(*right_val); case TokenType::SLASH: return left_val->divide(*right_val);
            case TokenType::CARET: return left_val->power(*right_val);
            case TokenType::EQUAL_EQUAL: return std::make_shared<NumberValue>(left_val->isEqualTo(*right_val) ? 1 : 0);
            case TokenType::BANG_EQUAL: return std::make_shared<NumberValue>(!left_val->isEqualTo(*right_val) ? 1 : 0);
            case TokenType::LESS: return std::make_shared<NumberValue>(left_val->isLessThan(*right_val) ? 1 : 0);
            case TokenType::LESS_EQUAL: return std::make_shared<NumberValue>(!right_val->isLessThan(*left_val) ? 1 : 0);
            case TokenType::GREATER: return std::make_shared<NumberValue>(right_val->isLessThan(*left_val) ? 1 : 0);
            case TokenType::GREATER_EQUAL: return std::make_shared<NumberValue>(!left_val->isLessThan(*right_val) ? 1 : 0);
            default: break;
        }
    } catch(const std::runtime_error& e) { throw RuntimeError(op.line, e.what()); }
    return std::make_shared<NullValue>();
}
ValuePtr SubscriptNode::accept(Interpreter& visitor) {
    try {
        auto object = visitor.evaluate(this->object);
        if (dynamic_cast<CallNode*>(this->object.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }

        auto index = visitor.evaluate(this->index);
        if (dynamic_cast<CallNode*>(this->index.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
        
        return object->getSubscript(*index);
    } catch (const std::runtime_error& e) {
        throw RuntimeError(line, e.what());
    }
}
ValuePtr IfStatementNode::accept(Interpreter& visitor) { 
    visitor.check_timeout(line); 
    auto cond_val = visitor.evaluate(condition);
    if (dynamic_cast<CallNode*>(condition.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }

    if (cond_val->isTruthy()) { 
        visitor.execute_block(then_branch, std::make_shared<Environment>(visitor.environment)); 
    } else if (!else_branch.empty()) { 
        visitor.execute_block(else_branch, std::make_shared<Environment>(visitor.environment)); 
    } 
    return std::make_shared<NullValue>(); 
}
ValuePtr WhileStatementNode::accept(Interpreter& visitor) { 
    while(true) {
        visitor.check_timeout(line);
        auto cond_val = visitor.evaluate(condition);
        if (dynamic_cast<CallNode*>(condition.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
        if (!cond_val->isTruthy()) break;

        visitor.execute_block(do_branch, std::make_shared<Environment>(visitor.environment)); 
    }
    if (!finally_branch.empty()) { visitor.execute_block(finally_branch, std::make_shared<Environment>(visitor.environment)); } 
    return std::make_shared<NullValue>(); 
}
ValuePtr AwaitStatementNode::accept(Interpreter& visitor) {
    while(true) {
        visitor.check_timeout(line);
        auto cond_val = visitor.evaluate(condition);
        // 在 await 中，我们希望每次循环都清理调用栈
        if (dynamic_cast<CallNode*>(condition.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
        if (cond_val->isTruthy()) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    visitor.execute_block(then_branch, std::make_shared<Environment>(visitor.environment));
    return std::make_shared<NullValue>();
}
ValuePtr SayNode::accept(Interpreter& visitor) { 
    auto val = visitor.evaluate(expression); 
    if (dynamic_cast<CallNode*>(expression.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }

    std::cout << val->toString() << std::endl; 
    return std::make_shared<NullValue>(); 
}
ValuePtr InpNode::accept(Interpreter& visitor) { 
    auto prompt = visitor.evaluate(expression); 
    if (dynamic_cast<CallNode*>(expression.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
    
    std::cout << prompt->toString(); 
    std::string input; 
    std::getline(std::cin, input); 
    return std::make_shared<StringValue>(input); 
}
ValuePtr FunctionDefNode::accept(Interpreter& visitor) { 
    auto function = std::make_shared<Function>(name, params, body, visitor.environment, line); 
    visitor.environment->define(name, std::make_shared<FunctionValue>(function)); 
    return std::make_shared<NullValue>(); 
}
ValuePtr CallNode::accept(Interpreter& visitor) {
    visitor.check_timeout(line);
    
    // swap 是一个特殊情况，直接处理
    if (auto callee_var = dynamic_cast<VariableNode*>(callee.get())) {
        if (callee_var->name == "swap") {
            if (arguments.size() != 2) throw RuntimeError(line, "swap()需要2个变量名作为参数.");
            auto arg1 = dynamic_cast<VariableNode*>(arguments[0].get());
            auto arg2 = dynamic_cast<VariableNode*>(arguments[1].get());
            if (!arg1 || !arg2) throw RuntimeError(line, "swap()的参数必须是变量名.");

            try {
                std::string name1 = arg1->name; std::string name2 = arg2->name;
                ValuePtr val1 = visitor.environment->get(name1); ValuePtr val2 = visitor.environment->get(name2);
                ValuePtr type1_str_val = visitor.environment->get_type(name1); std::string type1_str = dynamic_cast<StringValue*>(type1_str_val.get())->value;
                ValuePtr type2_str_val = visitor.environment->get_type(name2); std::string type2_str = dynamic_cast<StringValue*>(type2_str_val.get())->value;
                ValuePtr new_val1;
                if(type1_str == "dec") { try { new_val1 = std::make_shared<NumberValue>(BigNumber(val2->toString())); } catch(...) { new_val1 = std::make_shared<StringValue>(val2->toString()); visitor.environment->assign(name1, std::make_shared<StringValue>(""));} }
                else if (type1_str == "str") { new_val1 = std::make_shared<StringValue>(val2->toString()); }
                else if (type1_str == "bin") { try { new_val1 = std::make_shared<BinaryValue>(val2->toString()); } catch(...) { new_val1 = std::make_shared<StringValue>(val2->toString()); visitor.environment->assign(name1, std::make_shared<StringValue>(""));} }
                else { new_val1 = val2->clone(); }
                ValuePtr new_val2;
                if(type2_str == "dec") { try { new_val2 = std::make_shared<NumberValue>(BigNumber(val1->toString())); } catch(...) { new_val2 = std::make_shared<StringValue>(val1->toString()); visitor.environment->assign(name2, std::make_shared<StringValue>(""));} }
                else if (type2_str == "str") { new_val2 = std::make_shared<StringValue>(val1->toString()); }
                else if (type2_str == "bin") { try { new_val2 = std::make_shared<BinaryValue>(val1->toString()); } catch(...) { new_val2 = std::make_shared<StringValue>(val1->toString()); visitor.environment->assign(name2, std::make_shared<StringValue>(""));} }
                else { new_val2 = val1->clone(); }
                visitor.environment->assign(name1, new_val1);
                visitor.environment->assign(name2, new_val2);
            } catch(const RuntimeError& e) {
                throw RuntimeError(line, e.what());
            }
            return std::make_shared<NullValue>();
        }
    }
    
    auto callee_val = visitor.evaluate(callee);
    if (dynamic_cast<CallNode*>(callee.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }

    std::vector<ValuePtr> arg_values; 
    for(const auto& arg_expr : arguments) { 
        arg_values.push_back(visitor.evaluate(arg_expr)); 
        if (dynamic_cast<CallNode*>(arg_expr.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
    }

    if (auto native_fn = dynamic_cast<NativeFnValue*>(callee_val.get())) {
        visitor.call_stack.push_back({native_fn->name, line, 0});
        try {
            ValuePtr result = native_fn->call(arg_values);
            // 本机函数调用在这里弹出
            if (!visitor.call_stack.empty()) visitor.call_stack.pop_back();
            return result;
        } catch(const std::runtime_error& e) {
            if (!visitor.call_stack.empty()) visitor.call_stack.pop_back();
            throw RuntimeError(line, e.what());
        }
    }

    auto func_val = dynamic_cast<FunctionValue*>(callee_val.get());
    if (!func_val) throw RuntimeError(line, "只能调用函数.", callee_val->origin_line);
    auto function = func_val->value;
    
    if (arg_values.size() != function->params.size()) { std::stringstream ss; ss << "函数 '" << function->name << "' 需要 " << function->params.size() << " 个参数, 但收到了 " << arg_values.size() << " 个."; throw RuntimeError(line, ss.str()); }
    
    auto call_env = std::make_shared<Environment>(function->closure);
    for (size_t i = 0; i < function->params.size(); ++i) { call_env->define(function->params[i], arg_values[i]); }
    
    // 推入调用栈，但不在此处弹出
    visitor.call_stack.push_back({function->name, line, function->definition_line}); 
    
    ValuePtr return_val = std::make_shared<NullValue>();
    try { 
        visitor.execute_block(function->body, call_env); 
    } catch (const ReturnValueException& rv) { 
        return_val = rv.value; 
    }
    
    // 返回值，调用者将负责弹出栈
    return return_val;
}

ValuePtr ReturnNode::accept(Interpreter& visitor) { 
    ValuePtr val = std::make_shared<NullValue>(); 
    if (value) { 
        val = visitor.evaluate(value); 
        if (dynamic_cast<CallNode*>(this->value.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
    } 
    val->origin_line = line; // 标记返回值的来源行
    throw ReturnValueException(val); 
}
ValuePtr RaiseNode::accept(Interpreter& visitor) { 
    auto value_to_raise = visitor.evaluate(expression); 
    if (dynamic_cast<CallNode*>(expression.get())) { if (!visitor.call_stack.empty()) visitor.call_stack.pop_back(); }
    throw SimPyRaiseException(value_to_raise); 
}
ValuePtr TryCatchNode::accept(Interpreter& visitor) {
    std::unique_ptr<std::exception_ptr> captured_exception = nullptr;
    try {
        try {
            visitor.execute_block(try_branch, std::make_shared<Environment>(visitor.environment));
        } catch (const SimPyRaiseException& ex) {
            auto catch_env = std::make_shared<Environment>(visitor.environment);
            catch_env->define(exception_var, ex.value);
            visitor.execute_block(catch_branch, catch_env);
        } catch (const RuntimeError& ex) {
            auto exception_obj = std::make_shared<ExceptionValue>(std::make_shared<StringValue>(ex.what()));
            auto catch_env = std::make_shared<Environment>(visitor.environment);
            catch_env->define(exception_var, exception_obj);
            visitor.execute_block(catch_branch, catch_env);
        }
    } catch (...) {
        captured_exception.reset(new std::exception_ptr(std::current_exception()));
    }

    if (!finally_branch.empty()) {
        visitor.execute_block(finally_branch, std::make_shared<Environment>(visitor.environment));
    }

    if (captured_exception) {
        std::rethrow_exception(*captured_exception);
    }
    return std::make_shared<NullValue>();
}


// --- Interpreter Constructor and Native Functions ---
Interpreter::Interpreter() : globals(std::make_shared<Environment>()), environment(globals), time_limit_ms(0) {
    define_native_functions();
}

void Interpreter::define_native_functions() {
    #define REQUIRE_ARGS(name, count) if(args.size() != count) throw std::runtime_error(name "() 需要 " #count " 个参数.");
    #define REQUIRE_MIN_ARGS(name, count) if(args.size() < count) throw std::runtime_error(name "() 至少需要 " #count " 个参数.");
    #define GET_NUM(val, var_name) auto var_name = dynamic_cast<NumberValue*>(val.get()); if(!var_name) throw std::runtime_error("参数必须是数字.");
    #define GET_LIST(val, var_name) auto var_name = dynamic_cast<ListValue*>(val.get()); if(!var_name) throw std::runtime_error("参数必须是列表.");

    // --- Exception Factory ---
    globals->define("Exception", std::make_shared<NativeFnValue>("Exception", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("Exception", 1);
        return std::make_shared<ExceptionValue>(args[0]);
    }));

    // --- Math Functions ---
    globals->define("abs", std::make_shared<NativeFnValue>("abs", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("abs", 1);
        GET_NUM(args[0], num_val);
        return std::make_shared<NumberValue>(num_val->value.abs());
    }));

    globals->define("rt", std::make_shared<NativeFnValue>("rt", [](const std::vector<ValuePtr>& args){
        if (args.size() < 1 || args.size() > 2) throw std::runtime_error("rt() 需要 1 或 2 个参数.");
        GET_NUM(args[0], num_val);
        BigNumber n = 2;
        if(args.size() == 2) {
            GET_NUM(args[1], n_val);
            n = n_val->value;
        }
        return std::make_shared<NumberValue>(BigNumber::root(num_val->value, n));
    }));
    
    // --- List Functions ---
    globals->define("sort", std::make_shared<NativeFnValue>("sort", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("sort", 1);
        GET_LIST(args[0], list_val);
        auto new_list = std::make_shared<ListValue>(list_val->elements); // Create a copy
        std::sort(new_list->elements.begin(), new_list->elements.end(), 
            [](const ValuePtr& a, const ValuePtr& b){
                try { return a->isLessThan(*b); } catch(...) { return false; }
            });
        return new_list;
    }));
    
    globals->define("setify", std::make_shared<NativeFnValue>("setify", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("setify", 1);
        GET_LIST(args[0], list_val);
        std::vector<ValuePtr> unique_elements;
        for(const auto& elem : list_val->elements) {
            bool found = false;
            for(const auto& unique_elem : unique_elements) {
                if(elem->isEqualTo(*unique_elem)) {
                    found = true;
                    break;
                }
            }
            if(!found) unique_elements.push_back(elem);
        }
        return std::make_shared<ListValue>(unique_elements);
    }));

    // --- Aggregate Functions ---
    auto min_max_logic = [](const std::vector<ValuePtr>& args, bool is_max) {
        if (args.empty()) throw std::runtime_error("min/max 需要至少一个参数.");
        const std::vector<ValuePtr>* values_to_compare;
        std::vector<ValuePtr> temp_list;
        if (args.size() == 1 && dynamic_cast<ListValue*>(args[0].get())) {
             values_to_compare = &dynamic_cast<ListValue*>(args[0].get())->elements;
        } else {
            temp_list = args;
            values_to_compare = &temp_list;
        }
        if (values_to_compare->empty()) throw std::runtime_error("无法从空列表/参数中查找 min/max.");
        
        ValuePtr extreme = (*values_to_compare)[0];
        for (size_t i = 1; i < values_to_compare->size(); ++i) {
            ValuePtr current = (*values_to_compare)[i];
            try {
                if (is_max) {
                    if (extreme->isLessThan(*current)) extreme = current;
                } else {
                    if (current->isLessThan(*extreme)) extreme = current;
                }
            } catch (const std::runtime_error&) {
                throw std::runtime_error("min/max 的所有参数必须是可比较类型 (数字或字符串).");
            }
        }
        return extreme;
    };
    globals->define("max", std::make_shared<NativeFnValue>("max", [min_max_logic](const std::vector<ValuePtr>& args){ return min_max_logic(args, true); }));
    globals->define("min", std::make_shared<NativeFnValue>("min", [min_max_logic](const std::vector<ValuePtr>& args){ return min_max_logic(args, false); }));

    // --- System/Utility Functions ---
    globals->define("countdown", std::make_shared<NativeFnValue>("countdown", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("countdown", 1);
        GET_NUM(args[0], sec_val);
        auto end_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(sec_val->value.toLongLong() * 1000);
        auto timer_fn_body = [end_time](const std::vector<ValuePtr>& inner_args) -> ValuePtr {
            if (!inner_args.empty()) throw std::runtime_error("计时器函数不接受参数.");
            auto now = std::chrono::high_resolution_clock::now();
            return std::make_shared<NumberValue>(now >= end_time ? 1 : 0);
        };
        auto native_timer_fn = std::make_shared<NativeFnValue>("timer", timer_fn_body);
        return native_timer_fn;
    }));
    
    globals->define("hash", std::make_shared<NativeFnValue>("hash", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("hash", 2);
        std::string data_str = args[0]->toString();
        GET_NUM(args[1], key_val);
        long long key = key_val->value.toLongLong();
        unsigned long long hash_val = 5381;
        for(char c : data_str) {
            hash_val = ((hash_val << 5) + hash_val) + c;
        }
        hash_val ^= key;
        return std::make_shared<NumberValue>(BigNumber((long long)hash_val));
    }));
    
    globals->define("sin", std::make_shared<NativeFnValue>("sin", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("sin", 1); GET_NUM(args[0], x); return std::make_shared<NumberValue>(BigNumber(std::to_string(sin(x->value.toLongLong()))));
    }));
    globals->define("cos", std::make_shared<NativeFnValue>("cos", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("cos", 1); GET_NUM(args[0], x); return std::make_shared<NumberValue>(BigNumber(std::to_string(cos(x->value.toLongLong()))));
    }));
    globals->define("tan", std::make_shared<NativeFnValue>("tan", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("tan", 1); GET_NUM(args[0], x); return std::make_shared<NumberValue>(BigNumber(std::to_string(tan(x->value.toLongLong()))));
    }));
    globals->define("log", std::make_shared<NativeFnValue>("log", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("log", 1); GET_NUM(args[0], x); if(x->value <= BigNumber(0)) throw std::runtime_error("log()的参数必须为正.");
        return std::make_shared<NumberValue>(BigNumber(std::to_string(log(x->value.toLongLong()))));
    }));
}
// --- 辅助函数定义 ---
std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r";
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return "";
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

int main(int argc, char* argv[]) {

    std::string simpy_code_to_run = R"RAW(

WRITE_SRC_CODE_HERE

)RAW";

    Interpreter interpreter;
    Parser parser(simpy_code_to_run);
    
    auto statements = parser.parse();

    if (parser.has_error()) {
        std::cerr << "Execution aborted due to parsing errors." << std::endl;
        return 1;
    }
    
    interpreter.interpret(statements, simpy_code_to_run);

    return 0;
}
