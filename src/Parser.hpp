#pragma once
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include "Tokenizer.hpp"
#include "Ast.hpp"

class Parser {
public:
    Parser(const std::string& source);
    std::vector<AstNodePtr> parse();
    bool has_error() const { return had_error; }
private:
    Tokenizer tokenizer;
    Token current_token, previous_token;
    bool had_error;

    void advance();
    void consume(TokenType type, const std::string& msg);
    bool check(TokenType type);
    bool match(const std::vector<TokenType>& types);
    void consume_end(const std::string& msg);
    void synchronize();

    AstNodePtr statement();
    AstNodePtr declaration();
    ParameterDefinition parse_parameter();
    AstNodePtr var_declaration();
    AstNodePtr fn_definition(const std::string& kind);
    AstNodePtr class_definition();
    AstNodePtr struct_definition();
    AstNodePtr using_statement();
    AstNodePtr require_statement();
    AstNodePtr expose_statement();
    AstNodePtr if_statement();
    AstNodePtr while_statement();
    AstNodePtr loop_statement();
    AstNodePtr for_in_statement();
    AstNodePtr break_statement();
    AstNodePtr await_statement();
    AstNodePtr try_statement();
    AstNodePtr raise_statement();
    AstNodePtr say_statement();
    AstNodePtr return_statement();
    AstNodePtr swap_statement();
    AstNodePtr expression_statement();
    AstNodePtr expression();
    AstNodePtr assignment();
    AstNodePtr logical_or();
    AstNodePtr logical_and();
    AstNodePtr equality();
    AstNodePtr comparison();
    AstNodePtr term();
    AstNodePtr factor();
    AstNodePtr power();
    AstNodePtr typecast();
    AstNodePtr unary();
    AstNodePtr call();
    AstNodePtr fn_lambda(int line);
    AstNodePtr finish_call(AstNodePtr callee);
    AstNodePtr finish_subscript(AstNodePtr object);
    AstNodePtr list_literal();
    AstNodePtr dim_literal();
    AstNodePtr primary();
};
