#include "Environment.hpp"
#include "Ast.hpp"
#include "Interpreter.hpp"
#include "msg_cn.hpp"

#ifndef DEBUG
constexpr bool DEBUG = false;
#endif

void Environment::define(const std::string& name, ValuePtr value) {
    if (DEBUG) std::cout << "DEBUG: "
        << "Defining variable '" << name << "' in environment " << this
        << " as " << value->repr() << std::endl;
    values[name] = value;
}

void Environment::assign(const std::string& name, ValuePtr value) {
    auto it = values.find(name);
    if (it != values.end()) {
        if (DEBUG) std::cout << "DEBUG: "
            << "Assigning variable '" << name << "' in environment " << this
            << " to " << value->repr() << std::endl;
        it->second = value;
        return;
    }
    if (enclosing) {
        enclosing->assign(name, value);
        return;
    }
    throw RuntimeError(0, fmt(Msg::UNDEFINED_VAR, name));
}

void Environment::assign_local(const std::string& name, ValuePtr value) {
    auto it = values.find(name);
    if (it != values.end()) {
        it->second = value;
        return;
    }
    throw RuntimeError(0, fmt(Msg::UNDEFINED_VAR, name));
}

ValuePtr Environment::get(const std::string& name) {
    if (DEBUG) std::cout << "DEBUG: Getting '" << name << "' from environment " << this << std::endl;
    auto it = values.find(name);
    if (it != values.end()) {
        if (DEBUG) std::cout << "DEBUG: Found '" << name << "' = " << it->second->repr() << std::endl;
        return it->second;
    }
    if (enclosing) {
        return enclosing->get(name);
    }
    throw RuntimeError(0, fmt(Msg::UNDEFINED_VAR, name));
}

ValuePtr Environment::get(const std::string& name) const {
    auto it = values.find(name);
    if (it != values.end()) return it->second;
    if (enclosing) return enclosing->get(name);
    throw RuntimeError(0, fmt(Msg::UNDEFINED_VAR, name));
}

ValuePtr Environment::get_type(const std::string& name) {
    ValuePtr val = get(name);
    if (dynamic_cast<NumberValue*>(val.get())) return make_value<StringValue>("dec");
    if (dynamic_cast<StringValue*>(val.get())) return make_value<StringValue>("str");
    if (dynamic_cast<BinaryValue*>(val.get())) return make_value<StringValue>("bin");
    if (dynamic_cast<LnValue*>(val.get())) return make_value<StringValue>("ln");
    if (dynamic_cast<DimValue*>(val.get())) return make_value<StringValue>("dim");
    if (dynamic_cast<ExceptionValue*>(val.get())) return make_value<StringValue>("exception");
    if (dynamic_cast<Class*>(val.get())) return make_value<StringValue>("class");
    if (dynamic_cast<Instance*>(val.get())) return make_value<StringValue>("instance");
    return make_value<StringValue>("unknown");
}

// Class
Class::Class(const std::string& n, const std::vector<ParameterDefinition>& f, const std::vector<AstNodePtr>& ib,
             const std::map<std::string, std::shared_ptr<Function>>& m, const std::shared_ptr<Environment>& c)
    : Value(VTag::Class), name(n), fields(f), initializer_body(ib), methods(m), closure(c) {}

bool Class::isEqualTo(const Value& other) const {
    if (const Class* o = dynamic_cast<const Class*>(&other)) return this->name == o->name;
    return false;
}

// Instance
Instance::Instance(ClassPtr k) : Value(VTag::Instance), klass(k) {
    if (DEBUG) std::cout << "DEBUG: Creating " << k->name << " instance with new environment "
                         << k->closure.get() << std::endl;
    instance_env = std::make_shared<Environment>(klass->closure);
    for (const auto& field_def : klass->fields) {
        ValuePtr default_val;
        if (field_def.default_expr) {
            default_val = make_value<NullValue>();
        } else {
            default_val = field_def.default_value ? field_def.default_value->clone() : make_value<NullValue>();
        }
        instance_env->define(field_def.name, default_val);
    }
}

ValuePtr Instance::clone() const {
    auto new_inst = make_value<Instance>(klass);
    for (const auto& field_def : klass->fields) {
        try {
            ValuePtr field_val = instance_env->get(field_def.name);
            new_inst->instance_env->define(field_def.name, field_val->clone());
        } catch (...) {}
    }
    return new_inst;
}

ValuePtr Instance::get(const std::string& name) {
    if (DEBUG) std::cout << "DEBUG: Getting property '" << name << "' from " << klass->name << "'." << std::endl;
    // The instance's own scope first, without the exception round-trip:
    // field reads inside methods run once per loop iteration, and the
    // throwing path costs a C++ exception per miss.
    {
        const auto& own = instance_env->get_values();
        auto own_it = own.find(name);
        if (own_it != own.end()) return own_it->second;
    }
    // Bytecode methods (OP_DEF_CLASS) resolve to their closure; the VM's
    // call paths bind the receiver into the frame's slot 0.
    for (const auto& pm : klass->bc_methods)
        if (pm.first == name) return pm.second;
    auto it = klass->methods.find(name);
    if (it != klass->methods.end()) {
        if (DEBUG) std::cout << "DEBUG: Found method '" << name << "', creating bound method." << std::endl;
        return make_value<BoundMethodValue>(
            ptr_static_cast<Instance>(self()),
            it->second);
    }
    // Fall back to the full scope walk (globals stay visible through the
    // instance environment); it throws when the name truly does not exist.
    return instance_env->get(name);
}

void Instance::set(const std::string& name, ValuePtr value) {
    if (DEBUG) std::cout << "DEBUG: Setting property '" << name << "' for " << klass->name
                         << " to " << value->repr() << std::endl;
    bool field_found = false;
    for (const auto& field_def : klass->fields) {
        if (field_def.name == name) {
            field_found = true;
            if (!is_type_compatible(field_def.type_keyword, value)) {
                std::stringstream ss;
                ss << "字段 '" << name << "' 类型不匹配。期望 " << token_type_to_string(field_def.type_keyword) << ", 得到 ";
                if (dynamic_cast<NumberValue*>(value.get())) ss << "dec";
                else if (dynamic_cast<StringValue*>(value.get())) ss << "str";
                else if (dynamic_cast<BinaryValue*>(value.get())) ss << "bin";
                else if (dynamic_cast<LnValue*>(value.get())) ss << "ln";
                else if (dynamic_cast<DimValue*>(value.get())) ss << "dim";
                else ss << "unknown";
                ss << "。";
                throw std::runtime_error(ss.str());
            }
            break;
        }
    }
    if (!field_found) {
        throw std::runtime_error(fmt(Msg::UNDEF_FIELD, name));
    }
    instance_env->define(name, value);
}

// BoundMethodValue implementations (need full Instance/Class/Function types)
std::string BoundMethodValue::toString() const {
    return "<bound method " + instance->klass->name + "." + method->name + ">";
}
std::string BoundMethodValue::repr() const {
    return toString();
}
