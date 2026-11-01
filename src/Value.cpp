#include "Value.hpp"
#include "Environment.hpp"
#include "Ast.hpp"
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "msg_cn.hpp"
#include "msgs.hpp"

// NullValue
std::string NullValue::repr() const {
    std::stringstream ss;
    ss << "<NullObject at " << static_cast<const void*>(this) << ">";
    return ss.str();
}
bool NullValue::isEqualTo(const Value& other) const {
    return dynamic_cast<const NullValue*>(&other) != nullptr;
}

// --- Slicing helper ---
long long value_to_long(const ValuePtr& val_ptr, long long default_val) {
    if (!val_ptr || dynamic_cast<NullValue*>(val_ptr.get())) {
        return default_val;
    }
    if (auto num_val = dynamic_cast<NumberValue*>(val_ptr.get())) {
        try {
            return num_val->value.toLongLong();
        } catch (...) {
            throw std::runtime_error(msg(Msg::SLICE_TOO_LARGE));
        }
    }
    throw std::runtime_error(msg(Msg::SLICE_MUST_NUM));
}
SliceParams calculate_slice_indices(long long start, long long stop, long long step, long long len) {
    if (step == 0) {
        throw std::runtime_error(msg(Msg::SLICE_STEP_ZERO));
    }
    if (step > 0) {
        if (start < 0) start += len;
        if (stop < 0) stop += len;
        start = std::max(0LL, std::min(len, start));
        stop = std::max(0LL, std::min(len, stop));
    } else {
        if (start < 0) start += len;
        if (stop < 0) stop += len;
        start = std::max(-1LL, std::min(len - 1, start));
        stop = std::max(-1LL, std::min(len - 1, stop));
    }
    return {start, stop, step};
}

// --- Type checking helpers ---
bool is_type_compatible(TokenType expected_type, const ValuePtr& value) {
    switch (expected_type) {
        case TokenType::ANY: return true;
        case TokenType::DEC: return dynamic_cast<NumberValue*>(value.get()) != nullptr;
        case TokenType::STR: return dynamic_cast<StringValue*>(value.get()) != nullptr;
        case TokenType::BIN: return dynamic_cast<BinaryValue*>(value.get()) != nullptr;
        case TokenType::LN: return dynamic_cast<LnValue*>(value.get()) != nullptr;
        case TokenType::DIM: return dynamic_cast<DimValue*>(value.get()) != nullptr;
        default: return true;
    }
}
std::string token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::ANY: return "any";
        case TokenType::DEC: return "dec";
        case TokenType::STR: return "str";
        case TokenType::BIN: return "bin";
        case TokenType::LN: return "ln";
        case TokenType::DIM: return "dim";
        default: return "unknown";
    }
}

// --- Default Value implementations ---
ValuePtr Value::add(const Value&) const { throw std::runtime_error(msg(Msg::ADD_TYPE)); }
ValuePtr Value::subtract(const Value&) const { throw std::runtime_error(msg(Msg::SUB_TYPE)); }
ValuePtr Value::multiply(const Value&) const { throw std::runtime_error(msg(Msg::MUL_TYPE)); }
ValuePtr Value::divide(const Value&) const { throw std::runtime_error(msg(Msg::DIV_TYPE)); }
ValuePtr Value::power(const Value&) const { throw std::runtime_error(msg(Msg::POW_TYPE)); }
ValuePtr Value::modulo(const Value&) const { throw std::runtime_error(msg(Msg::MOD_TYPE)); }
bool Value::isEqualTo(const Value&) const { return false; }
bool Value::isLessThan(const Value&) const { throw std::runtime_error(msg(Msg::CMP_TYPE)); }
ValuePtr Value::getSubscript(const Value&) const { throw std::runtime_error(msg(Msg::NOT_SUBSCR)); }
void Value::setSubscript(const Value&, ValuePtr) { throw std::runtime_error(msg(Msg::NO_ITEM_SET)); }
ValuePtr Value::getSlice(const ValuePtr&, const ValuePtr&, const ValuePtr&) const { throw std::runtime_error(msg(Msg::SLICE_UNSUPPORTED)); }
void Value::setSlice(const ValuePtr&, const ValuePtr&, const ValuePtr&, ValuePtr) { throw std::runtime_error(msg(Msg::SLICE_ASSIGN)); }

