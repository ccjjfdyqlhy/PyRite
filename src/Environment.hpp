#pragma once
#include <string>
#include <map>
#include <memory>
#include <stdexcept>
#include <sstream>
#include "Value.hpp"
#include "Ast.hpp"

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment(std::shared_ptr<Environment> enc = nullptr) : enclosing(enc) {}
    void define(const std::string& name, ValuePtr value);
    void assign(const std::string& name, ValuePtr value);
    void assign_local(const std::string& name, ValuePtr value);
    ValuePtr get(const std::string& name);
    ValuePtr get(const std::string& name) const;
    ValuePtr get_type(const std::string& name);
    const std::map<std::string, ValuePtr>& get_values() const { return values; }
    // Direct pointer to the stored value of `name`, walking the scope
    // chain. The map is node-based and nothing erases from it, so the
    // pointer stays valid across later defines and assignments - which
    // is what lets the VM cache it per call site.
    const ValuePtr* cell(const std::string& name) {
        auto it = values.find(name);
        if (it != values.end()) return &it->second;
        if (enclosing) return enclosing->cell(name);
        return nullptr;
    }
    // Pointer for writing: creates the entry when missing.
    ValuePtr* cell_mut(const std::string& name) {
        return &values[name];
    }
private:
    std::shared_ptr<Environment> enclosing;
    std::map<std::string, ValuePtr> values;
};

struct Class : public Value {
    std::string name;
    std::vector<ParameterDefinition> fields;
    std::vector<AstNodePtr> initializer_body;
    std::map<std::string, std::shared_ptr<Function>> methods;
    std::shared_ptr<Environment> closure;
    Class(const std::string& n, const std::vector<ParameterDefinition>& f, const std::vector<AstNodePtr>& ib,
          const std::map<std::string, std::shared_ptr<Function>>& m, const std::shared_ptr<Environment>& c);
    std::string toString() const override { return "<class " + name + ">"; }
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override { return make_value<Class>(*this); }
    bool isEqualTo(const Value& other) const override;
};

struct Instance : public Value {
public:
    ClassPtr klass;
    std::shared_ptr<Environment> instance_env;
    Instance(ClassPtr k);
    std::string toString() const override { return "<" + klass->name + " instance>"; }
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override;
    ValuePtr get(const std::string& name);
    void set(const std::string& name, ValuePtr value);
};
