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
#include <cstdlib>
#include <cstdio>
#include <unordered_set>
#include "BigNumber.hpp"
#include "Tense.hpp"    // Matrix support
#include "File.hpp"     // File operation support
#include "msg_jp_special1.hpp" // --- Import all string constants ---

// --- Global DEBUG Switch ----
// Be careful, do not use in release builds.
// Enabling this may slow down I/O due to printing large amounts of debug information.
constexpr bool DEBUG = true;

// --- Forward Declarations ---
struct Value; struct Function; class Interpreter; class Environment; class NumberValue; class BinaryValue; class StringValue; class ListValue; class NativeFnValue; class ExceptionValue;
struct Class; struct Instance;
using ValuePtr = std::shared_ptr<Value>;
using ClassPtr = std::shared_ptr<Class>;
using InstancePtr = std::shared_ptr<Instance>;

// --- Base Value Class and Derived Classes ---
struct Value {
    virtual ~Value() {}
    virtual std::string toString() const = 0;
    virtual std::string repr() const = 0;
    virtual bool isTruthy() const = 0;
    virtual ValuePtr clone() const = 0;
    virtual ValuePtr add(const Value& other) const; virtual ValuePtr subtract(const Value& other) const; virtual ValuePtr multiply(const Value& other) const; virtual ValuePtr divide(const Value& other) const; virtual ValuePtr power(const Value& other) const;
    virtual bool isEqualTo(const Value& other) const; virtual bool isLessThan(const Value& other) const;
    virtual ValuePtr getSubscript(const Value& index) const; virtual void setSubscript(const Value& index, ValuePtr value);
};
class NullValue : public Value {
public:
    std::string toString() const override { return "null"; }
    std::string repr() const override { std::stringstream ss; ss << "<NullObject at " << static_cast<const void*>(this) << ">"; return ss.str(); }
    bool isTruthy() const override { return false; }
    ValuePtr clone() const override { return std::make_shared<NullValue>(); }
    bool isEqualTo(const Value& other) const override { return dynamic_cast<const NullValue*>(&other) != nullptr; }
};
class NumberValue : public Value {
public:
    BigNumber value;
    NumberValue(const BigNumber& n) : value(n) {}
    std::string toString() const override { return value.toString(); }
    std::string repr() const override { return value.toString(); }
    bool isTruthy() const override { return value != BigNumber(0); }
    ValuePtr clone() const override { return std::make_shared<NumberValue>(value); }
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
    ValuePtr clone() const override { return std::make_shared<BinaryValue>(value); }
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
    ValuePtr clone() const override { return std::make_shared<StringValue>(value); }
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
    ValuePtr clone() const override { return std::make_shared<ListValue>(elements); }
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
    ValuePtr clone() const override { return std::make_shared<FunctionValue>(value); }
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
    ValuePtr clone() const override { return std::make_shared<NativeFnValue>(name, fn); }
    ValuePtr call(const std::vector<ValuePtr>& args) const { return fn(args); }
};
// Represents a method that has been bound to an instance.
class BoundMethodValue : public Value {
public:
    InstancePtr instance; // The 'this' object
    std::shared_ptr<Function> method; // The function code for the method

    BoundMethodValue(InstancePtr inst, std::shared_ptr<Function> m) : instance(inst), method(m) {}

    // --- Implementation moved out, only declaration remains ---
    std::string toString() const override;
    std::string repr() const override;

    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return std::make_shared<BoundMethodValue>(instance, method); }
};
class ExceptionValue : public Value {
public:
    ValuePtr payload;
    ExceptionValue(ValuePtr p) : payload(p) {}
    std::string toString() const override { return "<Exception: " + payload->toString() + ">"; }
    std::string repr() const override { std::stringstream ss; ss << "<ExceptionObject at " << static_cast<const void*>(this) << " payload=" << payload->repr() << ">"; return ss.str(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return std::make_shared<ExceptionValue>(payload->clone()); }
    bool isEqualTo(const Value& other) const override { if (const ExceptionValue* o = dynamic_cast<const ExceptionValue*>(&other)) { return payload->isEqualTo(*(o->payload)); } return false; }
};

// --- Operator and Conversion Implementations ---
ValuePtr Value::add(const Value&) const { throw std::runtime_error(PyRiteMessages::ERROR_UNSUPPORTED_OPERAND_ADD); }
ValuePtr Value::subtract(const Value&) const { throw std::runtime_error(PyRiteMessages::ERROR_UNSUPPORTED_OPERAND_SUB); }
ValuePtr Value::multiply(const Value&) const { throw std::runtime_error(PyRiteMessages::ERROR_UNSUPPORTED_OPERAND_MUL); }
ValuePtr Value::divide(const Value&) const { throw std::runtime_error(PyRiteMessages::ERROR_UNSUPPORTED_OPERAND_DIV); }
ValuePtr Value::power(const Value&) const { throw std::runtime_error(PyRiteMessages::ERROR_UNSUPPORTED_OPERAND_POW); }
bool Value::isEqualTo(const Value&) const { return false; }
bool Value::isLessThan(const Value&) const { throw std::runtime_error(PyRiteMessages::ERROR_UNSUPPORTED_COMPARISON); }
ValuePtr Value::getSubscript(const Value&) const { throw std::runtime_error(PyRiteMessages::ERROR_OBJECT_NOT_SUBSCRIPTABLE); }
void Value::setSubscript(const Value&, ValuePtr) { throw std::runtime_error(PyRiteMessages::ERROR_OBJECT_ITEM_ASSIGNMENT_UNSUPPORTED); }
ValuePtr NumberValue::add(const Value& other) const {
    if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value + o->value);
    if (const BinaryValue* o = dynamic_cast<const BinaryValue*>(&other)) return std::make_shared<NumberValue>(this->value + o->toBigNumber());
    return std::make_shared<StringValue>(this->toString() + other.toString());
}
ValuePtr NumberValue::subtract(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value - o->value); return Value::subtract(other); }
ValuePtr NumberValue::multiply(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value * o->value); return Value::multiply(other); }
ValuePtr NumberValue::divide(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value / o->value); return Value::divide(other); }
ValuePtr NumberValue::power(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->value ^ o->value); return Value::power(other); }
bool NumberValue::isEqualTo(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return this->value == o->value; if (const BinaryValue* o = dynamic_cast<const BinaryValue*>(&other)) return this->value == o->toBigNumber(); return false; }
bool NumberValue::isLessThan(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return this->value < o->value; return Value::isLessThan(other); }
BinaryValue::BinaryValue(const std::string& hex_str) {
    if (hex_str.rfind("0x", 0) != 0) throw std::invalid_argument(PyRiteMessages::ERROR_HEX_STRING_PREFIX);
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
    if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return std::make_shared<NumberValue>(this->toBigNumber() + o->value);
    return std::make_shared<StringValue>(this->toString() + other.toString());
}
bool BinaryValue::isEqualTo(const Value& other) const {
    if (const BinaryValue* o = dynamic_cast<const BinaryValue*>(&other)) return this->value == o->value;
    if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return this->toBigNumber() == o->value;
    return false;
}
ValuePtr StringValue::add(const Value& other) const { return std::make_shared<StringValue>(this->value + other.toString()); }
bool StringValue::isEqualTo(const Value& other) const { if (const StringValue* o = dynamic_cast<const StringValue*>(&other)) return this->value == o->value; return false; }
bool StringValue::isLessThan(const Value& other) const { if (const StringValue* o = dynamic_cast<const StringValue*>(&other)) return this->value < o->value; return Value::isLessThan(other); }
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
    if (const ListValue* o = dynamic_cast<const ListValue*>(&other)) {
        auto new_elements = this->elements;
        new_elements.insert(new_elements.end(), o->elements.begin(), o->elements.end());
        return std::make_shared<ListValue>(new_elements);
    }
    return Value::add(other);
}
ValuePtr ListValue::multiply(const Value& other) const {
    if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) {
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
            throw std::runtime_error(PyRiteMessages::ERROR_LIST_REPEAT_COUNT_INTEGER);
        }
    }
    return Value::multiply(other);
}
bool ListValue::isEqualTo(const Value& other) const {
    const ListValue* o = dynamic_cast<const ListValue*>(&other);
    if (!o || this->elements.size() != o->elements.size()) return false;
    for (size_t i = 0; i < this->elements.size(); ++i) {
        if (!this->elements[i]->isEqualTo(*o->elements[i])) return false;
    }
    return true;
}
ValuePtr ListValue::getSubscript(const Value& index) const {
    const NumberValue* num_val = dynamic_cast<const NumberValue*>(&index);
    if (!num_val) throw std::runtime_error(PyRiteMessages::ERROR_LIST_INDEX_MUST_BE_NUMBER);
    try {
        long long i = num_val->value.toLongLong();
        long long size = elements.size();
        if (i < 0) i += size;
        if (i >= 0 && i < size) {
            return elements[i];
        }
        throw std::runtime_error(PyRiteMessages::ERROR_LIST_INDEX_OUT_OF_RANGE);
    } catch (...) {
        throw std::runtime_error(PyRiteMessages::ERROR_INVALID_LIST_INDEX);
    }
}
void ListValue::setSubscript(const Value& index, ValuePtr value) {
    const NumberValue* num_val = dynamic_cast<const NumberValue*>(&index);
    if (!num_val) throw std::runtime_error(PyRiteMessages::ERROR_LIST_INDEX_MUST_BE_NUMBER);
     try {
        long long i = num_val->value.toLongLong();
        long long size = elements.size();
        if (i < 0) i += size;
        if (i >= 0 && i < size) {
            elements[i] = value;
            return;
        }
        throw std::runtime_error(PyRiteMessages::ERROR_LIST_INDEX_OUT_OF_RANGE);
    } catch (...) {
        throw std::runtime_error(PyRiteMessages::ERROR_INVALID_LIST_INDEX);
    }
}
// --- Function Parameter Definition ---
// 1. Move TokenType enum class definition here to ensure ParameterDefinition can see it
enum class TokenType {
    DEC, STR, BIN, LIST, ANY, TENSE,  // Added ANY and TENSE
    IF, THEN, ELSE, ENDIF, WHILE, DO, FINALLY, ENDWHILE, DEF, ENDDEF, RETURN, SAY, ASK, MARK, JUMP, HALT, RUN,
    TRY, CATCH, ENDTRY, RAISE,
    AWAIT, ENDAWAIT,
    INS, CONTAINS, ENDINS,
    IDENTIFIER, NUMBER, STRING, HEX_LITERAL,
    EQUAL, EQUAL_EQUAL, BANG_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    PLUS, MINUS, STAR, SLASH, LPAREN, RPAREN, COMMA, CARET,
    LBRACKET, RBRACKET,
    DOT, // For accessing instance properties/methods
    NULL_LITERAL,  // Added null literal
    END_OF_FILE, UNKNOWN
};
// 2. Modify ParameterDefinition struct definition
struct ParameterDefinition {
    TokenType type_keyword; // Type keyword (TokenType::DEC, STR, BIN, LIST)
    std::string name;       // Parameter name
    ValuePtr default_value; // Default value (can be nullptr)
    bool has_default;       // Flag for whether it has a default value
    // Constructor
    ParameterDefinition(TokenType tk, const std::string& n, ValuePtr dv = nullptr)
        : type_keyword(tk), name(n), default_value(dv), has_default(dv != nullptr) {}
};
// --- AST Nodes and Related Structures ---
struct AstNode; class Environment; using AstNodePtr = std::shared_ptr<AstNode>;
// 3. Modify Function struct to use ParameterDefinition
struct Function {
    std::string name;
    std::vector<ParameterDefinition> params; // Use a list of ParameterDefinition
    std::vector<AstNodePtr> body;
    std::shared_ptr<Environment> closure;
    Function(const std::string& n, const std::vector<ParameterDefinition>& p, const std::vector<AstNodePtr>& b, const std::shared_ptr<Environment>& c)
        : name(n), params(p), body(b), closure(c) {}
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
// --- C++ Exception Definitions ---
class RuntimeError : public std::runtime_error { public: int line; RuntimeError(int l, const std::string& msg) : std::runtime_error(msg), line(l) {} };
class ReturnValueException : public std::runtime_error { public: ValuePtr value; ReturnValueException(ValuePtr v) : std::runtime_error(""), value(v) {} };
class PyRiteRaiseException : public std::runtime_error { public: ValuePtr value; PyRiteRaiseException(ValuePtr v) : std::runtime_error(""), value(v) {} };

// --- Environment ---
class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment(std::shared_ptr<Environment> enc = nullptr) : enclosing(enc) {}
    void define(const std::string& name, ValuePtr value) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_ENV_DEFINE << name << PyRiteMessages::DEBUG_ENV_IN_ENV << this << PyRiteMessages::DEBUG_ENV_AS << value->repr() << std::endl;
        values[name] = value;
    }
    void assign(const std::string& name, ValuePtr value) {
        if (values.count(name)) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_ENV_ASSIGN << name << PyRiteMessages::DEBUG_ENV_IN_ENV << this << PyRiteMessages::DEBUG_ENV_VALUE << value->repr() << std::endl;
            values[name] = value;
            return;
        }
        if (enclosing) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_ENV_ASSIGN_FAIL << this << PyRiteMessages::DEBUG_ENV_TRY_ENCLOSING << enclosing.get() << std::endl;
            enclosing->assign(name, value);
            return;
        }
        throw RuntimeError(0, std::string(PyRiteMessages::RUNTIME_ERROR_UNDEFINED_VARIABLE_PREFIX) + name + PyRiteMessages::RUNTIME_ERROR_UNDEFINED_VARIABLE_SUFFIX);
    }
    ValuePtr get(const std::string& name) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_ENV_GET << name << PyRiteMessages::DEBUG_ENV_FROM_ENV << this << std::endl;
        if (values.count(name)) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_ENV_FOUND << name << "' = " << values.at(name)->repr() << std::endl;
            return values[name];
        }
        if (enclosing) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_ENV_GET_FAIL << this << PyRiteMessages::DEBUG_ENV_TRY_ENCLOSING << enclosing.get() << std::endl;
            return enclosing->get(name);
        }
        throw RuntimeError(0, std::string(PyRiteMessages::RUNTIME_ERROR_UNDEFINED_VARIABLE_PREFIX) + name + PyRiteMessages::RUNTIME_ERROR_UNDEFINED_VARIABLE_SUFFIX);
    }
    
    // Implementation for get_type moved out, only declaration remains
    ValuePtr get_type(const std::string& name); 