// NumberValue
ValuePtr NumberValue::add(const Value& other) const {
    if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return make_value<NumberValue>(this->value + o->value);
    if (const BinaryValue* o = dynamic_cast<const BinaryValue*>(&other)) return make_value<NumberValue>(this->value + o->toBigNumber());
    return make_value<StringValue>(this->toString() + other.toString());
}
ValuePtr NumberValue::subtract(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return make_value<NumberValue>(this->value - o->value); return Value::subtract(other); }
ValuePtr NumberValue::multiply(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return make_value<NumberValue>(this->value * o->value); return Value::multiply(other); }
ValuePtr NumberValue::divide(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return make_value<NumberValue>(this->value / o->value); return Value::divide(other); }
ValuePtr NumberValue::power(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return make_value<NumberValue>(this->value ^ o->value); return Value::power(other); }
ValuePtr NumberValue::modulo(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return make_value<NumberValue>(this->value % o->value); return Value::modulo(other); }
bool NumberValue::isEqualTo(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return this->value == o->value; if (const BinaryValue* o = dynamic_cast<const BinaryValue*>(&other)) return this->value == o->toBigNumber(); return false; }
bool NumberValue::isLessThan(const Value& other) const { if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return this->value < o->value; return Value::isLessThan(other); }

// BinaryValue
BinaryValue::BinaryValue(const std::string& hex_str) : Value(VTag::Binary) {
    if (hex_str.rfind("0x", 0) != 0) throw std::invalid_argument(msg(Msg::HEX_PREFIX));
    std::string hex = hex_str.substr(2);
    if (hex.length() % 2 != 0) hex = "0" + hex;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        value.push_back((uint8_t)std::stoul(byteString, nullptr, 16));
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
bool BinaryValue::isTruthy() const {
    for (auto byte : value) { if (byte != 0) return true; }
    return false;
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
    if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return make_value<NumberValue>(this->toBigNumber() + o->value);
    return make_value<StringValue>(this->toString() + other.toString());
}
bool BinaryValue::isEqualTo(const Value& other) const {
    if (const BinaryValue* o = dynamic_cast<const BinaryValue*>(&other)) return this->value == o->value;
    if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) return this->toBigNumber() == o->value;
    return false;
}

// StringValue
ValuePtr StringValue::add(const Value& other) const { return make_value<StringValue>(this->value + other.toString()); }
bool StringValue::isEqualTo(const Value& other) const { if (const StringValue* o = dynamic_cast<const StringValue*>(&other)) return this->value == o->value; return false; }
bool StringValue::isLessThan(const Value& other) const { if (const StringValue* o = dynamic_cast<const StringValue*>(&other)) return this->value < o->value; return Value::isLessThan(other); }
ValuePtr StringValue::getSubscript(const Value& index) const {
    if (const NumberValue* num = dynamic_cast<const NumberValue*>(&index)) {
        long long i = num->value.toLongLong();
        long long len = static_cast<long long>(this->value.size());
        if (i < 0) i += len;
        if (i < 0 || i >= len) {
            throw std::runtime_error(msg(Msg::LN_IDX_OOB));
        }
        return make_value<StringValue>(std::string(1, this->value[i]));
    }
    throw std::runtime_error(msg(Msg::LN_IDX_NUM));
}
std::string StringValue::repr() const {
    // The browser runtime prints a string inside a list with double
    // quotes, so the same list has to look the same in both.
    std::stringstream ss;
    ss << '"' << value << '"';
    return ss.str();
}
ValuePtr StringValue::getSlice(const ValuePtr& start_val, const ValuePtr& end_val, const ValuePtr& step_val) const {
    long long len = this->value.length();
    long long step = value_to_long(step_val, 1);
    long long start = value_to_long(start_val, (step > 0) ? 0 : len - 1);
    long long end = value_to_long(end_val, (step > 0) ? len : -1);
    SliceParams params = calculate_slice_indices(start, end, step, len);
    std::string result_str = "";
    if (params.step > 0) {
        for (long long i = params.start; i < params.stop; i += params.step) result_str += this->value[i];
    } else {
        for (long long i = params.start; i > params.stop; i += params.step) result_str += this->value[i];
    }
    return make_value<StringValue>(result_str);
}

