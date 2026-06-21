#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Value.hpp"
#include "Tokenizer.hpp"

class Interpreter;
using AstNodePtr = std::shared_ptr<class AstNode>;

struct Function {
    std::string name;
    std::vector<class ParameterDefinition> params;
    std::vector<AstNodePtr> body;
    std::shared_ptr<Environment> closure;
    Function(const std::string& n, const std::vector<ParameterDefinition>& p,
             const std::vector<AstNodePtr>& b, const std::shared_ptr<Environment>& c)
        : name(n), params(p), body(b), closure(c) {}
};

struct ParameterDefinition {
    TokenType type_keyword;
    std::string name;
    ValuePtr default_value;      // Cached literal default
    AstNodePtr default_expr;     // Expression default (evaluated at call time)
    bool has_default;
    ParameterDefinition(TokenType tk, const std::string& n, ValuePtr dv = nullptr)
        : type_keyword(tk), name(n), default_value(dv), default_expr(nullptr), has_default(dv != nullptr) {}
    ParameterDefinition(TokenType tk, const std::string& n, AstNodePtr de)
        : type_keyword(tk), name(n), default_value(nullptr), default_expr(de), has_default(true) {}
};

struct AstNode { int line; AstNode(int l) : line(l) {} virtual ~AstNode() = default; virtual ValuePtr accept(Interpreter& visitor) = 0; };
struct LiteralNode : AstNode { ValuePtr value; LiteralNode(int l, ValuePtr v) : AstNode(l), value(v) {} ValuePtr accept(Interpreter& visitor) override; };
struct ListLiteralNode : AstNode { std::vector<AstNodePtr> elements; ListLiteralNode(int l, std::vector<AstNodePtr> e) : AstNode(l), elements(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct DimLiteralNode : AstNode { std::vector<std::pair<AstNodePtr, AstNodePtr>> entries; DimLiteralNode(int l, std::vector<std::pair<AstNodePtr, AstNodePtr>> e) : AstNode(l), entries(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct VariableNode : AstNode { std::string name; VariableNode(int l, std::string n) : AstNode(l), name(n) {} ValuePtr accept(Interpreter& visitor) override; };
struct UnaryOpNode : AstNode { Token op; AstNodePtr right; UnaryOpNode(int l, Token o, AstNodePtr r) : AstNode(l), op(o), right(r) {} ValuePtr accept(Interpreter& visitor) override; };
struct BinaryOpNode : AstNode { AstNodePtr left; Token op; AstNodePtr right; BinaryOpNode(int l, AstNodePtr lt, Token o, AstNodePtr rt) : AstNode(l), left(lt), op(o), right(rt) {} ValuePtr accept(Interpreter& visitor) override; };
struct LogicalOpNode : AstNode { AstNodePtr left; Token op; AstNodePtr right; LogicalOpNode(int l, AstNodePtr lt, Token o, AstNodePtr rt) : AstNode(l), left(lt), op(o), right(rt) {} ValuePtr accept(Interpreter& visitor) override; };
struct TypeConversionNode : AstNode { AstNodePtr expression; Token type_keyword; TypeConversionNode(int l, AstNodePtr e, Token tk) : AstNode(l), expression(e), type_keyword(tk) {} ValuePtr accept(Interpreter& visitor) override; };
struct AssignmentNode : AstNode { AstNodePtr target; AstNodePtr value; AssignmentNode(int l, AstNodePtr t, AstNodePtr v) : AstNode(l), target(t), value(v) {} ValuePtr accept(Interpreter& visitor) override; };
struct VarDeclarationNode : AstNode { Token keyword; std::string name; AstNodePtr initializer; bool is_exposed; VarDeclarationNode(int l, Token kw, std::string n, AstNodePtr init, bool e = false) : AstNode(l), keyword(kw), name(n), initializer(init), is_exposed(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct UsingNode : AstNode { std::string original_name; std::string alias_name; UsingNode(int l, std::string o, std::string a) : AstNode(l), original_name(o), alias_name(a) {} ValuePtr accept(Interpreter& visitor) override; };
struct IfStatementNode : AstNode { AstNodePtr condition; std::vector<AstNodePtr> then_branch, else_branch; IfStatementNode(int l, AstNodePtr c, std::vector<AstNodePtr> t, std::vector<AstNodePtr> e) : AstNode(l), condition(c), then_branch(t), else_branch(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct WhileStatementNode : AstNode { AstNodePtr condition; std::vector<AstNodePtr> do_branch, finally_branch; WhileStatementNode(int l, AstNodePtr c, std::vector<AstNodePtr> d, std::vector<AstNodePtr> f) : AstNode(l), condition(c), do_branch(d), finally_branch(f) {} ValuePtr accept(Interpreter& visitor) override; };
struct LoopForNode : AstNode { std::string index_var_name; std::vector<AstNodePtr> body; AstNodePtr count_expr; LoopForNode(int l, std::string ivn, std::vector<AstNodePtr> b, AstNodePtr c) : AstNode(l), index_var_name(ivn), body(b), count_expr(c) {} ValuePtr accept(Interpreter& visitor) override; };
struct ForInNode : AstNode { std::string var_name; AstNodePtr iterable; std::vector<AstNodePtr> body; ForInNode(int l, const std::string& vn, AstNodePtr it, const std::vector<AstNodePtr>& b) : AstNode(l), var_name(vn), iterable(it), body(b) {} ValuePtr accept(Interpreter& visitor) override; };
struct LoopUntilNode : AstNode { std::string index_var_name; std::vector<AstNodePtr> body; AstNodePtr condition; LoopUntilNode(int l, std::string ivn, std::vector<AstNodePtr> b, AstNodePtr c) : AstNode(l), index_var_name(ivn), body(b), condition(c) {} ValuePtr accept(Interpreter& visitor) override; };
struct BreakNode : AstNode { BreakNode(int l) : AstNode(l) {} ValuePtr accept(Interpreter& visitor) override; };
struct ContinueNode : AstNode { ContinueNode(int l) : AstNode(l) {} ValuePtr accept(Interpreter& visitor) override; };
struct AwaitStatementNode : AstNode { AstNodePtr condition; std::vector<AstNodePtr> then_branch; AwaitStatementNode(int l, AstNodePtr c, std::vector<AstNodePtr> t) : AstNode(l), condition(c), then_branch(t) {} ValuePtr accept(Interpreter& visitor) override; };
struct SayNode : AstNode { AstNodePtr expression; SayNode(int l, AstNodePtr e) : AstNode(l), expression(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct InpNode : AstNode { AstNodePtr expression; InpNode(int l, AstNodePtr e) : AstNode(l), expression(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct FnDefNode : AstNode { std::string name; std::vector<ParameterDefinition> params; std::vector<AstNodePtr> body; bool is_method; bool is_exposed; FnDefNode(int l, std::string n, std::vector<ParameterDefinition> p, std::vector<AstNodePtr> b, bool m = false, bool e = false) : AstNode(l), name(n), params(p), body(b), is_method(m), is_exposed(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct CallNode : AstNode { AstNodePtr callee; std::vector<AstNodePtr> arguments; CallNode(int l, AstNodePtr c, std::vector<AstNodePtr> a) : AstNode(l), callee(c), arguments(a) {} ValuePtr accept(Interpreter& visitor) override; };
struct SubscriptNode : AstNode { AstNodePtr object; AstNodePtr start; AstNodePtr end; AstNodePtr step; bool is_slice; SubscriptNode(int l, AstNodePtr o, AstNodePtr s, AstNodePtr e, AstNodePtr st, bool slice) : AstNode(l), object(o), start(s), end(e), step(st), is_slice(slice) {} ValuePtr accept(Interpreter& visitor) override; };
struct ReturnNode : AstNode { AstNodePtr value; ReturnNode(int l, AstNodePtr v) : AstNode(l), value(v) {} ValuePtr accept(Interpreter& visitor) override; };
struct RaiseNode : AstNode { AstNodePtr expression; RaiseNode(int l, AstNodePtr e) : AstNode(l), expression(e) {} ValuePtr accept(Interpreter& visitor) override; };
struct TryCatchNode : AstNode { std::vector<AstNodePtr> try_branch; std::string exception_var; std::vector<AstNodePtr> catch_branch; std::vector<AstNodePtr> finally_branch; TryCatchNode(int l, std::vector<AstNodePtr> t, std::string ev, std::vector<AstNodePtr> c, std::vector<AstNodePtr> f) : AstNode(l), try_branch(t), exception_var(ev), catch_branch(c), finally_branch(f) {} ValuePtr accept(Interpreter& visitor) override; };
struct ClassDefNode : AstNode {
    std::string name;
    std::vector<ParameterDefinition> fields;
    std::vector<AstNodePtr> initializer_body;
    std::vector<AstNodePtr> methods;
    ClassDefNode(int l, const std::string& n, const std::vector<ParameterDefinition>& f, const std::vector<AstNodePtr>& ib, const std::vector<AstNodePtr>& m)
        : AstNode(l), name(n), fields(f), initializer_body(ib), methods(m) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct GetNode : AstNode {
    AstNodePtr object;
    std::string name;
    GetNode(int l, AstNodePtr obj, const std::string& n) : AstNode(l), object(obj), name(n) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct SetNode : AstNode {
    AstNodePtr object;
    std::string name;
    AstNodePtr value;
    SetNode(int l, AstNodePtr obj, const std::string& n, AstNodePtr val) : AstNode(l), object(obj), name(n), value(val) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct LambdaNode : AstNode {
    std::vector<ParameterDefinition> params;
    std::vector<AstNodePtr> body;
    LambdaNode(int l, const std::vector<ParameterDefinition>& p, const std::vector<AstNodePtr>& b)
        : AstNode(l), params(p), body(b) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct StructDefNode : AstNode {
    std::string name;
    std::vector<ParameterDefinition> fields;
    StructDefNode(int l, const std::string& n, const std::vector<ParameterDefinition>& f)
        : AstNode(l), name(n), fields(f) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct SwapNode : AstNode {
    AstNodePtr left, right;
    SwapNode(int l, AstNodePtr lv, AstNodePtr rv) : AstNode(l), left(lv), right(rv) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct RequireNode : AstNode {
    std::string module_path;
    std::string alias_name;
    RequireNode(int l, std::string p, std::string a) : AstNode(l), module_path(p), alias_name(a) {}
    ValuePtr accept(Interpreter& visitor) override;
};
struct ExpressionStatementNode : AstNode {
    AstNodePtr expression;
    ExpressionStatementNode(int l, AstNodePtr e) : AstNode(l), expression(e) {}
    ValuePtr accept(Interpreter& visitor) override;
};