private:
    std::shared_ptr<Environment> enclosing; std::map<std::string, ValuePtr> values;
};

// --- Helper function for type checking (forward declaration) ---
bool is_type_compatible(TokenType expected_type, const ValuePtr& value);
std::string token_type_to_string(TokenType type);
// --- Class and Instance Definitions (moved after Environment) ---
struct Class : public Value {
    std::string name;
    std::vector<ParameterDefinition> fields; // Field definitions with types and defaults
    std::map<std::string, std::shared_ptr<Function>> methods; // Method definitions
    std::shared_ptr<Environment> closure; // Environment where the class was defined
    Class(const std::string& n, const std::vector<ParameterDefinition>& f, const std::map<std::string, std::shared_ptr<Function>>& m, const std::shared_ptr<Environment>& c)
        : name(n), fields(f), methods(m), closure(c) {}
    std::string toString() const override { return "<class " + name + ">"; }
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return std::make_shared<Class>(*this); } // Shallow copy is usually fine for classes
    bool isEqualTo(const Value& other) const override {
        if (const Class* o = dynamic_cast<const Class*>(&other)) {
            return this->name == o->name; // Simple comparison by name
        }
        return false;
    }
};
struct Instance : public Value, public std::enable_shared_from_this<Instance> {
public: // Add public specifier
    ClassPtr klass;
    std::shared_ptr<Environment> instance_env; // Holds instance fields
    Instance(ClassPtr k) : klass(k) {
        // Create a new environment for this instance, enclosed in the class's closure
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_CREATING << k->name << PyRiteMessages::DEBUG_INSTANCE_NEW_ENV << k->closure.get() << std::endl;
        instance_env = std::make_shared<Environment>(klass->closure);
        // Initialize fields with their default values
        for (const auto& field_def : klass->fields) {
            ValuePtr default_val = field_def.default_value ? field_def.default_value->clone() : std::make_shared<NullValue>();
             if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_INIT_FIELD << field_def.name << PyRiteMessages::DEBUG_INSTANCE_WITH_DEFAULT << default_val->repr() << std::endl;
            instance_env->define(field_def.name, default_val);
        }
    }
    std::string toString() const override {
        return "<" + klass->name + " instance>";
    }
    std::string repr() const override {
        return toString(); // Could be enhanced to show fields
    }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override {
        return std::make_shared<Instance>(klass);
    }
    // Get a field or method
    ValuePtr get(const std::string& name) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_GET_PROP << name << PyRiteMessages::DEBUG_INSTANCE_FROM << klass->name << "'." << std::endl;
        // 1. Check instance fields
        try {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_CHECK_FIELDS << std::endl;
            return instance_env->get(name);
        } catch (const RuntimeError&) {
            // 2. Check class methods
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_PROP_NOT_IN_FIELDS << name << PyRiteMessages::DEBUG_INSTANCE_CHECK_METHODS << std::endl;
            auto it = klass->methods.find(name);
            if (it != klass->methods.end()) {
                if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_FOUND_METHOD << name << PyRiteMessages::DEBUG_INSTANCE_CREATING_BOUND_METHOD << std::endl;
                // Found a method. Return a new BoundMethodValue which pairs the instance ('this')
                // with the method's function data. The interpreter will handle calling it.
                return std::make_shared<BoundMethodValue>(this->shared_from_this(), it->second);
            }
        }
        throw std::runtime_error(std::string(PyRiteMessages::RUNTIME_ERROR_UNDEFINED_PROPERTY_PREFIX) + name + PyRiteMessages::RUNTIME_ERROR_UNDEFINED_PROPERTY_SUFFIX);
    }
    // Set a field
    void set(const std::string& name, ValuePtr value) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_SET_PROP << name << PyRiteMessages::DEBUG_INSTANCE_FOR << klass->name << PyRiteMessages::DEBUG_INSTANCE_TO_VALUE << value->repr() << "." << std::endl;
        // Check if the field is defined in the class
        bool field_found = false;
        for (const auto& field_def : klass->fields) {
            if (field_def.name == name) {
                field_found = true;
                // Type check
                if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_FOUND_FIELD << name << PyRiteMessages::DEBUG_INSTANCE_TYPE_CHECKING << token_type_to_string(field_def.type_keyword) << std::endl;
                if (!is_type_compatible(field_def.type_keyword, value)) { // Use helper function
                     std::stringstream ss;
                     ss << PyRiteMessages::RUNTIME_ERROR_FIELD_TYPE_MISMATCH_PREFIX << name << PyRiteMessages::RUNTIME_ERROR_FIELD_TYPE_MISMATCH_EXPECTED << token_type_to_string(field_def.type_keyword) // Use helper function
                        << PyRiteMessages::RUNTIME_ERROR_FIELD_TYPE_MISMATCH_GOT;
                     if (dynamic_cast<NumberValue*>(value.get())) ss << "dec";
                     else if (dynamic_cast<StringValue*>(value.get())) ss << "str";
                     else if (dynamic_cast<BinaryValue*>(value.get())) ss << "bin";
                     else if (dynamic_cast<ListValue*>(value.get())) ss << "list";
                     else ss << "unknown";
                     ss << PyRiteMessages::RUNTIME_ERROR_FIELD_TYPE_MISMATCH_SUFFIX;
                     throw std::runtime_error(ss.str());
                }
                break;
            }
        }
        if (!field_found) {
            throw std::runtime_error(std::string(PyRiteMessages::RUNTIME_ERROR_UNDEFINED_FIELD_PREFIX) + name + PyRiteMessages::RUNTIME_ERROR_UNDEFINED_FIELD_SUFFIX);
        }
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INSTANCE_TYPE_CHECK_OK << std::endl;
        instance_env->define(name, value); // This will overwrite if it exists
    }
};

// --- Environment::get_type Implementation (after Class and Instance definitions) ---
ValuePtr Environment::get_type(const std::string& name) {
    ValuePtr val = get(name);
    if (dynamic_cast<NumberValue*>(val.get())) return std::make_shared<StringValue>("dec");
    if (dynamic_cast<StringValue*>(val.get())) return std::make_shared<StringValue>("str");
    if (dynamic_cast<BinaryValue*>(val.get())) return std::make_shared<StringValue>("bin");
    if (dynamic_cast<ListValue*>(val.get())) return std::make_shared<StringValue>("list");
    if (dynamic_cast<ExceptionValue*>(val.get())) return std::make_shared<StringValue>("exception");
    // At this point, Class and Instance are complete types, so dynamic_cast can be used safely
    if (dynamic_cast<Class*>(val.get())) return std::make_shared<StringValue>("class");
    if (dynamic_cast<Instance*>(val.get())) return std::make_shared<StringValue>("instance");
    return std::make_shared<StringValue>("unknown");
}

// --- BoundMethodValue Method Implementations (after Instance and Function definitions) ---
std::string BoundMethodValue::toString() const {
    // At this point, Instance and Function are complete types, so their members can be accessed safely
    return "<bound method " + instance->klass->name + "." + method->name + ">";
}

std::string BoundMethodValue::repr() const {
    return toString();
}

// --- Tokenizer ---
struct Token { TokenType type; std::string lexeme; int line; };
class Tokenizer {
public:
    Tokenizer(const std::string& source) : source(source), start(0), current(0), line(1) {
        keywords["any"] = TokenType::ANY;  // Add 'any' keyword
        keywords["tense"] = TokenType::TENSE;  // Add 'tense' keyword
        keywords["nul"] = TokenType::NULL_LITERAL;  // Add 'nul' keyword
        keywords["dec"] = TokenType::DEC; keywords["str"] = TokenType::STR; keywords["bin"] = TokenType::BIN; keywords["list"] = TokenType::LIST;
        keywords["if"] = TokenType::IF; keywords["then"] = TokenType::THEN; keywords["else"] = TokenType::ELSE; keywords["endif"] = TokenType::ENDIF; 
        keywords["while"] = TokenType::WHILE; keywords["do"] = TokenType::DO; keywords["finally"] = TokenType::FINALLY; keywords["endwhile"] = TokenType::ENDWHILE; 
        keywords["def"] = TokenType::DEF; keywords["enddef"] = TokenType::ENDDEF; keywords["return"] = TokenType::RETURN; keywords["say"] = TokenType::SAY; 
        keywords["ask"] = TokenType::ASK; keywords["mark"] = TokenType::MARK; keywords["jump"] = TokenType::JUMP; keywords["halt"] = TokenType::HALT; 
        keywords["run"] = TokenType::RUN;
        keywords["await"] = TokenType::AWAIT; keywords["endawait"] = TokenType::ENDAWAIT;
        keywords["try"] = TokenType::TRY; keywords["catch"] = TokenType::CATCH; keywords["endtry"] = TokenType::ENDTRY; keywords["raise"] = TokenType::RAISE;
        keywords["ins"] = TokenType::INS; keywords["contains"] = TokenType::CONTAINS; keywords["endins"] = TokenType::ENDINS; // Register new keywords
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
            case '.': return make_token(TokenType::DOT); // Add DOT token
        }
        return make_token(TokenType::UNKNOWN, PyRiteMessages::PARSE_ERROR_UNEXPECTED_CHAR);
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
    Token string(char quote) { while (peek() != quote && !is_at_end()) { if (peek() == '\n') line++; advance(); } if (is_at_end()) return make_token(TokenType::UNKNOWN, PyRiteMessages::PARSE_ERROR_UNTERMINATED_STRING); advance(); return Token{TokenType::STRING, source.substr(start + 1, current - start - 2), line}; }
};

