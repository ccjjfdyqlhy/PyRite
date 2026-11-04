#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
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

// Open-addressing hash map for dictionary values. One flat slot array
// (power-of-two capacity, linear probing, 0.75 load) instead of a
// node-per-entry unordered_map: a lookup is a hash plus one or two loads
// with no pointer chase and no per-entry allocation. Slots expose
// first/second so every map-shaped call site keeps its form. h == 0
// marks an empty slot; iteration order is unspecified (all order-bound
// reads go through sorted_entries()).
class FlatDict {
public:
    struct Slot {
        std::string first;
        ValuePtr second;
        uint64_t h = 0;
    };
    struct iterator {
        Slot* p; Slot* e;
        void skip() { while (p < e && p->h == 0) ++p; }
        iterator& operator++() { ++p; skip(); return *this; }
        bool operator!=(const iterator& o) const { return p != o.p; }
        bool operator==(const iterator& o) const { return p == o.p; }
        Slot& operator*() const { return *p; }
        Slot* operator->() const { return p; }
    };
    struct const_iterator {
        const Slot* p; const Slot* e;
        void skip() { while (p < e && p->h == 0) ++p; }
        const_iterator& operator++() { ++p; skip(); return *this; }
        bool operator!=(const const_iterator& o) const { return p != o.p; }
        bool operator==(const const_iterator& o) const { return p == o.p; }
        const Slot& operator*() const { return *p; }
        const Slot* operator->() const { return p; }
    };

    FlatDict() { slots_.resize(8); }
    size_t size() const { return used_; }
    bool empty() const { return used_ == 0; }
    iterator begin() { iterator it{slots_.data(), slots_.data() + slots_.size()}; it.skip(); return it; }
    iterator end() { return iterator{slots_.data() + slots_.size(), slots_.data() + slots_.size()}; }
    const_iterator begin() const { const_iterator it{slots_.data(), slots_.data() + slots_.size()}; it.skip(); return it; }
    const_iterator end() const { return const_iterator{slots_.data() + slots_.size(), slots_.data() + slots_.size()}; }

    iterator find(const std::string& key) {
        if (used_ == 0) return end();
        uint64_t h = hash_str(key);
        size_t mask = slots_.size() - 1;
        size_t i = (size_t)h & mask;
        while (slots_[i].h != 0) {
            if (slots_[i].h == h && slots_[i].first == key) return iterator{&slots_[i], slots_.data() + slots_.size()};
            i = (i + 1) & mask;
        }
        return end();
    }
    const_iterator find(const std::string& key) const {
        if (used_ == 0) return end();
        uint64_t h = hash_str(key);
        size_t mask = slots_.size() - 1;
        size_t i = (size_t)h & mask;
        while (slots_[i].h != 0) {
            if (slots_[i].h == h && slots_[i].first == key) return const_iterator{&slots_[i], slots_.data() + slots_.size()};
            i = (i + 1) & mask;
        }
        return end();
    }
    size_t count(const std::string& key) const {
        if (used_ == 0) return 0;
        uint64_t h = hash_str(key);
        size_t mask = slots_.size() - 1;
        size_t i = (size_t)h & mask;
        while (slots_[i].h != 0) {
            if (slots_[i].h == h && slots_[i].first == key) return 1;
            i = (i + 1) & mask;
        }
        return 0;
    }
    ValuePtr& operator[](const std::string& key) {
        if ((used_ + 1) * 4 > slots_.size() * 3) rehash(slots_.size() * 2);
        uint64_t h = hash_str(key);
        size_t mask = slots_.size() - 1;
        size_t i = (size_t)h & mask;
        while (slots_[i].h != 0) {
            if (slots_[i].h == h && slots_[i].first == key) return slots_[i].second;
            i = (i + 1) & mask;
        }
        slots_[i].h = h;
        slots_[i].first = key;
        used_++;
        return slots_[i].second;
    }

private:
    static uint64_t hash_str(const std::string& s) {
        uint64_t h = 1469598103934665603ULL;
        for (size_t i = 0; i < s.size(); ++i) { h ^= (unsigned char)s[i]; h *= 1099511628211ULL; }
        h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33;
        return h | 1ULL;                     // 0 is the empty marker
    }
    void rehash(size_t ncap) {
        std::vector<Slot> old;
        old.swap(slots_);
        slots_.resize(ncap);
        used_ = 0;
        size_t mask = ncap - 1;
        for (const Slot& s : old) {
            if (s.h == 0) continue;
            size_t i = (size_t)s.h & mask;
            while (slots_[i].h != 0) i = (i + 1) & mask;
            slots_[i].h = s.h;
            slots_[i].first = std::move(const_cast<std::string&>(s.first));
            slots_[i].second = std::move(const_cast<ValuePtr&>(s.second));
            used_++;
        }
    }
    std::vector<Slot> slots_;
    size_t used_ = 0;
};