// LnValue
std::string LnValue::toString() const {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < elements.size(); ++i) {
        ss << elements[i]->repr();
        if (i < elements.size() - 1) ss << ", ";
    }
    ss << "]";
    return ss.str();
}
ValuePtr LnValue::add(const Value& other) const {
    if (const LnValue* o = dynamic_cast<const LnValue*>(&other)) {
        auto new_elements = this->elements;
        new_elements.insert(new_elements.end(), o->elements.begin(), o->elements.end());
        return make_value<LnValue>(new_elements);
    }
    return Value::add(other);
}
ValuePtr LnValue::multiply(const Value& other) const {
    if (const NumberValue* o = dynamic_cast<const NumberValue*>(&other)) {
        try {
            long long times = o->value.toLongLong();
            if (times < 0) times = 0;
            std::vector<ValuePtr> new_elements;
            for (long long i = 0; i < times; ++i) {
                for (const auto& elem : this->elements) new_elements.push_back(elem->clone());
            }
            return make_value<LnValue>(new_elements);
        } catch (...) {
            throw std::runtime_error(msg(Msg::LN_REPEAT_INT));
        }
    }
    return Value::multiply(other);
}
bool LnValue::isEqualTo(const Value& other) const {
    const LnValue* o = dynamic_cast<const LnValue*>(&other);
    if (!o || this->elements.size() != o->elements.size()) return false;
    for (size_t i = 0; i < this->elements.size(); ++i) {
        if (!this->elements[i]->isEqualTo(*o->elements[i])) return false;
    }
    return true;
}
ValuePtr LnValue::getSubscript(const Value& index) const {
    const NumberValue* num_val = dynamic_cast<const NumberValue*>(&index);
    if (!num_val) throw std::runtime_error(msg(Msg::LN_IDX_NUM));
    try {
        long long i = num_val->value.toLongLong();
        long long size = elements.size();
        if (i < 0) i += size;
        if (i >= 0 && i < size) return elements[i];
        throw std::runtime_error(msg(Msg::LN_IDX_OOB));
    } catch (...) {
        throw std::runtime_error(msg(Msg::LN_IDX_INV));
    }
}
void LnValue::setSubscript(const Value& index, ValuePtr value) {
    const NumberValue* num_val = dynamic_cast<const NumberValue*>(&index);
    if (!num_val) throw std::runtime_error(msg(Msg::LN_IDX_NUM));
    try {
        long long i = num_val->value.toLongLong();
        long long size = elements.size();
        if (i < 0) i += size;
        if (i >= 0 && i < size) { elements[i] = value; return; }
        throw std::runtime_error(msg(Msg::LN_IDX_OOB));
    } catch (...) {
        throw std::runtime_error(msg(Msg::LN_IDX_INV));
    }
}
ValuePtr LnValue::getSlice(const ValuePtr& start_val, const ValuePtr& end_val, const ValuePtr& step_val) const {
    long long len = this->elements.size();
    long long step = value_to_long(step_val, 1);
    long long start = value_to_long(start_val, (step > 0) ? 0 : len - 1);
    long long end = value_to_long(end_val, (step > 0) ? len : -1);
    SliceParams params = calculate_slice_indices(start, end, step, len);
    std::vector<ValuePtr> result_elements;
    if (params.step > 0) {
        for (long long i = params.start; i < params.stop; i += params.step) result_elements.push_back(this->elements[i]);
    } else {
        for (long long i = params.start; i > params.stop; i += params.step) result_elements.push_back(this->elements[i]);
    }
    return make_value<LnValue>(result_elements);
}
void LnValue::setSlice(const ValuePtr& start_val, const ValuePtr& end_val, const ValuePtr& step_val, ValuePtr value) {
    const LnValue* values_to_assign = dynamic_cast<const LnValue*>(value.get());
    if (!values_to_assign) throw std::runtime_error(msg(Msg::SLICE_LN_ONLY));
    long long len = this->elements.size();
    long long step = value_to_long(step_val, 1);
    long long start = value_to_long(start_val, (step > 0) ? 0 : len - 1);
    long long end = value_to_long(end_val, (step > 0) ? len : -1);

    if (step != 1) {
        std::vector<long long> indices;
        SliceParams params = calculate_slice_indices(start, end, step, len);
        if (params.step > 0) {
            for (long long i = params.start; i < params.stop; i += params.step) indices.push_back(i);
        } else {
            for (long long i = params.start; i > params.stop; i += params.step) indices.push_back(i);
        }
        if (indices.size() != values_to_assign->elements.size()) {
            std::stringstream ss;
            ss << "Attempt to assign sequence of size " << values_to_assign->elements.size()
               << " to extended slice of size " << indices.size();
            throw std::runtime_error(ss.str());
        }
        for (size_t i = 0; i < indices.size(); ++i) this->elements[indices[i]] = values_to_assign->elements[i];
    } else {
        SliceParams params = calculate_slice_indices(start, end, 1, len);
        auto it_start = this->elements.begin() + params.start;
        auto it_end = this->elements.begin() + params.stop;
        this->elements.erase(it_start, it_end);
        this->elements.insert(this->elements.begin() + params.start,
                              values_to_assign->elements.begin(),
                              values_to_assign->elements.end());
    }
}