// --- AST Node Definitions ---
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
struct FunctionDefNode : AstNode { std::string name; std::vector<ParameterDefinition> params; std::vector<AstNodePtr> body; FunctionDefNode(int l, std::string n, std::vector<ParameterDefinition> p, std::vector<AstNodePtr> b) : AstNode(l), name(n), params(p), body(b) {} ValuePtr accept(Interpreter& visitor) override; };
struct CallNode : AstNode { AstNodePtr callee; std::vector<AstNodePtr> arguments; CallNode(int l, AstNodePtr c, std::vector<AstNodePtr> a) : AstNode(l), callee(c), arguments(a) {} ValuePtr accept(Interpreter& visitor) override; };
struct SubscriptNode : AstNode { AstNodePtr object; AstNodePtr index; SubscriptNode(int l, AstNodePtr o, AstNodePtr i) : AstNode(l), object(o), index(i) {} ValuePtr accept(Interpreter& visitor) override; };
struct ReturnNode : AstNode { AstNodePtr value; ReturnNode(int l, AstNodePtr v) : AstNode(l), value(v) {} ValuePtr accept(Interpreter& visitor) override; };
struct RaiseNode : AstNode { AstNodePtr expression; RaiseNode(int l, AstNodePtr e) : AstNode(l), expression(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct TryCatchNode : AstNode { std::vector<AstNodePtr> try_branch; std::string exception_var; std::vector<AstNodePtr> catch_branch; std::vector<AstNodePtr> finally_branch; TryCatchNode(int l, std::vector<AstNodePtr> t, std::string ev, std::vector<AstNodePtr> c, std::vector<AstNodePtr> f) : AstNode(l), try_branch(t), exception_var(ev), catch_branch(c), finally_branch(f) {} ValuePtr accept(Interpreter& visitor) override; };
struct ClassDefNode : AstNode {
    std::string name;
    std::vector<ParameterDefinition> fields; // Field definitions
    std::vector<AstNodePtr> methods; // Method definitions (FunctionDefNode)
    ClassDefNode(int l, const std::string& n, const std::vector<ParameterDefinition>& f, const std::vector<AstNodePtr>& m)
        : AstNode(l), name(n), fields(f), methods(m) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct GetNode : AstNode {
    AstNodePtr object;
    std::string name; // property/method name
    GetNode(int l, AstNodePtr obj, const std::string& n) : AstNode(l), object(obj), name(n) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct SetNode : AstNode {
    AstNodePtr object;
    std::string name; // property name
    AstNodePtr value;
    SetNode(int l, AstNodePtr obj, const std::string& n, AstNodePtr val) : AstNode(l), object(obj), name(n), value(val) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct ExpressionStatementNode : AstNode {
    AstNodePtr expression;
    ExpressionStatementNode(int l, AstNodePtr e) : AstNode(l), expression(e) {}
    ValuePtr accept(Interpreter& visitor) override;
};
// --- Parser ---
class Parser {
public:
    Parser(const std::string& source) : tokenizer(source), had_error(false) {}
    std::vector<AstNodePtr> parse() { 
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_START << std::endl;
        std::vector<AstNodePtr> statements; 
        current_token = tokenizer.next_token(); 
        while (current_token.type != TokenType::END_OF_FILE && current_token.type != TokenType::HALT && current_token.type != TokenType::RUN) { 
            try { 
                statements.push_back(declaration()); 
            } catch (const std::runtime_error& e) { 
                std::cerr << PyRiteMessages::PARSE_ERROR_PREFIX << current_token.line << ": " << e.what() << PyRiteMessages::PARSE_ERROR_SUFFIX;
                had_error = true; 
                synchronize(); 
            } 
        } 
        return statements; 
    }
    bool has_error() const { return had_error; }
private:
    Tokenizer tokenizer; 
    Token current_token, previous_token; 
    bool had_error;

    // Forward declaration for statement() method
    AstNodePtr statement() {
        if (match({TokenType::IF})) return if_statement();
        if (match({TokenType::WHILE})) return while_statement();
        if (match({TokenType::AWAIT})) return await_statement();
        if (match({TokenType::SAY})) return say_statement();
        if (match({TokenType::RETURN})) return return_statement();
        if (match({TokenType::TRY})) return try_statement();
        if (match({TokenType::RAISE})) return raise_statement();
        return expression_statement();
    }
    
    void advance() { previous_token = current_token; current_token = tokenizer.next_token(); }
    void consume(TokenType type, const std::string& msg) { if (current_token.type == type) { advance(); return; } throw std::runtime_error(msg); }
    bool check(TokenType type) { return current_token.type == type; }
    bool match(const std::vector<TokenType>& types) { for (TokenType type : types) { if (check(type)) { advance(); return true; } } return false; }
    void synchronize() { advance(); while(current_token.type != TokenType::END_OF_FILE) { switch(current_token.type) { case TokenType::DEC: case TokenType::STR: case TokenType::IF: case TokenType::WHILE: case TokenType::DEF: case TokenType::INS: case TokenType::SAY: case TokenType::RETURN: case TokenType::TRY: return; default: advance(); } } }
    AstNodePtr declaration() { 
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_DECL << current_token.lexeme << ")..." << std::endl;
        if (match({TokenType::DEC, TokenType::STR, TokenType::BIN, TokenType::LIST})) return var_declaration(); 
        if (match({TokenType::DEF})) return function_definition("function"); 
        if (match({TokenType::INS})) return class_definition(); 
        return statement(); 
    }
    // 4. Implement parse_parameter method
    ParameterDefinition parse_parameter() {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_PARAM << std::endl;
        Token keyword = current_token;
        if (!match({TokenType::DEC, TokenType::STR, TokenType::BIN, TokenType::LIST, TokenType::ANY})) { 
            // Also update the error message
            throw std::runtime_error(PyRiteMessages::PARSE_ERROR_EXPECT_PARAM_TYPE);
        }
        consume(TokenType::IDENTIFIER, PyRiteMessages::PARSE_ERROR_EXPECT_PARAM_NAME);
        std::string param_name = previous_token.lexeme;
        ValuePtr default_value = nullptr;
        if (match({TokenType::EQUAL})) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_DEFAULT_VAL_FOR << param_name << "'..." << std::endl;
            // Evaluate default value expression. For simplicity, we'll only allow literals here.
            // A more robust implementation would evaluate the expression in the global scope.
            if (check(TokenType::NUMBER)) {
                advance();
                default_value = std::make_shared<NumberValue>(BigNumber(previous_token.lexeme));
            } else if (check(TokenType::STRING)) {
                advance();
                default_value = std::make_shared<StringValue>(previous_token.lexeme);
            } else if (check(TokenType::HEX_LITERAL)) {
                advance();
                default_value = std::make_shared<BinaryValue>(previous_token.lexeme);
            } else if (check(TokenType::NULL_LITERAL)) {
                advance(); // consume 'nul' token
                default_value = std::make_shared<NullValue>();
			} else if (check(TokenType::LBRACKET)) {
                // For list default, we need to parse a list literal.
                // This is a bit tricky in a single pass, so for now, we'll just create an empty list as default.
                // A more complete solution would parse the list literal properly.
                advance(); // consume '['
                if (check(TokenType::RBRACKET)) {
                    advance(); // consume ']'
                    default_value = std::make_shared<ListValue>(std::vector<ValuePtr>{});
                } else {
                    throw std::runtime_error(PyRiteMessages::PARSE_ERROR_UNSUPPORTED_DEFAULT_LIST);
                }
            } else {
                throw std::runtime_error(PyRiteMessages::PARSE_ERROR_DEFAULT_VALUE_LITERAL);
            }
        }
        // 5. Use the correct constructor to create ParameterDefinition
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSED_PARAM << param_name << (default_value ? PyRiteMessages::DEBUG_PARSER_WITH_DEFAULT : "") << std::endl;
        return ParameterDefinition(keyword.type, param_name, default_value);
    }
    AstNodePtr var_declaration() { 
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_VAR_DECL << std::endl;
        Token keyword = previous_token; consume(TokenType::IDENTIFIER, PyRiteMessages::PARSE_ERROR_EXPECT_VAR_NAME); std::string name = previous_token.lexeme; AstNodePtr initializer = nullptr; if (match({TokenType::EQUAL})) { initializer = expression(); } return std::make_shared<VarDeclarationNode>(keyword.line, keyword, name, initializer); }
    AstNodePtr function_definition(const std::string& kind) {
        if (DEBUG) std::cout << (kind == "function" ? PyRiteMessages::DEBUG_PARSER_PARSING_FUNC_DEF : PyRiteMessages::DEBUG_PARSER_PARSING_METHOD_DEF) << std::endl;
        int line = previous_token.line;
        consume(TokenType::IDENTIFIER, std::string("Expected ") + kind + " name.");
        std::string name = previous_token.lexeme;
        consume(TokenType::LPAREN, std::string("'('") + PyRiteMessages::PARSE_ERROR_EXPECT_LPAREN_AFTER_NAME);
        std::vector<ParameterDefinition> params; // Use ParameterDefinition
        if (!check(TokenType::RPAREN)) {
            do {
                if (params.size() >= 255) throw std::runtime_error(PyRiteMessages::PARSE_ERROR_TOO_MANY_PARAMS);
                params.push_back(parse_parameter()); // Call parse_parameter
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RPAREN, PyRiteMessages::PARSE_ERROR_EXPECT_RPAREN_AFTER_PARAMS);
        consume(TokenType::DO, PyRiteMessages::PARSE_ERROR_EXPECT_DO_BEFORE_BODY);
        std::vector<AstNodePtr> body;
        while (!check(TokenType::ENDDEF) && !check(TokenType::END_OF_FILE)) {
            body.push_back(declaration());
        }
        consume(TokenType::ENDDEF, PyRiteMessages::PARSE_ERROR_EXPECT_ENDDEF_AFTER_BODY);
        if (DEBUG) std::cout << (kind == "function" ? PyRiteMessages::DEBUG_PARSER_DONE_PARSING_FUNC : PyRiteMessages::DEBUG_PARSER_DONE_PARSING_METHOD) << name << "'." << std::endl;
        return std::make_shared<FunctionDefNode>(line, name, params, body);
    }
    // --- New Parser method for Class Definition ---
    AstNodePtr class_definition() {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_CLASS_DEF << std::endl;
        int line = previous_token.line;
        consume(TokenType::IDENTIFIER, PyRiteMessages::PARSE_ERROR_EXPECT_CLASS_NAME);
        std::string name = previous_token.lexeme;
        std::vector<ParameterDefinition> fields;
        if (match({TokenType::LPAREN})) {
            if (!check(TokenType::RPAREN)) {
                do {
                    if (fields.size() >= 255) throw std::runtime_error(PyRiteMessages::PARSE_ERROR_TOO_MANY_FIELDS);
                    fields.push_back(parse_parameter()); // Reuse parse_parameter for fields
                } while (match({TokenType::COMMA}));
            }
            consume(TokenType::RPAREN, PyRiteMessages::PARSE_ERROR_EXPECT_RPAREN_AFTER_FIELDS);
        }
        consume(TokenType::CONTAINS, PyRiteMessages::PARSE_ERROR_EXPECT_CONTAINS_AFTER_CLASS_DEF);
        std::vector<AstNodePtr> methods;
        while (!check(TokenType::ENDINS) && !check(TokenType::END_OF_FILE)) {
            if (match({TokenType::DEF})) {
                methods.push_back(function_definition("method")); // Reuse function_definition for methods
            } else {
                 throw std::runtime_error(PyRiteMessages::PARSE_ERROR_ONLY_METHODS_IN_CLASS);
            }
        }
        consume(TokenType::ENDINS, PyRiteMessages::PARSE_ERROR_EXPECT_ENDINS_AFTER_CLASS_BODY);
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_DONE_PARSING_CLASS << name << "'." << std::endl;
        return std::make_shared<ClassDefNode>(line, name, fields, methods);
    }
    // --- End of new Parser method ---
    AstNodePtr if_statement() { int line = previous_token.line; AstNodePtr condition = expression(); consume(TokenType::THEN, PyRiteMessages::PARSE_ERROR_EXPECT_THEN_AFTER_IF); std::vector<AstNodePtr> then_branch; while(!check(TokenType::ELSE) && !check(TokenType::ENDIF) && !check(TokenType::END_OF_FILE)) { then_branch.push_back(declaration()); } std::vector<AstNodePtr> else_branch; if (match({TokenType::ELSE})) { while(!check(TokenType::ENDIF) && !check(TokenType::END_OF_FILE)) { else_branch.push_back(declaration()); } } consume(TokenType::ENDIF, PyRiteMessages::PARSE_ERROR_EXPECT_ENDIF_AFTER_IF); return std::make_shared<IfStatementNode>(line, condition, then_branch, else_branch); }
    AstNodePtr while_statement() { int line = previous_token.line; AstNodePtr condition = expression(); consume(TokenType::DO, PyRiteMessages::PARSE_ERROR_EXPECT_DO_AFTER_WHILE); std::vector<AstNodePtr> do_branch; while(!check(TokenType::FINALLY) && !check(TokenType::ENDWHILE) && !check(TokenType::END_OF_FILE)) { do_branch.push_back(declaration()); } std::vector<AstNodePtr> finally_branch; if (match({TokenType::FINALLY})) { while(!check(TokenType::ENDWHILE) && !check(TokenType::END_OF_FILE)) { finally_branch.push_back(declaration()); } } consume(TokenType::ENDWHILE, PyRiteMessages::PARSE_ERROR_EXPECT_ENDWHILE_AFTER_WHILE); return std::make_shared<WhileStatementNode>(line, condition, do_branch, finally_branch); }
    AstNodePtr await_statement() { int line = previous_token.line; AstNodePtr condition = expression(); consume(TokenType::THEN, PyRiteMessages::PARSE_ERROR_EXPECT_THEN_AFTER_AWAIT); std::vector<AstNodePtr> then_branch; while (!check(TokenType::ENDAWAIT) && !check(TokenType::END_OF_FILE)) { then_branch.push_back(declaration()); } consume(TokenType::ENDAWAIT, PyRiteMessages::PARSE_ERROR_EXPECT_ENDAWAIT_AFTER_AWAIT); return std::make_shared<AwaitStatementNode>(line, condition, then_branch); }
    AstNodePtr try_statement() { int line = previous_token.line; std::vector<AstNodePtr> try_branch; while (!check(TokenType::CATCH) && !check(TokenType::END_OF_FILE)) { try_branch.push_back(declaration()); } consume(TokenType::CATCH, PyRiteMessages::PARSE_ERROR_EXPECT_CATCH_AFTER_TRY); consume(TokenType::IDENTIFIER, PyRiteMessages::PARSE_ERROR_EXPECT_VAR_AFTER_CATCH); std::string exception_var = previous_token.lexeme; std::vector<AstNodePtr> catch_branch; while (!check(TokenType::FINALLY) && !check(TokenType::ENDTRY) && !check(TokenType::END_OF_FILE)) { catch_branch.push_back(declaration()); } std::vector<AstNodePtr> finally_branch; if (match({TokenType::FINALLY})) { while (!check(TokenType::ENDTRY) && !check(TokenType::END_OF_FILE)) { finally_branch.push_back(declaration()); } } consume(TokenType::ENDTRY, PyRiteMessages::PARSE_ERROR_EXPECT_ENDTRY_AFTER_TRY); return std::make_shared<TryCatchNode>(line, try_branch, exception_var, catch_branch, finally_branch); }
    AstNodePtr raise_statement() { int line = previous_token.line; AstNodePtr expr = expression(); return std::make_shared<RaiseNode>(line, expr); }
    AstNodePtr say_statement() { int line = previous_token.line; consume(TokenType::LPAREN, PyRiteMessages::PARSE_ERROR_EXPECT_LPAREN_AFTER_SAY); AstNodePtr value = expression(); consume(TokenType::RPAREN, PyRiteMessages::PARSE_ERROR_EXPECT_RPAREN_AFTER_EXPR); return std::make_shared<SayNode>(line, value); }
    AstNodePtr return_statement() { int line = previous_token.line; ValuePtr v = std::make_shared<NullValue>(); AstNodePtr val_node = std::make_shared<LiteralNode>(line, v); if (!check(TokenType::ENDDEF) && !check(TokenType::ENDIF) && !check(TokenType::ENDWHILE) && !check(TokenType::ENDTRY)) { val_node = expression(); } return std::make_shared<ReturnNode>(line, val_node); }
    AstNodePtr expression_statement() { 
        int line = current_token.line;
        AstNodePtr expr = expression();
        return std::make_shared<ExpressionStatementNode>(line, expr);
    }
    AstNodePtr expression() { return assignment(); }
    AstNodePtr assignment() {
        AstNodePtr expr = equality();
        if (match({TokenType::EQUAL})) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_ASSIGNMENT << std::endl;
            int line = previous_token.line;
            AstNodePtr value = assignment(); // Right-associative
            // Check if the left-hand side is a valid assignment target
            if (dynamic_cast<VariableNode*>(expr.get()) || dynamic_cast<SubscriptNode*>(expr.get()) || dynamic_cast<GetNode*>(expr.get())) { // Add GetNode check
                return std::make_shared<AssignmentNode>(line, expr, value);
            }
            throw std::runtime_error(PyRiteMessages::RUNTIME_ERROR_INVALID_ASSIGNMENT_TARGET);
        }
        return expr;
    }
    AstNodePtr equality() { AstNodePtr expr = comparison(); while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) { Token op = previous_token; AstNodePtr right = comparison(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr comparison() { AstNodePtr expr = term(); while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL})) { Token op = previous_token; AstNodePtr right = term(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr term() { AstNodePtr expr = factor(); while (match({TokenType::PLUS, TokenType::MINUS})) { Token op = previous_token; AstNodePtr right = factor(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr factor() { AstNodePtr expr = power(); while (match({TokenType::STAR, TokenType::SLASH})) { Token op = previous_token; AstNodePtr right = power(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr power() { AstNodePtr expr = unary(); while(match({TokenType::CARET})) { Token op = previous_token; AstNodePtr right = unary(); expr = std::make_shared<BinaryOpNode>(op.line, expr, op, right); } return expr; }
    AstNodePtr unary() { if (match({TokenType::MINUS})) { Token op = previous_token; AstNodePtr right = unary(); AstNodePtr left = std::make_shared<LiteralNode>(op.line, std::make_shared<NumberValue>(0)); return std::make_shared<BinaryOpNode>(op.line, left, op, right); } return call(); }
    AstNodePtr call() {
        AstNodePtr expr = primary();
        while (true) {
            if (match({TokenType::LPAREN})) {
                if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_CALL << std::endl;
                expr = finish_call(expr);
            } else if (match({TokenType::LBRACKET})) {
                if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_SUBSCRIPT << std::endl;
                expr = finish_subscript(expr);
            } else if (match({TokenType::DOT})) { // Handle property/method access
                if (DEBUG) std::cout << PyRiteMessages::DEBUG_PARSER_PARSING_GET << std::endl;
                consume(TokenType::IDENTIFIER, PyRiteMessages::PARSE_ERROR_EXPECT_PROP_NAME);
                std::string name = previous_token.lexeme;
                expr = std::make_shared<GetNode>(previous_token.line, expr, name); // Create GetNode
            } else {
                break;
            }
        }
        return expr;
    }
    AstNodePtr finish_call(AstNodePtr callee) { int line = previous_token.line; std::vector<AstNodePtr> arguments; if (!check(TokenType::RPAREN)) { do { if (arguments.size() >= 255) throw std::runtime_error(PyRiteMessages::PARSE_ERROR_TOO_MANY_ARGS); arguments.push_back(expression()); } while (match({TokenType::COMMA})); } consume(TokenType::RPAREN, PyRiteMessages::PARSE_ERROR_EXPECT_RPAREN_AFTER_PARAMS); return std::make_shared<CallNode>(line, callee, arguments); }
    AstNodePtr finish_subscript(AstNodePtr object) { int line = previous_token.line; AstNodePtr index = expression(); consume(TokenType::RBRACKET, PyRiteMessages::PARSE_ERROR_EXPECT_RBRACKET_AFTER_INDEX); return std::make_shared<SubscriptNode>(line, object, index); }
    AstNodePtr list_literal() { int line = previous_token.line; std::vector<AstNodePtr> elements; if (!check(TokenType::RBRACKET)) { do { elements.push_back(expression()); } while (match({TokenType::COMMA})); } consume(TokenType::RBRACKET, PyRiteMessages::PARSE_ERROR_EXPECT_RBRACKET_AFTER_LIST); return std::make_shared<ListLiteralNode>(line, elements); }
    AstNodePtr primary() { 
        int line = current_token.line; 
        if (match({TokenType::NUMBER})) return std::make_shared<LiteralNode>(line, std::make_shared<NumberValue>(BigNumber(previous_token.lexeme))); 
        if (match({TokenType::STRING})) return std::make_shared<LiteralNode>(line, std::make_shared<StringValue>(previous_token.lexeme)); 
        if (match({TokenType::HEX_LITERAL})) return std::make_shared<LiteralNode>(line, std::make_shared<BinaryValue>(previous_token.lexeme)); 
        if (match({TokenType::NULL_LITERAL})) {
            return std::make_shared<LiteralNode>(line, std::make_shared<NullValue>());
        }
		if (match({TokenType::LBRACKET})) return list_literal(); 
        if (match({TokenType::IDENTIFIER})) return std::make_shared<VariableNode>(line, previous_token.lexeme); 
        if (match({TokenType::ASK})) { 
            consume(TokenType::LPAREN, PyRiteMessages::PARSE_ERROR_EXPECT_LPAREN_AFTER_ASK);
            AstNodePtr prompt = expression(); 
            consume(TokenType::RPAREN, PyRiteMessages::PARSE_ERROR_EXPECT_RPAREN_AFTER_PROMPT);
            return std::make_shared<InpNode>(line, prompt); 
        } 
        if (match({TokenType::LPAREN})) { 
            AstNodePtr expr = expression(); 
            consume(TokenType::RPAREN, PyRiteMessages::PARSE_ERROR_EXPECT_RPAREN_AFTER_EXPR);
            return expr; 
        } 
        throw std::runtime_error(PyRiteMessages::PARSE_ERROR_EXPECT_EXPRESSION);
    }
};
// --- Interpreter ---
class Interpreter {
public:
    Interpreter(); // Constructor defined after helper methods
    std::string base_path;
    void interpret(const std::vector<AstNodePtr>& statements) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INTERP_START << statements.size() << PyRiteMessages::DEBUG_INTERP_STATEMENTS << std::endl;
        try { 
            int i = 0;
            for (const auto& stmt : statements) { 
                if (DEBUG) std::cout << PyRiteMessages::DEBUG_INTERP_EXEC_TOP_LEVEL << i++ << PyRiteMessages::DEBUG_INTERP_AT_LINE << stmt->line << PyRiteMessages::DEBUG_INTERP_END_EXEC_STMT << std::endl;
                check_timeout(stmt->line); 
                execute(stmt); 
            } 
        }
        catch (const PyRiteRaiseException& ex) { std::cerr << PyRiteMessages::RUNTIME_ERROR_UNCAUGHT_EXCEPTION_PREFIX << ex.value->repr() << std::endl; print_stack_trace(); }
        catch (const RuntimeError& error) { std::cerr << PyRiteMessages::RUNTIME_ERROR_PREFIX << error.line << ": " << error.what() << std::endl; print_stack_trace(); }
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INTERP_DONE << std::endl;
    }
    void execute(const AstNodePtr& stmt) { 
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INTERP_EXEC_STMT_TYPE << typeid(*stmt).name() << std::endl;
        stmt->accept(*this); 
    }
    ValuePtr evaluate(const AstNodePtr& expr) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INTERP_EVAL_EXPR_TYPE << typeid(*expr).name() << std::endl;
        return expr->accept(*this); 
    }
    void execute_block(const std::vector<AstNodePtr>& statements, std::shared_ptr<Environment> block_env) {
        std::shared_ptr<Environment> previous = this->environment;
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INTERP_BLOCK_ENTER << block_env.get() << PyRiteMessages::DEBUG_INTERP_BLOCK_PREVIOUS << previous.get() << std::endl;
        try { 
            this->environment = block_env; 
            for(const auto& stmt : statements) { 
                check_timeout(stmt->line); 
                execute(stmt); 
            } 
        } catch (...) { 
            this->environment = previous; 
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_INTERP_BLOCK_EXIT_EXCEPTION << previous.get() << std::endl;
            throw; 
        }
        this->environment = previous;
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_INTERP_BLOCK_EXIT << previous.get() << std::endl;
    }
    // Timeout check related
    void check_timeout(int line) {
        if (time_limit_ms > 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            if (elapsed >= time_limit_ms) {
                throw RuntimeError(line, std::string(PyRiteMessages::RUNTIME_ERROR_EXECUTION_TIMEOUT_PREFIX) + std::to_string(time_limit_ms) + PyRiteMessages::RUNTIME_ERROR_EXECUTION_TIMEOUT_SUFFIX);
            }
        }
    }
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    long long time_limit_ms;
    struct CallInfo { std::string function_name; int call_site_line; }; std::vector<CallInfo> call_stack;
    std::shared_ptr<Environment> globals; std::shared_ptr<Environment> environment;
    std::string repl_buffer; // For REPL buffer compilation
private:
    void define_native_functions();
    void print_stack_trace() { if (call_stack.empty()) return; std::cerr << PyRiteMessages::RUNTIME_ERROR_STACK_TRACE_HEADER << std::endl; for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it) { std::cerr << PyRiteMessages::RUNTIME_ERROR_STACK_TRACE_ENTRY_PREFIX << it->function_name << PyRiteMessages::RUNTIME_ERROR_STACK_TRACE_ENTRY_SUFFIX << it->call_site_line << ")" << std::endl; } call_stack.clear(); }
};
// --- Helper function for type checking (definition) ---
bool is_type_compatible(TokenType expected_type, const ValuePtr& value) {
    switch (expected_type) {
        case TokenType::ANY:
            return true;  // ANY type accepts any value
        case TokenType::DEC:
            return dynamic_cast<NumberValue*>(value.get()) != nullptr;
        case TokenType::STR:
            return dynamic_cast<StringValue*>(value.get()) != nullptr;
        case TokenType::BIN:
            return dynamic_cast<BinaryValue*>(value.get()) != nullptr;
        case TokenType::LIST:
            return dynamic_cast<ListValue*>(value.get()) != nullptr;
        default:
            return true; // Should not happen for parameter types
    }
}
std::string token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::ANY: return "any";
        case TokenType::DEC: return "dec";
        case TokenType::STR: return "str";
        case TokenType::BIN: return "bin";
        case TokenType::LIST: return "list";
        default: return "unknown";
    }
}

// --- AST Node accept Method Implementations ---
ValuePtr LiteralNode::accept(Interpreter& visitor) { 
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_LITERAL_NODE << value->repr() << std::endl;
    return value; 
}
ValuePtr ListLiteralNode::accept(Interpreter& visitor) {
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_LIST_LITERAL_NODE << elements.size() << PyRiteMessages::DEBUG_EXEC_LIST_LITERAL_NODE_ELEMENTS << std::endl;
    std::vector<ValuePtr> evaluated_elements; for (const auto& elem : elements) { evaluated_elements.push_back(visitor.evaluate(elem)); } return std::make_shared<ListValue>(evaluated_elements); }
ValuePtr VariableNode::accept(Interpreter& visitor) {
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_VAR_NODE << name << "'." << std::endl;
    try { return visitor.environment->get(name); } catch(RuntimeError& e) { throw RuntimeError(line, e.what()); }
}
ValuePtr AssignmentNode::accept(Interpreter& visitor) {
    auto val = visitor.evaluate(value);
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_ASSIGN_NODE << val->repr() << std::endl;

    if (auto var_node = dynamic_cast<VariableNode*>(target.get())) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_ASSIGN_TARGET_VAR << var_node->name << "'." << std::endl;
        try { visitor.environment->assign(var_node->name, val); }
        catch (RuntimeError& e) { throw RuntimeError(line, e.what()); }
    } else if (auto sub_node = dynamic_cast<SubscriptNode*>(target.get())) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_ASSIGN_TARGET_SUBSCRIPT << std::endl;
        try {
            auto object = visitor.evaluate(sub_node->object);
            auto index = visitor.evaluate(sub_node->index);
            object->setSubscript(*index, val);
        } catch (const std::runtime_error& e) {
            throw RuntimeError(line, e.what());
        }
    } else if (auto get_node = dynamic_cast<GetNode*>(target.get())) {
         if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_ASSIGN_TARGET_GET << get_node->name << "'." << std::endl;
         // This handles `instance.property = value`
         try {
             auto object = visitor.evaluate(get_node->object);
             if (auto instance = std::dynamic_pointer_cast<Instance>(object)) {
                 instance->set(get_node->name, val);
             } else {
                 throw RuntimeError(line, PyRiteMessages::RUNTIME_ERROR_ONLY_INSTANCES_CAN_SET_PROPERTIES);
             }
         } catch (const std::runtime_error& e) {
             throw RuntimeError(line, e.what());
         }
    } else {
        throw RuntimeError(line, PyRiteMessages::RUNTIME_ERROR_INVALID_ASSIGNMENT_TARGET);
    }
    return val;
}
ValuePtr VarDeclarationNode::accept(Interpreter& visitor) {
    ValuePtr val = std::make_shared<NullValue>();
    if (initializer) { 
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_VAR_DECL_NODE_INIT << name << PyRiteMessages::DEBUG_EXEC_VAR_DECL_NODE_WITH_INIT << std::endl;
        val = visitor.evaluate(initializer); 
    } else {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_VAR_DECL_NODE_INIT << name << PyRiteMessages::DEBUG_EXEC_VAR_DECL_NODE_NO_INIT << std::endl;
    }

    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_VAR_DECL_DECLARED_TYPE << keyword.lexeme << PyRiteMessages::DEBUG_EXEC_VAR_DECL_INITIAL_VALUE << val->repr() << std::endl;

    if (keyword.type == TokenType::DEC) {
        if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { val = std::make_shared<NumberValue>(BigNumber(s_val->value)); } catch (const std::invalid_argument&) { throw RuntimeError(line, std::string(PyRiteMessages::RUNTIME_ERROR_CANNOT_CONVERT_STRING_TO_NUMBER_PREFIX) + s_val->value + PyRiteMessages::RUNTIME_ERROR_CANNOT_CONVERT_STRING_TO_NUMBER_SUFFIX); } }
        else if (auto b_val = dynamic_cast<BinaryValue*>(val.get())) { val = std::make_shared<NumberValue>(b_val->toBigNumber()); }
        else if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<NumberValue>(0); }
    } else if (keyword.type == TokenType::STR) {
        val = std::make_shared<StringValue>(val->toString());
    } else if (keyword.type == TokenType::BIN) {
        if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { val = std::make_shared<BinaryValue>(s_val->value); } catch(...) { throw RuntimeError(line, std::string(PyRiteMessages::RUNTIME_ERROR_CANNOT_CONVERT_STRING_TO_BINARY_PREFIX) + s_val->value + PyRiteMessages::RUNTIME_ERROR_CANNOT_CONVERT_STRING_TO_BINARY_SUFFIX); } }
        else if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<BinaryValue>(std::vector<uint8_t>{0}); }
    } else if (keyword.type == TokenType::LIST) {
        if (!dynamic_cast<ListValue*>(val.get()) && !dynamic_cast<NullValue*>(val.get())) { throw RuntimeError(line, PyRiteMessages::RUNTIME_ERROR_LIST_INIT_WITH_LIST_ONLY); }
        if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<ListValue>(std::vector<ValuePtr>{}); }
    }
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_VAR_DECL_FINAL_VALUE << val->repr() << std::endl;
    visitor.environment->define(name, val);
    return std::make_shared<NullValue>();
}
ValuePtr BinaryOpNode::accept(Interpreter& visitor) { 
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_BINARY_OP << op.lexeme << ")." << std::endl;
    auto left_val = visitor.evaluate(left); 
    auto right_val = visitor.evaluate(right); 
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_BINARY_OP_LEFT << left_val->repr() << PyRiteMessages::DEBUG_EXEC_BINARY_OP_RIGHT << right_val->repr() << std::endl;
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
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_SUBSCRIPT_NODE << std::endl;
    try {
        auto object = visitor.evaluate(this->object);
        auto index = visitor.evaluate(this->index);
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_SUBSCRIPT_NODE_OBJ << object->repr() << PyRiteMessages::DEBUG_EXEC_SUBSCRIPT_NODE_IDX << index->repr() << std::endl;
        return object->getSubscript(*index);
    } catch (const std::runtime_error& e) {
        throw RuntimeError(line, e.what());
    }
}
// --- New AST Node accept methods for Classes ---
ValuePtr ClassDefNode::accept(Interpreter& visitor) {
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CLASS_DEF_NODE << name << "'." << std::endl;
    // Parse methods from AST nodes into Function objects
    std::map<std::string, std::shared_ptr<Function>> methods_map;
    for (const auto& method_ast : methods) {
        // The parser ensures these are FunctionDefNode
        auto func_def_node = std::dynamic_pointer_cast<FunctionDefNode>(method_ast);
        if (func_def_node) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CLASS_DEF_PROCESSING_METHOD << func_def_node->name << "'." << std::endl;
            // Create the Function object for the method
            auto method_func = std::make_shared<Function>(
                func_def_node->name,
                func_def_node->params,
                func_def_node->body,
                visitor.environment // Capture the environment where the class is defined
            );
            methods_map[func_def_node->name] = method_func;
        }
    }
    // Create the Class object
    auto klass = std::make_shared<Class>(name, fields, methods_map, visitor.environment);
    // Define the class in the current environment
    visitor.environment->define(name, klass);
    return std::make_shared<NullValue>();
}
ValuePtr GetNode::accept(Interpreter& visitor) {
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_GET_NODE << name << "'." << std::endl;
    auto object_val = visitor.evaluate(object);
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_GET_NODE_OBJECT_EVAL << object_val->repr() << std::endl;
    if (auto instance = std::dynamic_pointer_cast<Instance>(object_val)) {
        try {
            // Call the simplified get() method without the interpreter reference
            return instance->get(name);
        } catch (const std::runtime_error& e) {
            throw RuntimeError(line, e.what());
        }
    }
    throw RuntimeError(line, std::string(PyRiteMessages::RUNTIME_ERROR_ONLY_INSTANCES_HAVE_PROPERTIES_PREFIX) + name + PyRiteMessages::RUNTIME_ERROR_ONLY_INSTANCES_HAVE_PROPERTIES_SUFFIX);
}
ValuePtr SetNode::accept(Interpreter& visitor) {
    // This node is primarily handled in AssignmentNode now.
    // But if needed directly, it would evaluate object, value, and call instance->set().
    throw RuntimeError(line, PyRiteMessages::RUNTIME_ERROR_SET_NODE_HANDLED_BY_ASSIGNMENT);
}
// --- End of new AST Node accept methods ---
ValuePtr IfStatementNode::accept(Interpreter& visitor) { 
    visitor.check_timeout(line);
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_IF_NODE << std::endl;
    auto condition_val = visitor.evaluate(condition);
    if (DEBUG) std::cout << (condition_val->isTruthy() ? PyRiteMessages::DEBUG_EXEC_IF_CONDITION_TRUE : PyRiteMessages::DEBUG_EXEC_IF_CONDITION_FALSE) << PyRiteMessages::DEBUG_EXEC_IF_CONDITION_VALUE << condition_val->repr() << ")." << std::endl;

    if (condition_val->isTruthy()) { 
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_IF_THEN << std::endl;
        visitor.execute_block(then_branch, std::make_shared<Environment>(visitor.environment)); 
    } else if (!else_branch.empty()) { 
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_IF_ELSE << std::endl;
        visitor.execute_block(else_branch, std::make_shared<Environment>(visitor.environment)); 
    } 
    return std::make_shared<NullValue>(); 
}
ValuePtr WhileStatementNode::accept(Interpreter& visitor) {
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_WHILE_NODE << std::endl;
    while(true) {
        auto condition_val = visitor.evaluate(condition);
        if (DEBUG) std::cout << (condition_val->isTruthy() ? PyRiteMessages::DEBUG_EXEC_WHILE_CONDITION_TRUE : PyRiteMessages::DEBUG_EXEC_WHILE_CONDITION_FALSE) << PyRiteMessages::DEBUG_EXEC_IF_CONDITION_VALUE << condition_val->repr() << ")." << std::endl;
        if (!condition_val->isTruthy()) break;

        visitor.check_timeout(line); 
        visitor.execute_block(do_branch, std::make_shared<Environment>(visitor.environment)); 
    } 
    if (!finally_branch.empty()) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_WHILE_FINALLY << std::endl;
        visitor.execute_block(finally_branch, std::make_shared<Environment>(visitor.environment)); 
    } 
    return std::make_shared<NullValue>(); 
}
ValuePtr AwaitStatementNode::accept(Interpreter& visitor) {
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_AWAIT_NODE << std::endl;
    while(!visitor.evaluate(condition)->isTruthy()) {
        visitor.check_timeout(line);
        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Prevent busy-waiting
    }
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_AWAIT_SATISFIED << std::endl;
    visitor.execute_block(then_branch, std::make_shared<Environment>(visitor.environment));
    return std::make_shared<NullValue>();
}
ValuePtr SayNode::accept(Interpreter& visitor) { 
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_SAY_NODE << std::endl;
    auto val = visitor.evaluate(expression); 
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_SAY_VALUE << val->repr() << std::endl;
    std::cout << val->toString() << std::endl; 
    return std::make_shared<NullValue>(); 
}
ValuePtr InpNode::accept(Interpreter& visitor) { auto prompt = visitor.evaluate(expression); std::cout << prompt->toString(); std::string input; std::getline(std::cin, input); return std::make_shared<StringValue>(input); }
ValuePtr FunctionDefNode::accept(Interpreter& visitor) {
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_FUNC_DEF_NODE << name << "'." << std::endl;
    auto function = std::make_shared<Function>(name, params, body, visitor.environment); 
    visitor.environment->define(name, std::make_shared<FunctionValue>(function)); 
    return std::make_shared<NullValue>(); 
}
ValuePtr CallNode::accept(Interpreter& visitor) {
    visitor.check_timeout(line);
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE << line << PyRiteMessages::DEBUG_EXEC_CALL_NODE_EVAL_CALLEE << std::endl;

    if (auto callee_var = dynamic_cast<VariableNode*>(callee.get())) {
        if (callee_var->name == "swap") {
            if (arguments.size() != 2) throw RuntimeError(line, PyRiteMessages::ERROR_SWAP_REQUIRES_TWO_VARS);
            auto arg1 = dynamic_cast<VariableNode*>(arguments[0].get());
            auto arg2 = dynamic_cast<VariableNode*>(arguments[1].get());
            if (!arg1 || !arg2) throw RuntimeError(line, PyRiteMessages::ERROR_SWAP_ARGS_MUST_BE_VARS);
            try {
                std::string name1 = arg1->name; std::string name2 = arg2->name;
                ValuePtr val1 = visitor.environment->get(name1); ValuePtr val2 = visitor.environment->get(name2);
                ValuePtr type1_str_val = visitor.environment->get_type(name1); std::string type1_str = dynamic_cast<StringValue*>(type1_str_val.get())->value;
                ValuePtr type2_str_val = visitor.environment->get_type(name2); std::string type2_str = dynamic_cast<StringValue*>(type2_str_val.get())->value;
                ValuePtr new_val1;
                if(type1_str == "dec") {
                    try { new_val1 = std::make_shared<NumberValue>(BigNumber(val2->toString())); } catch (...) { new_val1 = std::make_shared<StringValue>(val2->toString()); }
                } else if (type1_str == "str") {
                    new_val1 = std::make_shared<StringValue>(val2->toString());
                } else if (type1_str == "bin") {
                    try { new_val1 = std::make_shared<BinaryValue>(val2->toString()); } catch (...) { new_val1 = std::make_shared<StringValue>(val2->toString()); }
                } else {
                    new_val1 = val2->clone();
                }
                ValuePtr new_val2;
                if(type2_str == "dec") {
                    try { new_val2 = std::make_shared<NumberValue>(BigNumber(val1->toString())); } catch (...) { new_val2 = std::make_shared<StringValue>(val1->toString()); }
                } else if (type2_str == "str") {
                    new_val2 = std::make_shared<StringValue>(val1->toString());
                } else if (type2_str == "bin") {
                    try { new_val2 = std::make_shared<BinaryValue>(val1->toString()); } catch (...) { new_val2 = std::make_shared<StringValue>(val1->toString()); }
                } else {
                    new_val2 = val1->clone();
                }
                visitor.environment->assign(name1, new_val1);
                visitor.environment->assign(name2, new_val2);
            } catch(const RuntimeError& e) {
                throw RuntimeError(line, e.what());
            }
            return std::make_shared<NullValue>();
        }
    }

    auto callee_val = visitor.evaluate(callee);
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_CALLEE_EVAL << callee_val->repr() << std::endl;

    std::vector<ValuePtr> arg_values;
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_EVAL_ARGS << arguments.size() << PyRiteMessages::DEBUG_EXEC_CALL_NODE_ARGS << std::endl; 
    for(const auto& arg_expr : arguments) { 
        arg_values.push_back(visitor.evaluate(arg_expr)); 
    }

    // 1. Handle native functions
    if (auto native_fn = dynamic_cast<NativeFnValue*>(callee_val.get())) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_NATIVE << native_fn->name << "'." << std::endl;
        visitor.call_stack.push_back({native_fn->name, line});
        try {
            ValuePtr result = native_fn->call(arg_values);
            visitor.call_stack.pop_back();
            return result;
        } catch(const std::exception& e) { // Catch more generic exception types
            visitor.call_stack.pop_back();
            throw RuntimeError(line, e.what());
        }
    }

    // 2. Handle bound instance methods
    if (auto bound_method = dynamic_cast<BoundMethodValue*>(callee_val.get())) {
        auto function = bound_method->method;
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_BOUND_METHOD << bound_method->instance->klass->name << "." << function->name << PyRiteMessages::DEBUG_EXEC_CALL_NODE_ON_INSTANCE << bound_method->instance->repr() << "." << std::endl;
        auto call_env = std::make_shared<Environment>(function->closure);
        
        // Crucial step: define 'this' in the method environment
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_DEFINE_THIS << std::endl;
        call_env->define("this", bound_method->instance);

        // --- Argument binding and type checking ---
        const auto& param_defs = function->params;
        size_t num_provided_args = arg_values.size();
        size_t num_required_params = 0;
        for (const auto& p : param_defs) { if (!p.has_default) num_required_params++; }
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_BINDING_ARGS << num_provided_args << PyRiteMessages::DEBUG_EXEC_CALL_NODE_ARGS_TO << param_defs.size() << PyRiteMessages::DEBUG_EXEC_CALL_NODE_PARAMS << num_required_params << PyRiteMessages::DEBUG_EXEC_CALL_NODE_REQUIRED << std::endl;

        if (num_provided_args < num_required_params) {
            std::stringstream ss;
            ss << "Method '" << function->name << PyRiteMessages::ERROR_ARG_COUNT_PREFIX_AT_LEAST << num_required_params << PyRiteMessages::ERROR_ARG_COUNT_SUFFIX_BUT_GOT_PLURAL << num_provided_args << ".";
            throw RuntimeError(line, ss.str());
        }
        if (num_provided_args > param_defs.size()) {
            std::stringstream ss;
            ss << "Method '" << function->name << PyRiteMessages::ERROR_ARG_COUNT_PREFIX_AT_MOST << param_defs.size() << PyRiteMessages::ERROR_ARG_COUNT_SUFFIX_BUT_GOT << num_provided_args << ".";
            throw RuntimeError(line, ss.str());
        }

        for (size_t i = 0; i < function->params.size(); ++i) {
            ValuePtr current_arg_value;
            if (i < num_provided_args) {
                current_arg_value = arg_values[i];
            } else {
                current_arg_value = param_defs[i].default_value->clone();
            }
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_BINDING_PARAM << param_defs[i].name << PyRiteMessages::DEBUG_EXEC_CALL_NODE_TO_ARG << current_arg_value->repr() << std::endl;
            if (!is_type_compatible(param_defs[i].type_keyword, current_arg_value)) {
                 std::stringstream ss;
                 ss << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_PREFIX << (i+1) << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_IN_METHOD << function->name
                    << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_NAME << param_defs[i].name
                    << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_EXPECTED << token_type_to_string(param_defs[i].type_keyword)
                    << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_GOT;
                 if (dynamic_cast<NumberValue*>(current_arg_value.get())) ss << "dec";
                 else if (dynamic_cast<StringValue*>(current_arg_value.get())) ss << "str";
                 else if (dynamic_cast<BinaryValue*>(current_arg_value.get())) ss << "bin";
                 else if (dynamic_cast<ListValue*>(current_arg_value.get())) ss << "list";
                 else ss << "unknown";
                 ss << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_SUFFIX;
                 throw RuntimeError(line, ss.str());
            }
            call_env->define(param_defs[i].name, current_arg_value);
        }
        // --- End of argument binding ---

        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_PUSH_STACK << function->name << PyRiteMessages::DEBUG_EXEC_CALL_NODE_TO_STACK << std::endl;
        visitor.call_stack.push_back({function->name, line});
        ValuePtr return_val = std::make_shared<NullValue>();
        try {
            visitor.execute_block(function->body, call_env);
        } catch (const ReturnValueException& rv) {
            return_val = rv.value;
        }
        visitor.call_stack.pop_back();
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_POP_STACK << function->name << PyRiteMessages::DEBUG_EXEC_CALL_NODE_FROM_STACK << return_val->repr() << std::endl;
        return return_val;
    }

    // 3. Handle regular functions
    if (auto func_val = dynamic_cast<FunctionValue*>(callee_val.get())) {
        auto function = func_val->value;
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_USER << function->name << "'." << std::endl;
        auto call_env = std::make_shared<Environment>(function->closure);

        // --- Argument binding and type checking (same logic as methods, but no 'this') ---
        const auto& param_defs = function->params;
        size_t num_provided_args = arg_values.size();
        size_t num_required_params = 0;
        for (const auto& p : param_defs) { if (!p.has_default) num_required_params++; }
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_BINDING_ARGS << num_provided_args << PyRiteMessages::DEBUG_EXEC_CALL_NODE_ARGS_TO << param_defs.size() << PyRiteMessages::DEBUG_EXEC_CALL_NODE_PARAMS << num_required_params << PyRiteMessages::DEBUG_EXEC_CALL_NODE_REQUIRED << std::endl;
        
        if (num_provided_args < num_required_params) {
            std::stringstream ss;
            ss << "Function '" << function->name << PyRiteMessages::ERROR_ARG_COUNT_PREFIX_AT_LEAST << num_required_params << PyRiteMessages::ERROR_ARG_COUNT_SUFFIX_BUT_GOT_PLURAL << num_provided_args << ".";
            throw RuntimeError(line, ss.str());
        }
        if (num_provided_args > param_defs.size()) {
            std::stringstream ss;
            ss << "Function '" << function->name << PyRiteMessages::ERROR_ARG_COUNT_PREFIX_AT_MOST << param_defs.size() << PyRiteMessages::ERROR_ARG_COUNT_SUFFIX_BUT_GOT << num_provided_args << ".";
            throw RuntimeError(line, ss.str());
        }

        for (size_t i = 0; i < function->params.size(); ++i) {
            ValuePtr current_arg_value;
            if (i < num_provided_args) {
                current_arg_value = arg_values[i];
            } else {
                current_arg_value = param_defs[i].default_value->clone();
            }
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_BINDING_PARAM << param_defs[i].name << PyRiteMessages::DEBUG_EXEC_CALL_NODE_TO_ARG << current_arg_value->repr() << std::endl;
            if (!is_type_compatible(param_defs[i].type_keyword, current_arg_value)) {
                 std::stringstream ss;
                 ss << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_PREFIX << (i+1) << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_IN_FUNCTION << function->name
                    << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_NAME << param_defs[i].name
                    << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_EXPECTED << token_type_to_string(param_defs[i].type_keyword)
                    << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_GOT;
                 if (dynamic_cast<NumberValue*>(current_arg_value.get())) ss << "dec";
                 else if (dynamic_cast<StringValue*>(current_arg_value.get())) ss << "str";
                 else if (dynamic_cast<BinaryValue*>(current_arg_value.get())) ss << "bin";
                 else if (dynamic_cast<ListValue*>(current_arg_value.get())) ss << "list";
                 else ss << "unknown";
                 ss << PyRiteMessages::ERROR_ARG_TYPE_MISMATCH_SUFFIX;
                 throw RuntimeError(line, ss.str());
            }
            call_env->define(param_defs[i].name, current_arg_value);
        }
        // --- End of argument binding ---

        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_PUSH_STACK << function->name << PyRiteMessages::DEBUG_EXEC_CALL_NODE_TO_STACK << std::endl;
        visitor.call_stack.push_back({function->name, line});
        ValuePtr return_val = std::make_shared<NullValue>();
        try { 
            visitor.execute_block(function->body, call_env); 
        } catch (const ReturnValueException& rv) { 
            return_val = rv.value; 
        }
        visitor.call_stack.pop_back();
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_CALL_NODE_POP_STACK << function->name << PyRiteMessages::DEBUG_EXEC_CALL_NODE_FROM_STACK << return_val->repr() << std::endl;
        return return_val;
    }

    throw RuntimeError(line, std::string(PyRiteMessages::RUNTIME_ERROR_CAN_ONLY_CALL_FUNCTIONS) + callee_val->repr() + PyRiteMessages::RUNTIME_ERROR_CAN_ONLY_CALL_FUNCTIONS_SUFFIX);
}
ValuePtr ReturnNode::accept(Interpreter& visitor) { 
    ValuePtr val = std::make_shared<NullValue>(); 
    if (value) { 
        val = visitor.evaluate(value); 
    } 
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_RETURN_NODE << val->repr() << "." << std::endl;
    throw ReturnValueException(val); 
}
ValuePtr RaiseNode::accept(Interpreter& visitor) {
    auto value_to_raise = visitor.evaluate(expression);
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_RAISE_NODE << value_to_raise->repr() << "." << std::endl;
    throw PyRiteRaiseException(value_to_raise); 
}
ValuePtr TryCatchNode::accept(Interpreter& visitor) {
    std::unique_ptr<std::exception_ptr> captured_exception = nullptr;
    try {
        try {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_TRY_NODE << std::endl;
            visitor.execute_block(try_branch, std::make_shared<Environment>(visitor.environment));
        } catch (const PyRiteRaiseException& ex) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_TRY_CAUGHT_PYRITE << ex.value->repr() << PyRiteMessages::DEBUG_EXEC_TRY_EXEC_CATCH << std::endl;
            auto catch_env = std::make_shared<Environment>(visitor.environment);
            catch_env->define(exception_var, ex.value);
            visitor.execute_block(catch_branch, catch_env);
        } catch (const RuntimeError& ex) {
            if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_TRY_CAUGHT_RUNTIME << ex.what() << PyRiteMessages::DEBUG_EXEC_TRY_EXEC_CATCH << std::endl;
            auto exception_obj = std::make_shared<ExceptionValue>(std::make_shared<StringValue>(ex.what()));
            auto catch_env = std::make_shared<Environment>(visitor.environment);
            catch_env->define(exception_var, exception_obj);
            visitor.execute_block(catch_branch, catch_env);
        }
    } catch (...) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_TRY_CAUGHT_UNEXPECTED << std::endl;
        captured_exception.reset(new std::exception_ptr(std::current_exception()));
    }
    if (!finally_branch.empty()) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_TRY_EXEC_FINALLY << std::endl;
        visitor.execute_block(finally_branch, std::make_shared<Environment>(visitor.environment));
    }
    if (captured_exception) {
        if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_TRY_RETHROWING << std::endl;
        std::rethrow_exception(*captured_exception);
    }
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_TRY_DONE << std::endl;
    return std::make_shared<NullValue>();
}
ValuePtr ExpressionStatementNode::accept(Interpreter& visitor) {
    if (DEBUG) std::cout << PyRiteMessages::DEBUG_EXEC_EXPR_STMT_NODE << std::endl;
    visitor.evaluate(expression);
    return std::make_shared<NullValue>(); // Expression statements themselves do not return a value
}