class DimValue : public Value {
public:
    // Hashed storage: dictionary lookup and insert are the hot paths of
    // loop-shaped scripts. Every operation that exposes an ORDER goes
    // through sorted_entries(), so observable behavior stays exactly the
    // sorted order an ordered map produced.
    FlatDict dict;
    DimValue(const FlatDict& d) : Value(VTag::Dim), dict(d) {}
    DimValue() : Value(VTag::Dim) {}
    std::vector<std::pair<std::string, ValuePtr>> sorted_entries() const {
        std::vector<std::pair<std::string, ValuePtr>> out;
        out.reserve(dict.size());
        for (const auto& kv : dict) out.emplace_back(kv.first, kv.second);
        std::sort(out.begin(), out.end(),
                  [](const std::pair<std::string, ValuePtr>& a,
                     const std::pair<std::string, ValuePtr>& b) { return a.first < b.first; });
        return out;
    }
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
    // Set when the method is a bytecode closure (OP_DEF_CLASS): the
    // receiver rides in the closure frame's slot 0 at call time.
    Ptr<struct Closure> bc_method;
    BoundMethodValue(InstancePtr inst, std::shared_ptr<Function> m);
    BoundMethodValue(InstancePtr inst, Ptr<struct Closure> m);
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

// The last reference to a Value drops through here. Numbers and strings
// are the overwhelming majority of every workload's garbage, so their
// destruction is devirtualized: a direct destructor call plus the pooled
// delete instead of a vtable dispatch per object.
void value_destroy(Value* p);
template <>
inline void Ptr<Value>::dec_ref(Value* p) {
    if (--p->refcount_ != 0) return;
    VTag t = p->tag;
    if (t == VTag::Number) {
        NumberValue* n = static_cast<NumberValue*>(p);
        n->~NumberValue();
        ::operator delete((void*)n);
    } else if (t == VTag::String) {
        StringValue* s = static_cast<StringValue*>(p);
        s->~StringValue();
        ::operator delete((void*)s);
    } else {
        value_destroy(p);
    }
}

// ============================================================================
// Interned small integers.
//
// A machine integer that has to be boxed again - because it reaches a
// global, a native call or the AST interpreter - comes back out of this
// table instead of the heap: the table covers [-4096, 4095], which is
// where loop counters and small results live.  Numbers are immutable, so
// a cached object never goes stale.
// ============================================================================
ValuePtr intern_small_int(long long v);
// The same table, borrowed: no reference counting, for the paths that
// only read the number.
Value* intern_small_int_raw(long long v);

// ============================================================================
// Slot: one cell of the register file or of the operand stack.  It holds
// either a boxed value or a raw machine integer.
//
// Machine integers are the common case in numeric code, and a boxed one is
// expensive: NumberValue is 160 bytes because BigNumber carries a 144-byte
// inline limb buffer.  Keeping the integer inline removes that object
// completely - the arithmetic paths then never touch BigNumber at all.  Only
// the boundaries (native calls, the AST bridge, globals, closures) box an
// integer again.
// ============================================================================
struct Slot {
    static constexpr uint8_t REF = 0;
    static constexpr uint8_t INT = 1;

    ValuePtr ref;      // the boxed value; null when the slot holds an int
    int64_t num;       // the machine integer; valid when kind == INT
    uint8_t kind;      // REF or INT

    Slot() : ref(), num(0), kind(REF) {}
    Slot(ValuePtr v) : ref(std::move(v)), num(0), kind(REF) {}
    Slot(const Slot& o) : ref(o.ref), num(o.num), kind(o.kind) {}
    Slot(Slot&& o) noexcept : ref(std::move(o.ref)), num(o.num), kind(o.kind) {}
    Slot& operator=(const Slot& o) { ref = o.ref; num = o.num; kind = o.kind; return *this; }
    Slot& operator=(Slot&& o) noexcept {
        ref = std::move(o.ref); num = o.num; kind = o.kind; return *this;
    }

    static Slot of_int(int64_t v) { Slot s; s.kind = INT; s.num = v; return s; }

    bool is_int() const { return kind == INT; }
    int64_t int_value() const { return num; }

    // The boxed form of this slot.  An integer becomes a NumberValue; the
    // caller owns the result.  A small integer comes out of the intern
    // table, so boxing it does not allocate.
    ValuePtr to_value() const {
        if (kind == REF) return ref;
        if (num >= -4096 && num <= 4095) return intern_small_int(num);
        return make_value<NumberValue>(BigNumber(num));
    }

    // Moves the boxed value out of the slot, leaving it empty.  Moving
    // instead of copying keeps the reference count where it was, so the
    // in-place arithmetic can still rewrite a uniquely owned object.
    ValuePtr take_value() {
        if (kind == REF) {
            ValuePtr v = std::move(ref);
            kind = REF;
            num = 0;
            return v;
        }
        ValuePtr v = to_value();      // a small integer comes from the intern table
        kind = REF;
        num = 0;
        return v;
    }
};

// Type checking helpers
bool is_type_compatible(TokenType expected_type, const ValuePtr& value);
std::string token_type_to_string(TokenType type);

// Slicing helpers
long long value_to_long(const ValuePtr& val_ptr, long long default_val);
struct SliceParams { long long start; long long stop; long long step; };
SliceParams calculate_slice_indices(long long start, long long stop, long long step, long long len);
