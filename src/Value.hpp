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

// A reference-counted pointer with a plain (non-atomic) counter on the
// pointee. The runtime is single-threaded, and profiling showed the
// atomic increment/decrement of std::shared_ptr dominating the hot
// interpreter paths; here a count change is a single add instruction.
template <typename T>
class Ptr {
public:
    Ptr() : p_(nullptr) {}
    Ptr(T* p) : p_(p) { inc(); }
    Ptr(const Ptr& o) : p_(o.p_) { inc(); }
    Ptr(Ptr&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    // Upcast: a derived value pointer converts to its base type.
    template <typename U>
    Ptr(const Ptr<U>& o, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0) : p_(o.get()) { inc(); }
    template <typename U>
    Ptr(Ptr<U>&& o, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0) noexcept : p_(o.release()) {}
    Ptr& operator=(const Ptr& o) {
        if (this != &o) { T* old = p_; p_ = o.p_; inc(); if (old) dec_ref(old); }
        return *this;
    }
    template <typename U>
    Ptr& operator=(Ptr<U>&& o) {
        if (p_ != o.get()) { T* old = p_; p_ = o.release(); if (old) dec_ref(old); }
        return *this;
    }
    Ptr& operator=(Ptr&& o) noexcept {
        if (this != &o) { T* old = p_; p_ = o.p_; o.p_ = nullptr; if (old) dec_ref(old); }
        return *this;
    }
    ~Ptr() { if (p_) dec_ref(p_); }

    T* get() const { return p_; }
    T* release() { T* t = p_; p_ = nullptr; return t; }
    int use_count() const { return p_ ? p_->refcount_ : 0; }
    T& operator*() const { return *p_; }
    T* operator->() const { return p_; }
    explicit operator bool() const { return p_ != nullptr; }
    bool operator==(const Ptr& o) const { return p_ == o.p_; }
    bool operator!=(const Ptr& o) const { return p_ != o.p_; }
    void swap(Ptr& o) { T* t = p_; p_ = o.p_; o.p_ = t; }

private:
    void inc() { if (p_) ++p_->refcount_; }
    static void dec_ref(T* p) { if (--p->refcount_ == 0) delete p; }
    T* p_;
};

template <typename T, typename... A>
static inline Ptr<T> make_value(A&&... a) {
    return Ptr<T>(new T(std::forward<A>(a)...));
}

// Pointer casts between value types, mirroring the shared_ptr helpers.
template <typename T, typename U>
static inline Ptr<T> ptr_cast(const Ptr<U>& v) {
    return Ptr<T>(dynamic_cast<T*>(v.get()));
}
template <typename T, typename U>
static inline Ptr<T> ptr_static_cast(const Ptr<U>& v) {
    return Ptr<T>(static_cast<T*>(v.get()));
}

using ValuePtr = Ptr<class Value>;

// The key that marks a table descriptor.
//
// A table of literals is compiled into ONE constant whose value lists the
// slots of the rows.  The runtime opens a row when the script reads it,
// so a whole table of dialogue never sits in memory at once.
//
// The key starts with a control character, which no script can type, so
// no dictionary a script writes can collide with it.
inline const char* table_key() { return "\x01" "table"; }
using ClassPtr = Ptr<Class>;
using InstancePtr = Ptr<Instance>;

// Runtime type tags. A tag read from the object beats a dynamic_cast in
// the hot arithmetic paths: no typeinfo walk, no virtual call.
enum class VTag : uint8_t {
    Null, Number, String, Binary, Ln, Dim,
    Function, Closure, NativeFn, BoundMethod, Exception, ModuleProxy,
    Class, Instance
};

struct Value {
    const VTag tag;
    mutable int refcount_ = 0;      // plain counter: single-threaded runtime
    Value(VTag t) : tag(t) {}
    ValuePtr self() const { return ValuePtr(const_cast<Value*>(this)); }
    virtual ~Value() {}
    VTag kind() const { return tag; }
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
    // The members a value answers to: "abc".upper(), xs.push(1),
    // d.keys().  They live with the value and not with the runtime
    // around it, so the interpreter and the bytecode VM offer the
    // same ones.
    virtual ValuePtr getMember(const std::string& name) const { return nullptr; }
    virtual void setSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step, ValuePtr value);
};

class NullValue : public Value {
public:
    NullValue() : Value(VTag::Null) {}
    std::string toString() const override { return "null"; }
    std::string repr() const override;
    bool isTruthy() const override { return false; }
    ValuePtr clone() const override { return make_value<NullValue>(); }
    bool isEqualTo(const Value& other) const override;
};

class NumberValue : public Value {
public:
    BigNumber value;
    NumberValue(const BigNumber& n) : Value(VTag::Number), value(n) {}
    std::string toString() const override { return value.toString(); }
    std::string repr() const override { return value.toString(); }
    bool isTruthy() const override { return !value.isZero(); }
    ValuePtr clone() const override { return make_value<NumberValue>(value); }
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
    BinaryValue(const std::vector<uint8_t>& v) : Value(VTag::Binary), value(v) {}
    BinaryValue(const std::string& hex_str);
    std::string toString() const override;
    std::string repr() const override { return toString(); }
    bool isTruthy() const override;
    ValuePtr clone() const override { return make_value<BinaryValue>(value); }
    ValuePtr add(const Value& other) const override;
    bool isEqualTo(const Value& other) const override;
    BigNumber toBigNumber() const;
};

