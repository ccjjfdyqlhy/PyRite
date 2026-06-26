#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <cstdint>
#include "BigNumber.hpp"
#include "Tokenizer.hpp"

struct Function;
struct Class;
struct Instance;
class Environment;

using ValuePtr = std::shared_ptr<class Value>;
using ClassPtr = std::shared_ptr<Class>;
using InstancePtr = std::shared_ptr<Instance>;

struct Value {
    virtual ~Value() {}
    virtual std::string toString() const = 0;
    virtual std::string repr() const = 0;
    virtual bool isTruthy() const = 0;
    virtual ValuePtr clone() const = 0;
    virtual ValuePtr add(const Value& other) const;
    virtual ValuePtr subtract(const Value& other) const;
    virtual ValuePtr multiply(const Value& other) const;
    virtual ValuePtr divide(const Value& other) const;
    virtual ValuePtr power(const Value& other) const;
    virtual ValuePtr modulo(const Value& other) const;
    virtual bool isEqualTo(const Value& other) const;
    virtual bool isLessThan(const Value& other) const;
    virtual ValuePtr getSubscript(const Value& index) const;
    virtual void setSubscript(const Value& index, ValuePtr value);
    virtual ValuePtr getSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step) const;
    virtual void setSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step, ValuePtr value);
};

class NullValue : public Value {
public:
    std::string toString() const override { return "null"; }
    std::string repr() const override;
    bool isTruthy() const override { return false; }
    ValuePtr clone() const override { return std::make_shared<NullValue>(); }
    bool isEqualTo(const Value& other) const override;
};

class NumberValue : public Value {
public:
    BigNumber value;
    NumberValue(const BigNumber& n) : value(n) {}
    std::string toString() const override { return value.toString(); }
    std::string repr() const override { return value.toString(); }
    bool isTruthy() const override { return value != BigNumber(0); }
    ValuePtr clone() const override { return std::make_shared<NumberValue>(value); }
    ValuePtr add(const Value& other) const override;
    ValuePtr subtract(const Value& other) const override;
    ValuePtr multiply(const Value& other) const override;
    ValuePtr divide(const Value& other) const override;
    ValuePtr power(const Value& other) const override;
    ValuePtr modulo(const Value& other) const override;
    bool isEqualTo(const Value& other) const override;
    bool isLessThan(const Value& other) const override;
};

class BinaryValue : public Value {
public:
    std::vector<uint8_t> value;
    BinaryValue(const std::vector<uint8_t>& v) : value(v) {}
    BinaryValue(const std::string& hex_str);
    std::string toString() const override;
    std::string repr() const override { return toString(); }
    bool isTruthy() const override;
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
    std::string repr() const override;
    bool isTruthy() const override { return !value.empty(); }
    ValuePtr clone() const override { return std::make_shared<StringValue>(value); }
    ValuePtr add(const Value& other) const override;
    bool isEqualTo(const Value& other) const override;
    bool isLessThan(const Value& other) const override;
    ValuePtr getSubscript(const Value& index) const override;
    ValuePtr getSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step) const override;
};

class LnValue : public Value {
public:
    std::vector<ValuePtr> elements;
    LnValue(const std::vector<ValuePtr>& e) : elements(e) {}
    std::string toString() const override;
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return !elements.empty(); }
    ValuePtr clone() const override {
        std::vector<ValuePtr> cloned;
        cloned.reserve(elements.size());
        for (const auto& elem : elements) cloned.push_back(elem->clone());
        return std::make_shared<LnValue>(cloned);
    }
    ValuePtr add(const Value& other) const override;
    ValuePtr multiply(const Value& other) const override;
    bool isEqualTo(const Value& other) const override;
    ValuePtr getSubscript(const Value& index) const override;
    void setSubscript(const Value& index, ValuePtr value) override;
    ValuePtr getSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step) const override;
    void setSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step, ValuePtr value) override;
    // Stack/queue operations
    ValuePtr push_pop(const Value* arg = nullptr) const;
    ValuePtr shift_unshift(const Value* arg = nullptr) const;
};

class DimValue : public Value {
public:
    std::map<std::string, ValuePtr> dict;
    DimValue(const std::map<std::string, ValuePtr>& d) : dict(d) {}
    DimValue() {}
    std::string toString() const override;
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return !dict.empty(); }
    ValuePtr clone() const override;
    ValuePtr getSubscript(const Value& index) const override;
    void setSubscript(const Value& index, ValuePtr value) override;
    bool isEqualTo(const Value& other) const override;
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

class BoundMethodValue : public Value {
public:
    InstancePtr instance;
    std::shared_ptr<Function> method;
    BoundMethodValue(InstancePtr inst, std::shared_ptr<Function> m) : instance(inst), method(m) {}
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
    std::string repr() const override;
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return std::make_shared<ExceptionValue>(payload->clone()); }
    bool isEqualTo(const Value& other) const override;
};

class ModuleProxy : public Value {
public:
    std::string file_path;
    bool loaded;
    ModuleProxy(const std::string& path) : file_path(path), loaded(false) {}
    std::string toString() const override { return "<module " + file_path + ">"; }
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return std::make_shared<ModuleProxy>(file_path); }
};

// Type checking helpers
bool is_type_compatible(TokenType expected_type, const ValuePtr& value);
std::string token_type_to_string(TokenType type);

// Slicing helpers
long long value_to_long(const ValuePtr& val_ptr, long long default_val);
struct SliceParams { long long start; long long stop; long long step; };
SliceParams calculate_slice_indices(long long start, long long stop, long long step, long long len);