// --- Interpreter Constructor and Native Function Definitions ---
Interpreter::Interpreter() : globals(std::make_shared<Environment>()), environment(globals), time_limit_ms(0) {
    define_native_functions();
}
void Interpreter::define_native_functions() {
    #define REQUIRE_ARGS(name, count) if(args.size() != count) throw std::runtime_error(name + std::string(PyRiteMessages::NATIVE_ERROR_REQUIRES_ARGS_SUFFIX) + #count + " arguments.");
    #define REQUIRE_MIN_ARGS(name, count) if(args.size() < count) throw std::runtime_error(name + std::string(PyRiteMessages::NATIVE_ERROR_REQUIRES_MIN_ARGS_SUFFIX) + #count + " arguments.");
    #define GET_NUM(val, var_name) auto var_name = dynamic_cast<NumberValue*>(val.get()); if(!var_name) throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_ARG_MUST_BE_NUMBER);
    #define GET_LIST(val, var_name) auto var_name = dynamic_cast<ListValue*>(val.get()); if(!var_name) throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_ARG_MUST_BE_LIST);
    #define GET_STR(val, var_name) auto var_name = dynamic_cast<StringValue*>(val.get()); if(!var_name) throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_ARG_MUST_BE_STRING);
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
        if (args.size() < 1 || args.size() > 2) throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_RT_ARGS);
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
        if (args.empty()) throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_MIN_MAX_EMPTY);
        const std::vector<ValuePtr>* values_to_compare;
        std::vector<ValuePtr> temp_list;
        if (args.size() == 1 && dynamic_cast<ListValue*>(args[0].get())) {
             values_to_compare = &dynamic_cast<ListValue*>(args[0].get())->elements;
        } else {
            temp_list = args;
            values_to_compare = &temp_list;
        }
        if (values_to_compare->empty()) throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_MIN_MAX_EMPTY_LIST);
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
                throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_MIN_MAX_UNCOMPARABLE);
            }
        }
        return extreme;
    };
    globals->define("max", std::make_shared<NativeFnValue>("max", [min_max_logic](const std::vector<ValuePtr>& args){ return min_max_logic(args, true); }));
    globals->define("min", std::make_shared<NativeFnValue>("min", [min_max_logic](const std::vector<ValuePtr>& args){ return min_max_logic(args, false); }));
    globals->define("countdown", std::make_shared<NativeFnValue>("countdown", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("countdown", 1);
        GET_NUM(args[0], sec_val);
        auto end_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(sec_val->value.toLongLong() * 1000);
        auto timer_fn_body = [end_time](const std::vector<ValuePtr>& inner_args) -> ValuePtr {
            if (!inner_args.empty()) throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_TIMER_FN_NO_ARGS);
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
        REQUIRE_ARGS("log", 1); GET_NUM(args[0], x); if(x->value <= BigNumber(0)) throw std::runtime_error(PyRiteMessages::NATIVE_ERROR_LOG_POSITIVE);
        return std::make_shared<NumberValue>(BigNumber(std::to_string(log(x->value.toLongLong()))));
    }));
    // Built-in function to create a new instance
    globals->define("new", std::make_shared<NativeFnValue>("new", [](const std::vector<ValuePtr>& args) -> ValuePtr {
         REQUIRE_ARGS("new", 1);
         auto class_val = std::dynamic_pointer_cast<Class>(args[0]);
         if (!class_val) {
             throw std::runtime_error(PyRiteMessages::ERROR_NEW_REQUIRES_CLASS);
         }
         return std::make_shared<Instance>(class_val);
     }));
}