// FunctionValue
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

// ExceptionValue
std::string ExceptionValue::repr() const {
    std::stringstream ss;
    ss << "<ExceptionObject at " << static_cast<const void*>(this) << " payload=" << payload->repr() << ">";
    return ss.str();
}
bool ExceptionValue::isEqualTo(const Value& other) const {
    if (const ExceptionValue* o = dynamic_cast<const ExceptionValue*>(&other)) {
        return payload->isEqualTo(*(o->payload));
    }
    return false;
}

// DimValue
std::string DimValue::toString() const {
    std::stringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& pair : sorted_entries()) {
        if (!first) ss << ", ";
        first = false;
        // The browser runtime prints the key of a dictionary in quotes,
        // so the same dictionary has to look the same in both.
        ss << '"' << pair.first << "\": " << pair.second->repr();
    }
    ss << "}";
    return ss.str();
}
// ============================================================================
// The members a value answers to
// ============================================================================
//
// "abc".upper(), xs.push(1), d.keys().  They live with the value and not
// with the runtime around it, so the interpreter and the bytecode VM
// answer the same way, and the browser answers the same way too.
//
// The helpers a script reaches for most often live in the standard
// library (~/PyRite/lib) as plain functions: list.map(xs, f) and the
// like.  These are the ones that are about the value itself.

namespace {

bool is_space_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
           c == '\f' || c == '\v';
}