class StringValue : public Value {
public:
    std::string value;
    StringValue(const std::string& s) : Value(VTag::String), value(s) {}
    std::string toString() const override { return value; }
    std::string repr() const override;
    bool isTruthy() const override { return !value.empty(); }
    ValuePtr clone() const override { return make_value<StringValue>(value); }
    ValuePtr add(const Value& other) const override;
    bool isEqualTo(const Value& other) const override;
    bool isLessThan(const Value& other) const override;
    ValuePtr getSubscript(const Value& index) const override;
    ValuePtr getSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step) const override;
    ValuePtr getMember(const std::string& name) const override;
};

class LnValue : public Value {
public:
    std::vector<ValuePtr> elements;
    LnValue(const std::vector<ValuePtr>& e) : Value(VTag::Ln), elements(e) {}
    std::string toString() const override;
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return !elements.empty(); }
    ValuePtr clone() const override {
        std::vector<ValuePtr> cloned;
        cloned.reserve(elements.size());
        for (const auto& elem : elements) cloned.push_back(elem->clone());
        return make_value<LnValue>(cloned);
    }
    ValuePtr add(const Value& other) const override;
    ValuePtr multiply(const Value& other) const override;
    bool isEqualTo(const Value& other) const override;
    ValuePtr getSubscript(const Value& index) const override;
    void setSubscript(const Value& index, ValuePtr value) override;
    ValuePtr getSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step) const override;
    void setSlice(const ValuePtr& start, const ValuePtr& end, const ValuePtr& step, ValuePtr value) override;
    ValuePtr getMember(const std::string& name) const override;
    // Stack/queue operations
    ValuePtr push_pop(const Value* arg = nullptr) const;
    ValuePtr shift_unshift(const Value* arg = nullptr) const;
};

class DimValue : public Value {
public:
    std::map<std::string, ValuePtr> dict;
    DimValue(const std::map<std::string, ValuePtr>& d) : Value(VTag::Dim), dict(d) {}
    DimValue() : Value(VTag::Dim) {}
    std::string toString() const override;
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return !dict.empty(); }
    ValuePtr clone() const override;
    ValuePtr getSubscript(const Value& index) const override;
    void setSubscript(const Value& index, ValuePtr value) override;
    bool isEqualTo(const Value& other) const override;
    ValuePtr getMember(const std::string& name) const override;
};

class FunctionValue : public Value {
public:
    std::shared_ptr<Function> value;
    FunctionValue(const std::shared_ptr<Function>& f) : Value(VTag::Function), value(f) {}
    std::string toString() const override;
    std::string repr() const override;
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return make_value<FunctionValue>(value); }
};

class NativeFnValue : public Value {
public:
    using NativeFn = std::function<ValuePtr(const std::vector<ValuePtr>&)>;
    NativeFn fn;
    std::string name;
    NativeFnValue(const std::string& n, NativeFn f) : Value(VTag::NativeFn), name(n), fn(f) {}
    std::string toString() const override { return "<native function " + name + ">"; }
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return make_value<NativeFnValue>(name, fn); }
    ValuePtr call(const std::vector<ValuePtr>& args) const { return fn(args); }
};

class BoundMethodValue : public Value {
public:
    InstancePtr instance;
    std::shared_ptr<Function> method;
    BoundMethodValue(InstancePtr inst, std::shared_ptr<Function> m);
    // Out of line: Instance is only forward-declared here, and the
    // pointer's destructor must see the complete type to drop the count.
    ~BoundMethodValue() override;
    std::string toString() const override;
    std::string repr() const override;
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override;
};

class ExceptionValue : public Value {
public:
    ValuePtr payload;
    ExceptionValue(ValuePtr p) : Value(VTag::Exception), payload(p) {}
    std::string toString() const override { return "<Exception: " + payload->toString() + ">"; }
    std::string repr() const override;
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return make_value<ExceptionValue>(payload->clone()); }
    bool isEqualTo(const Value& other) const override;
};

class ModuleProxy : public Value {
public:
    std::string file_path;
    bool loaded;
    ModuleProxy(const std::string& path) : Value(VTag::ModuleProxy), file_path(path), loaded(false) {}
    std::string toString() const override { return "<module " + file_path + ">"; }
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return make_value<ModuleProxy>(file_path); }
};

// Type checking helpers
bool is_type_compatible(TokenType expected_type, const ValuePtr& value);
std::string token_type_to_string(TokenType type);

// Slicing helpers
long long value_to_long(const ValuePtr& val_ptr, long long default_val);
struct SliceParams { long long start; long long stop; long long step; };
SliceParams calculate_slice_indices(long long start, long long stop, long long step, long long len);