// --- Helper Functions ---
std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r";
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return "";
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}
bool is_simple_identifier(const std::string& s) {
    if (s.empty() || !(isalpha(s[0]) || s[0] == '_')) return false;
    for (char c : s) { if (!isalnum(c) && c != '_') return false; }
    return true;
}
void run_file(const char* filename, Interpreter& interpreter) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << PyRiteMessages::MAIN_FILE_OPEN_ERROR << filename << "'" << std::endl;
        exit(1);
    }
    std::string source_code((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    file.close();
    Parser parser(source_code);
    auto statements = parser.parse();
    if (parser.has_error()) {
        exit(1);
    }
    interpreter.interpret(statements);
}
// Helper to check if a string starts with a prefix
bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}
// Helper to check if a string ends with a suffix
bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// run(...) compile(...) 
std::map<std::string, std::string> parse_function_call(const std::string& call_str) {
    std::map<std::string, std::string> result;
    size_t open_paren = call_str.find('(');
    size_t close_paren = call_str.rfind(')');
    if (close_paren == std::string::npos || open_paren == std::string::npos) {
        result["error"] = PyRiteMessages::COMPILE_SYNTAX_ERROR;
        return result;
    }
    std::string args_str = call_str.substr(open_paren + 1, close_paren - open_paren - 1);
    args_str = trim(args_str);
    if (args_str.empty()) {
        return result; // No arguments
    }
    // Simple parser for key=value arguments, handling quoted strings
    size_t pos = 0;
    bool in_quotes = false;
    size_t arg_start = 0;
    int paren_level = 0; // Handle nested parentheses if needed, though not strictly required here
    for (size_t i = 0; i <= args_str.length(); ++i) {
        char c = (i < args_str.length()) ? args_str[i] : ',';
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (!in_quotes) {
            if (c == '(') {
                paren_level++;
            } else if (c == ')') {
                paren_level--;
            } else if (c == ',' && paren_level == 0) {
                std::string pair = args_str.substr(arg_start, i - arg_start);
                pair = trim(pair);
                size_t eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = trim(pair.substr(0, eq_pos));
                    std::string value = trim(pair.substr(eq_pos + 1));
                    // Remove surrounding quotes if present
                    if (value.length() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
                        value = value.substr(1, value.length() - 2);
                    }
                    result[key] = value;
                } else {
                    // Positional argument or error. We'll treat it as an error for named args.
                    result["error"] = PyRiteMessages::COMPILE_ARG_SYNTAX_ERROR;
                    return result;
                }
                arg_start = i + 1;
            }
        }
    }
    // Handle last argument if there's no trailing comma
    if (arg_start < args_str.length()) {
        std::string pair = args_str.substr(arg_start);
        pair = trim(pair);
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(pair.substr(0, eq_pos));
            std::string value = trim(pair.substr(eq_pos + 1));
            if (value.length() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.length() - 2);
            }
            result[key] = value;
        } else {
            result["error"] = PyRiteMessages::COMPILE_ARG_SYNTAX_ERROR;
        }
    }
    return result;
}