std::string upper_text(const std::string& text) {
    std::string out = text;
    for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

std::string lower_text(const std::string& text) {
    std::string out = text;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string trim_text(const std::string& text) {
    size_t start = 0;
    while (start < text.size() && is_space_char(text[start])) start++;
    size_t stop = text.size();
    while (stop > start && is_space_char(text[stop - 1])) stop--;
    return text.substr(start, stop - start);
}

std::vector<ValuePtr> split_text(const std::string& text, const std::string& sep) {
    std::vector<ValuePtr> parts;
    if (sep.empty()) {
        for (size_t i = 0; i < text.size(); i++)
            parts.push_back(make_value<StringValue>(text.substr(i, 1)));
        return parts;
    }
    size_t pos = 0;
    while (true) {
        size_t hit = text.find(sep, pos);
        if (hit == std::string::npos) {
            parts.push_back(make_value<StringValue>(text.substr(pos)));
            break;
        }
        parts.push_back(make_value<StringValue>(text.substr(pos, hit - pos)));
        pos = hit + sep.size();
    }
    return parts;
}

std::string replace_text(const std::string& text, const std::string& from,
                          const std::string& to) {
    if (from.empty()) return text;
    std::string out;
    size_t pos = 0;
    while (true) {
        size_t hit = text.find(from, pos);
        if (hit == std::string::npos) {
            out.append(text, pos, std::string::npos);
            break;
        }
        out.append(text, pos, hit - pos);
        out.append(to);
        pos = hit + from.size();
    }
    return out;
}

long long number_of(const ValuePtr& value, const char* who) {
    auto number = dynamic_cast<NumberValue*>(value.get());
    if (!number) throw std::runtime_error(std::string("Argument to ") + who +
                                          "() must be a number.");
    return number->value.toLongLong();
}

std::string text_of(const ValuePtr& value, const char* who) {
    auto text = dynamic_cast<StringValue*>(value.get());
    if (!text) throw std::runtime_error(std::string("Argument to ") + who +
                                        "() must be a string.");
    return text->value;
}

} // namespace

// ---- A string ----
ValuePtr StringValue::getMember(const std::string& name) const {
    if (name == "length")
        return make_value<NumberValue>(BigNumber((long long)value.size()));
    if (name == "upper")
        return make_value<NativeFnValue>("upper", [this](const std::vector<ValuePtr>&) {
            return make_value<StringValue>(upper_text(value));
        });
    if (name == "lower")
        return make_value<NativeFnValue>("lower", [this](const std::vector<ValuePtr>&) {
            return make_value<StringValue>(lower_text(value));
        });
    if (name == "trim")
        return make_value<NativeFnValue>("trim", [this](const std::vector<ValuePtr>&) {
            return make_value<StringValue>(trim_text(value));
        });
    if (name == "split")
        return make_value<NativeFnValue>("split", [this](const std::vector<ValuePtr>& args) {
            // A member call passes only the explicit arguments, so the
            // separator arrives at args[0] - the same place it sits for
            // the other string members.
            std::string sep = " ";
            if (!args.empty()) sep = text_of(args[0], "split");
            return make_value<LnValue>(split_text(value, sep));
        });
    if (name == "replace")
        return make_value<NativeFnValue>("replace", [this](const std::vector<ValuePtr>& args) {
            std::string from = text_of(args[0], "replace");
            std::string to = args.size() > 1 ? text_of(args[1], "replace") : "";
            return make_value<StringValue>(replace_text(value, from, to));
        });
    if (name == "startsWith")
        return make_value<NativeFnValue>("startsWith", [this](const std::vector<ValuePtr>& args) {
            std::string prefix = text_of(args[0], "startsWith");
            return make_value<NumberValue>(BigNumber(
                value.compare(0, prefix.size(), prefix) == 0 ? 1 : 0));
        });
    if (name == "endsWith")
        return make_value<NativeFnValue>("endsWith", [this](const std::vector<ValuePtr>& args) {
            std::string suffix = text_of(args[0], "endsWith");
            if (suffix.size() > value.size()) return make_value<NumberValue>(BigNumber(0));
            return make_value<NumberValue>(BigNumber(
                value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0 ? 1 : 0));
        });
    if (name == "indexOf")
        return make_value<NativeFnValue>("indexOf", [this](const std::vector<ValuePtr>& args) {
            std::string needle = text_of(args[0], "indexOf");
            size_t hit = value.find(needle);
            return make_value<NumberValue>(BigNumber(
                hit == std::string::npos ? -1 : (long long)hit));
        });
    return nullptr;
}

BoundMethodValue::BoundMethodValue(InstancePtr inst, std::shared_ptr<Function> m)
    : Value(VTag::BoundMethod), instance(inst), method(m) {}
BoundMethodValue::BoundMethodValue(InstancePtr inst, Ptr<struct Closure> m)
    : Value(VTag::BoundMethod), instance(inst), bc_method(std::move(m)) {}
BoundMethodValue::~BoundMethodValue() = default;
ValuePtr BoundMethodValue::clone() const {
    if (bc_method) return make_value<BoundMethodValue>(instance, bc_method);
    return make_value<BoundMethodValue>(instance, method);
}

// ---- A list ----
ValuePtr LnValue::getMember(const std::string& name) const {
    // The list is shared with the script, so a method that changes it
    // (push, pop) changes the list the script holds.
    Ptr<LnValue> self = ptr_static_cast<LnValue>(Value::self());
    if (name == "length")
        return make_value<NumberValue>(BigNumber((long long)elements.size()));
    if (name == "push" || name == "append")
        return make_value<NativeFnValue>(name, [self](const std::vector<ValuePtr>& args) -> ValuePtr {
            if (args.empty()) return make_value<NullValue>();
            self->elements.push_back(args[0]);
            return make_value<NumberValue>(BigNumber((long long)self->elements.size()));
        });
    if (name == "pop")
        return make_value<NativeFnValue>("pop", [self](const std::vector<ValuePtr>&) -> ValuePtr {
            if (self->elements.empty()) return make_value<NullValue>();
            ValuePtr last = self->elements.back();
            self->elements.pop_back();
            return last;
        });
    if (name == "join")
        return make_value<NativeFnValue>("join", [self](const std::vector<ValuePtr>& args) {
            std::string sep = ",";
            if (!args.empty()) sep = text_of(args[0], "join");
            std::string out;
            for (size_t i = 0; i < self->elements.size(); i++) {
                if (i > 0) out += sep;
                out += self->elements[i]->toString();
            }
            return make_value<StringValue>(out);
        });
    if (name == "slice")
        return make_value<NativeFnValue>("slice", [self](const std::vector<ValuePtr>& args) {
            long long count = (long long)self->elements.size();
            long long start = args.size() > 0 ? number_of(args[0], "slice") : 0;
            long long stop = args.size() > 1 ? number_of(args[1], "slice") : count;
            if (start < 0) start = count + start;
            if (stop < 0) stop = count + stop;
            if (start < 0) start = 0;
            if (stop > count) stop = count;
            if (stop < start) stop = start;
            std::vector<ValuePtr> out;
            for (long long i = start; i < stop; i++)
                out.push_back(self->elements[(size_t)i]);
            return make_value<LnValue>(out);
        });
    if (name == "indexOf")
        return make_value<NativeFnValue>("indexOf", [self](const std::vector<ValuePtr>& args) -> ValuePtr {
            if (args.empty()) return make_value<NumberValue>(BigNumber(-1));
            for (size_t i = 0; i < self->elements.size(); i++) {
                if (self->elements[i]->isEqualTo(*args[0]))
                    return make_value<NumberValue>(BigNumber((long long)i));
            }
            return make_value<NumberValue>(BigNumber(-1));
        });
    return nullptr;
}

// ---- A dictionary ----
ValuePtr DimValue::getMember(const std::string& name) const {
    if (name == "keys")
        return make_value<NativeFnValue>("keys", [this](const std::vector<ValuePtr>&) {
            std::vector<ValuePtr> out;
            for (const auto& pair : sorted_entries()) out.push_back(make_value<StringValue>(pair.first));
            return make_value<LnValue>(out);
        });
    if (name == "values")
        return make_value<NativeFnValue>("values", [this](const std::vector<ValuePtr>&) {
            std::vector<ValuePtr> out;
            for (const auto& pair : sorted_entries()) out.push_back(pair.second);
            return make_value<LnValue>(out);
        });
    if (name == "has")
        return make_value<NativeFnValue>("has", [this](const std::vector<ValuePtr>& args) {
            if (args.empty()) return make_value<NumberValue>(BigNumber(0));
            std::string key = text_of(args[0], "has");
            return make_value<NumberValue>(BigNumber(
                dict.count(key) ? 1 : 0));
        });
    return nullptr;
}

ValuePtr DimValue::clone() const {
    std::unordered_map<std::string, ValuePtr> cloned;
    cloned.reserve(dict.size());
    for (const auto& pair : dict) {
        cloned[pair.first] = pair.second->clone();
    }
    return make_value<DimValue>(cloned);
}
ValuePtr DimValue::getSubscript(const Value& index) const {
    if (const StringValue* s = dynamic_cast<const StringValue*>(&index)) {
        auto it = dict.find(s->value);
        if (it != dict.end()) return it->second;
        throw std::runtime_error(fmt(Msg::DIM_KEY_MISS, s->value));
    }
    throw std::runtime_error(msg(Msg::DIM_KEY_TYPE));
}
void DimValue::setSubscript(const Value& index, ValuePtr value) {
    if (const StringValue* s = dynamic_cast<const StringValue*>(&index)) {
        dict[s->value] = value;
        return;
    }
    throw std::runtime_error(msg(Msg::DIM_KEY_TYPE));
}
bool DimValue::isEqualTo(const Value& other) const {
    const DimValue* o = dynamic_cast<const DimValue*>(&other);
    if (!o || dict.size() != o->dict.size()) return false;
    for (const auto& pair : dict) {
        auto it = o->dict.find(pair.first);
        if (it == o->dict.end()) return false;
        if (!pair.second->isEqualTo(*it->second)) return false;
    }
    return true;
}