// -- Start!!  --
void run_repl(Interpreter& interpreter) {
    interpreter.repl_buffer.clear();
    int line_number = 1;
    std::vector<std::string> env_stack = {"void"};
    std::cout << PyRiteMessages::REPL_WELCOME_BANNER_1
              << (DEBUG ? PyRiteMessages::REPL_WELCOME_BANNER_DEBUG : "") << PyRiteMessages::REPL_WELCOME_BANNER_2;
    std::cout << PyRiteMessages::REPL_WELCOME_BANNER_3;
    std::cout << std::endl;
    while (true) {
        std::string current_env = env_stack.back();
        std::string env_display = "(" + current_env + ")";
        std::string line_num_str = std::to_string(line_number);
        std::stringstream prompt_ss;
        int total_width = 12;
        int env_len = env_display.length();
        int num_len = line_num_str.length();
        int padding = total_width - env_len - num_len;
        if (padding < 1) padding = 1;
        prompt_ss << env_display << std::string(padding, ' ') << line_num_str;
        std::cout << prompt_ss.str() << "| ";
        std::string line_input;
        if (!std::getline(std::cin, line_input)) break;
        std::string trimmed_line = trim(line_input);
        if (trimmed_line == "halt()") break;
        if (trimmed_line == "about()") {
            std::cout << PyRiteMessages::ABOUT_HEADER_FOOTER
                      << PyRiteMessages::ABOUT_LINE_1 << (DEBUG ? PyRiteMessages::REPL_WELCOME_BANNER_DEBUG : "")
                      << PyRiteMessages::ABOUT_LINE_2
                      << PyRiteMessages::ABOUT_LINE_3
                      << PyRiteMessages::ABOUT_HEADER_FOOTER;
            continue;
        }
        if (starts_with(trimmed_line, "compile(") && ends_with(trimmed_line, ")")) {
            try {
                auto start_time_compile = std::chrono::high_resolution_clock::now();
                #ifdef _WIN32
                const char PATH_SEPARATOR = '\\';
                #else
                const char PATH_SEPARATOR = '/';
                #endif
                // 1. Parse arguments
                auto parsed_args = parse_function_call(trimmed_line);
                if (parsed_args.count("error")) {
                    throw std::runtime_error(parsed_args.at("error"));
                }
                std::string src_path_arg = parsed_args.count("route") ? parsed_args.at("route") : "";
                std::string extra_flags_arg = parsed_args.count("args") ? parsed_args.at("args") : "";
                // Validate types for 'route' and 'args'
                // In this context, they are strings passed from the REPL, so we just check if they exist.
                // The parsing above ensures they are strings.
                // 2. Determine source code, paths, and output names
                std::string source_code;
                std::string output_dir = interpreter.base_path;
                std::string output_stem = "buffer";
                std::string full_src_filename_for_msg = "buffer";
                bool compile_from_buffer = src_path_arg.empty();
                if (compile_from_buffer) {
                    source_code = interpreter.repl_buffer;
                    if (source_code.empty()) {
                        throw std::runtime_error(PyRiteMessages::COMPILE_BUFFER_EMPTY);
                    }
                } else {
                    full_src_filename_for_msg = src_path_arg;
                    std::ifstream src_file(src_path_arg);
                    if (!src_file.is_open()) throw std::runtime_error(std::string(PyRiteMessages::COMPILE_CANNOT_OPEN_SOURCE) + src_path_arg);
                    source_code.assign((std::istreambuf_iterator<char>(src_file)), std::istreambuf_iterator<char>());
                    src_file.close();
                    size_t last_slash = src_path_arg.find_last_of("/\\");
                    output_dir = (last_slash == std::string::npos) ? "." : src_path_arg.substr(0, last_slash);
                    std::string filename = (last_slash == std::string::npos) ? src_path_arg : src_path_arg.substr(last_slash + 1);
                    size_t last_dot = filename.find_last_of(".");
                    output_stem = (last_dot == std::string::npos) ? filename : filename.substr(0, last_dot);
                }
                // Step 4: Read template.cpp
                std::string template_content;
                std::string template_path = interpreter.base_path + PATH_SEPARATOR + "template.cpp";
                std::ifstream template_file(template_path);
                if (!template_file.is_open()) throw std::runtime_error(std::string(PyRiteMessages::COMPILE_CANNOT_OPEN_TEMPLATE) + template_path);
                template_content.assign((std::istreambuf_iterator<char>(template_file)), std::istreambuf_iterator<char>());
                template_file.close();
                // Step 5: Replace placeholder in template
                std::string placeholder = "WRITE_SRC_CODE_HERE";
                size_t pos = template_content.find(placeholder);
                if (pos == std::string::npos) throw std::runtime_error(PyRiteMessages::COMPILE_TEMPLATE_PLACEHOLDER_MISSING);
                template_content.replace(pos, placeholder.length(), source_code);
                // Step 6: Write temporary C++ file and VERIFY the write operation
                std::string temp_cpp_path = output_dir + PATH_SEPARATOR + output_stem + ".cpp";
                std::cout << PyRiteMessages::COMPILE_TRANSLATION_TARGET << temp_cpp_path << std::endl;
                std::ofstream temp_cpp_file(temp_cpp_path);
                if (!temp_cpp_file.is_open()) {
                    throw std::runtime_error(std::string(PyRiteMessages::COMPILE_CANNOT_OPEN_TEMP_WRITE) + temp_cpp_path);
                }
                temp_cpp_file << template_content;
                temp_cpp_file.close();
                if (temp_cpp_file.fail()) {
                    throw std::runtime_error(std::string(PyRiteMessages::COMPILE_WRITE_TEMP_FAILED) + temp_cpp_path);
                }
                // Step 7: Construct and execute compile command
                std::string output_exe_name = output_stem;
                #ifdef _WIN32
                output_exe_name += ".exe";
                #endif
                std::string output_exe_path = output_dir + PATH_SEPARATOR + output_exe_name;
                std::string compiler_path = interpreter.base_path + PATH_SEPARATOR + "compilers" + PATH_SEPARATOR + "MinGW64" + PATH_SEPARATOR + "bin" + PATH_SEPARATOR + "g++.exe";
                std::stringstream cmd;
                cmd << "\"" << compiler_path << "\""
                    << " \"" << temp_cpp_path << "\""
                    << " -o \"" << output_exe_path << "\""
                    << " -I. -std=c++11 -O2 " << extra_flags_arg;
                std::cout << PyRiteMessages::COMPILE_COMMAND_INFO << cmd.str() << std::endl;
                int result = system(("\"" + cmd.str() + "\"").c_str());
                // Step 8: Clean up
                remove(temp_cpp_path.c_str());
                // Step 9: Report result
                auto end_time_compile = std::chrono::high_resolution_clock::now();
                double duration = std::chrono::duration_cast<std::chrono::duration<double>>(end_time_compile - start_time_compile).count();
                if (result == 0) {
                    std::cout << PyRiteMessages::COMPILE_SUCCESS_PREFIX << full_src_filename_for_msg << PyRiteMessages::COMPILE_SUCCESS_TOOK
                              << std::fixed << std::setprecision(2) << duration << PyRiteMessages::COMPILE_SUCCESS_SECONDS << output_exe_path << std::endl;
                } else {
                    throw std::runtime_error(std::string(PyRiteMessages::COMPILE_FAILURE_PREFIX) + full_src_filename_for_msg + PyRiteMessages::COMPILE_FAILURE_SUFFIX);
                }
            } catch (const std::runtime_error& e) {
                std::cerr << PyRiteMessages::COMPILE_ERROR_PREFIX << e.what() << PyRiteMessages::COMPILE_ERROR_SUFFIX << std::endl;
            }
            continue;
        }
        if (is_simple_identifier(trimmed_line)) {
            try {
                ValuePtr val = interpreter.environment->get(trimmed_line);
                std::cout << val->repr() << std::endl;
                continue;
            } catch (const RuntimeError&) { }
        }
        if (starts_with(trimmed_line, "run(") && ends_with(trimmed_line, ")")) {
            if (interpreter.repl_buffer.empty()) {
                std::cout << PyRiteMessages::REPL_NO_CODE_TO_RUN << std::endl;
                continue;
            }
            bool tick_enabled = false;
            long long time_limit = 0;
            // Parse run arguments
            auto parsed_args = parse_function_call(trimmed_line);
            if (parsed_args.count("error")) {
                std::cerr << PyRiteMessages::RUNTIME_ERROR_PREFIX << parsed_args.at("error") << std::endl;
                continue;
            }
            if (parsed_args.count("tick")) {
                std::string tick_val = parsed_args.at("tick");
                if (tick_val == "1" || tick_val == "true") {
                    tick_enabled = true;
                } else if (tick_val != "0" && tick_val != "false") {
                    std::cerr << PyRiteMessages::REPL_TICK_ARG_ERROR << std::endl;
                    continue;
                }
            }
            if (parsed_args.count("limit")) {
                try {
                    std::string limit_str = parsed_args.at("limit");
                    // Check if it's a valid number literal
                    if (!limit_str.empty() && std::all_of(limit_str.begin(), limit_str.end(), [](char c){ return std::isdigit(c) || c == '-'; })) {
                        time_limit = std::stoll(limit_str);
                    } else {
                        // It might be an expression or variable. For simplicity in REPL, we require a literal number.
                         std::cerr << PyRiteMessages::REPL_LIMIT_ARG_ERROR_LITERAL << std::endl;
                         continue;
                    }
                } catch(...) {
                    std::cerr << PyRiteMessages::REPL_LIMIT_ARG_ERROR_INVALID << std::endl;
                    continue;
                }
            }
            Parser parser(interpreter.repl_buffer);
            auto statements = parser.parse();
            if (!parser.has_error()) {
                auto start_time = std::chrono::high_resolution_clock::now();
                interpreter.start_time = start_time;
                interpreter.time_limit_ms = time_limit;
                interpreter.interpret(statements);
                if (tick_enabled) {
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                    std::cout << PyRiteMessages::REPL_EXECUTION_TIME_PREFIX << duration << PyRiteMessages::REPL_EXECUTION_TIME_SUFFIX << std::endl;
                }
            }
            interpreter.repl_buffer.clear();
            line_number = 1;
            env_stack = {"void"};
            std::cout << std::endl;
            continue;
        }
        std::string first_word;
        size_t word_end = trimmed_line.find_first_of(" \t\n\r(");
        first_word = (word_end == std::string::npos) ? trimmed_line : trimmed_line.substr(0, word_end);
        if (first_word == "if" || first_word == "while" || first_word == "def" || first_word == "await" || first_word == "try" || first_word == "ins") {
            env_stack.push_back(first_word);
        } else if (first_word == "endif") {
            if (env_stack.size() > 1 && env_stack.back() == "if") env_stack.pop_back();
        } else if (first_word == "endwhile") {
            if (env_stack.size() > 1 && env_stack.back() == "while") env_stack.pop_back();
        } else if (first_word == "enddef") {
            if (env_stack.size() > 1 && env_stack.back() == "def") env_stack.pop_back();
        } else if (first_word == "endawait") {
             if (env_stack.size() > 1 && env_stack.back() == "await") env_stack.pop_back();
        } else if (first_word == "endtry") {
             if (env_stack.size() > 1 && env_stack.back() == "try") env_stack.pop_back();
        } else if (first_word == "endins") {
             if (env_stack.size() > 1 && env_stack.back() == "ins") env_stack.pop_back();
        }
        if (!trimmed_line.empty() && trimmed_line.front() == '$') {
            std::string code_to_run;
            bool is_temp_exec_only = false;
            if (trimmed_line.length() > 1 && trimmed_line[1] == '#') {
                code_to_run = trimmed_line.substr(2);
                is_temp_exec_only = true;
            } else {
                code_to_run = trimmed_line.substr(1);
            }
            Parser p(code_to_run);
            auto stmts = p.parse();
            if (!p.has_error()) interpreter.interpret(stmts);
            if (is_temp_exec_only) {
                interpreter.repl_buffer += "#" + code_to_run + "#\n";
            } else {
                interpreter.repl_buffer += line_input + "\n";
            }
            line_number++;
            continue;
        }
        interpreter.repl_buffer += line_input + "\n";
        line_number++;
    }
    std::cout << PyRiteMessages::REPL_HALTED << std::endl;
}

// -- main entrance -- 
int main(int argc, char* argv[]) {
    Interpreter interpreter;
    std::string executable_path = argv[0];
    size_t last_slash_pos = executable_path.find_last_of("/\\");
    std::string executable_dir = ".";
    if (std::string::npos != last_slash_pos) {
        executable_dir = executable_path.substr(0, last_slash_pos);
    }
    interpreter.base_path = executable_dir;
    if (argc > 2) {
        std::cerr << PyRiteMessages::MAIN_USAGE_ERROR << argv[0] << PyRiteMessages::MAIN_USAGE_ERROR_SCRIPT << std::endl;
        return 1;
    } else if (argc == 2) {
        run_file(argv[1], interpreter);
    } else {
        run_repl(interpreter);
    }
    return 0;
}
