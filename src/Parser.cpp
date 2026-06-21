#include "Parser.hpp"
#include "msg_cn.hpp"

#ifndef DEBUG
constexpr bool DEBUG = false;
#endif

Parser::Parser(const std::string& source) : tokenizer(source), had_error(false) {}

std::vector<AstNodePtr> Parser::parse() {
    if (DEBUG) std::cout << "DEBUG: Starting parse..." << std::endl;
    std::vector<AstNodePtr> statements;
    current_token = tokenizer.next_token();
    while (current_token.type != TokenType::END_OF_FILE) {
        try {
            statements.push_back(declaration());
        } catch (const std::runtime_error& e) {
            std::cerr << msg(Msg::PARSE_PREFIX) << current_token.line << ": " << e.what() << "\n";
            had_error = true;
            synchronize();
        }
    }
    return statements;
}

void Parser::advance() { previous_token = current_token; current_token = tokenizer.next_token(); }
void Parser::consume(TokenType type, const std::string& msg) { if (current_token.type == type) { advance(); return; } throw std::runtime_error(msg); }
bool Parser::check(TokenType type) { return current_token.type == type; }
bool Parser::match(const std::vector<TokenType>& types) { for (TokenType type : types) { if (check(type)) { advance(); return true; } } return false; }
// Accept generic 'end' OR a specific block terminator (endif/endfn/etc.)
void Parser::consume_end(const std::string& msg) {
    if (match({TokenType::END})) return;
    throw std::runtime_error(msg);
}

void Parser::synchronize() {
    advance();
    while (current_token.type != TokenType::END_OF_FILE) {
        switch (current_token.type) {
            case TokenType::DEC: case TokenType::STR: case TokenType::BIN: case TokenType::LN: case TokenType::DIM: case TokenType::ANY:
            case TokenType::IF: case TokenType::WHILE: case TokenType::FN: case TokenType::INS: case TokenType::STRUCT:
            case TokenType::SAY: case TokenType::RETURN: case TokenType::TRY: case TokenType::LOOP: case TokenType::AWAIT: case TokenType::USING: case TokenType::RAISE: case TokenType::REQUIRE: case TokenType::EXPOSE: return;
            default: advance();
        }
    }
}

AstNodePtr Parser::declaration() {
    if (DEBUG) std::cout << "DEBUG: Parsing declaration '" << current_token.lexeme << ")..." << std::endl;
    if (match({TokenType::REQUIRE})) return require_statement();
    if (match({TokenType::EXPOSE})) return expose_statement();
    if (match({TokenType::DEC, TokenType::STR, TokenType::BIN, TokenType::LN, TokenType::DIM, TokenType::ANY})) return var_declaration();
    if (match({TokenType::FN})) return fn_definition("function");
    if (match({TokenType::INS})) return class_definition();
    if (match({TokenType::STRUCT})) return struct_definition();
    if (match({TokenType::USING})) return using_statement();
    return statement();
}

AstNodePtr Parser::statement() {
    if (match({TokenType::IF})) return if_statement();
    if (match({TokenType::WHILE})) return while_statement();
    if (match({TokenType::LOOP})) return loop_statement();
    if (match({TokenType::BREAK})) return break_statement();
    if (match({TokenType::CONTINUE})) return std::make_shared<ContinueNode>(previous_token.line);
    if (match({TokenType::FOR})) return for_in_statement();
    if (match({TokenType::AWAIT})) return await_statement();
    if (match({TokenType::SAY})) return say_statement();
    if (match({TokenType::RETURN})) return return_statement();
    if (match({TokenType::TRY})) return try_statement();
    if (match({TokenType::RAISE})) return raise_statement();
    if (check(TokenType::IDENTIFIER) && current_token.lexeme == "swap") {
        return swap_statement();
    }
    return expression_statement();
}

ParameterDefinition Parser::parse_parameter() {
    if (DEBUG) std::cout << "DEBUG: Parsing parameter..." << std::endl;
    Token keyword = current_token;
    if (!match({TokenType::DEC, TokenType::STR, TokenType::BIN, TokenType::LN, TokenType::DIM, TokenType::ANY})) {
        throw std::runtime_error(msg(Msg::PARSE_PARAM_TYPE));
    }
    consume(TokenType::IDENTIFIER, msg(Msg::PARSE_PARAM_NAME));
    std::string param_name = previous_token.lexeme;
    ValuePtr default_value = nullptr;
    AstNodePtr default_expr = nullptr;
    if (match({TokenType::EQUAL})) {
        if (DEBUG) std::cout << "DEBUG: Parsing default value for '" << param_name << "'..." << std::endl;
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
            advance();
            default_value = std::make_shared<NullValue>();
        } else if (check(TokenType::LBRACKET)) {
            advance();
            if (check(TokenType::RBRACKET)) {
                advance();
                default_value = std::make_shared<LnValue>(std::vector<ValuePtr>{});
            } else {
                default_expr = list_literal();
            }
        } else if (check(TokenType::LBRACE)) {
            default_expr = dim_literal();
        } else {
            // Try parsing as expression default (variable reference, fn call, etc.)
            default_expr = expression();
        }
    }
    if (default_expr) return ParameterDefinition(keyword.type, param_name, default_expr);
    return ParameterDefinition(keyword.type, param_name, default_value);
}

AstNodePtr Parser::var_declaration() {
    if (DEBUG) std::cout << "DEBUG: Parsing variable declaration..." << std::endl;
    Token keyword = previous_token;
    consume(TokenType::IDENTIFIER, msg(Msg::PARSE_VAR_NAME));
    std::string name = previous_token.lexeme;
    AstNodePtr initializer = nullptr;
    if (match({TokenType::EQUAL})) { initializer = expression(); }
    return std::make_shared<VarDeclarationNode>(keyword.line, keyword, name, initializer);
}

AstNodePtr Parser::fn_definition(const std::string& kind) {
    if (DEBUG) std::cout << "DEBUG: Parsing " << kind << " definition..." << std::endl;
    int line = previous_token.line;
    consume(TokenType::IDENTIFIER, std::string("Expected ") + kind + " name.");
    std::string name = previous_token.lexeme;
    consume(TokenType::LPAREN, msg(Msg::PARSE_LPAREN_NAME));
    std::vector<ParameterDefinition> params;
    if (!check(TokenType::RPAREN)) {
        do {
            if (params.size() >= 255) throw std::runtime_error(msg(Msg::PARSE_TOO_MANY_PARAMS));
            params.push_back(parse_parameter());
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RPAREN, msg(Msg::PARSE_RPAREN_PARAMS));
    // Shorthand: fn name(params) => expr
    if (match({TokenType::ARROW})) {
        AstNodePtr expr = expression();
        std::vector<AstNodePtr> body = {std::make_shared<ReturnNode>(line, expr)};
        return std::make_shared<FnDefNode>(line, name, params, body, kind == "method");
    }
    consume(TokenType::DO, msg(Msg::PARSE_DO_BODY));
    std::vector<AstNodePtr> body;
    while (!check(TokenType::ENDFN) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { body.push_back(declaration()); }
    if (!match({TokenType::ENDFN, TokenType::END})) throw std::runtime_error("Expect 'endfn' or 'end' after function body.");
    return std::make_shared<FnDefNode>(line, name, params, body, kind == "method");
}

AstNodePtr Parser::fn_lambda(int line) {
    if (!check(TokenType::LPAREN)) {
        throw std::runtime_error("Expect '(' after 'fn' for anonymous function.");
    }
    advance();
    std::vector<ParameterDefinition> params;
    if (!check(TokenType::RPAREN)) {
        do {
            if (params.size() >= 255) throw std::runtime_error(msg(Msg::PARSE_TOO_MANY_PARAMS));
            params.push_back(parse_parameter());
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RPAREN, msg(Msg::PARSE_RPAREN_PARAMS));
    // fn(params) => expr  (lambda / arrow function)
    if (match({TokenType::ARROW})) {
        AstNodePtr expr = expression();
        std::vector<AstNodePtr> body = {std::make_shared<ReturnNode>(line, expr)};
        return std::make_shared<LambdaNode>(line, params, body);
    }
    // fn(params) do ... endfn  (anonymous function with body)
    consume(TokenType::DO, msg(Msg::PARSE_DO_BODY));
    std::vector<AstNodePtr> body;
    while (!check(TokenType::ENDFN) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { body.push_back(declaration()); }
    if (!match({TokenType::ENDFN, TokenType::END})) throw std::runtime_error("Expect 'endfn' or 'end' after function body.");
    return std::make_shared<LambdaNode>(line, params, body);
}

AstNodePtr Parser::class_definition() {
    if (DEBUG) std::cout << "DEBUG: Parsing class definition..." << std::endl;
    int line = previous_token.line;
    consume(TokenType::IDENTIFIER, msg(Msg::PARSE_CLASS_NAME));
    std::string name = previous_token.lexeme;
    std::vector<ParameterDefinition> fields;
    if (match({TokenType::LPAREN})) {
        if (!check(TokenType::RPAREN)) {
            do {
                if (fields.size() >= 255) throw std::runtime_error(msg(Msg::PARSE_TOO_MANY_FIELDS));
                fields.push_back(parse_parameter());
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RPAREN, msg(Msg::PARSE_RPAREN_FIELDS));
    }
    std::vector<AstNodePtr> initializer_body;
    while (!check(TokenType::CONTAINS) && !check(TokenType::ENDINS) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) {
        initializer_body.push_back(declaration());
    }
    consume(TokenType::CONTAINS, msg(Msg::PARSE_CONTAINS));
    std::vector<AstNodePtr> methods;
    while (!check(TokenType::ENDINS) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) {
        if (match({TokenType::FN})) { methods.push_back(fn_definition("method")); }
        else { throw std::runtime_error(msg(Msg::PARSE_ONLY_METHODS)); }
    }
    if (!match({TokenType::ENDINS, TokenType::END})) throw std::runtime_error(msg(Msg::PARSE_ENDINS));
    return std::make_shared<ClassDefNode>(line, name, fields, initializer_body, methods);
}

AstNodePtr Parser::struct_definition() {
    int line = previous_token.line;
    consume(TokenType::IDENTIFIER, msg(Msg::PARSE_CLASS_NAME));
    std::string name = previous_token.lexeme;
    std::vector<ParameterDefinition> fields;
    if (match({TokenType::LPAREN})) {
        if (!check(TokenType::RPAREN)) {
            do {
                if (fields.size() >= 255) throw std::runtime_error(msg(Msg::PARSE_TOO_MANY_FIELDS));
                fields.push_back(parse_parameter());
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RPAREN, "Expect ')' after struct fields.");
    }
    return std::make_shared<StructDefNode>(line, name, fields);
}

AstNodePtr Parser::using_statement() {
    int line = previous_token.line;
    consume(TokenType::IDENTIFIER, "Expect variable name after 'using'.");
    std::string original = previous_token.lexeme;
    consume(TokenType::AS, "Expect 'as' after variable name in 'using' statement.");
    consume(TokenType::IDENTIFIER, "Expect alias name after 'as'.");
    std::string alias = previous_token.lexeme;
    return std::make_shared<UsingNode>(line, original, alias);
}

AstNodePtr Parser::require_statement() {
    int line = previous_token.line;
    consume(TokenType::STRING, "Expect module path string after 'require'.");
    std::string module_path = previous_token.lexeme;
    std::string alias_name;
    if (match({TokenType::AS})) {
        consume(TokenType::IDENTIFIER, "Expect alias name after 'as' in require statement.");
        alias_name = previous_token.lexeme;
    }
    return std::make_shared<RequireNode>(line, module_path, alias_name);
}

AstNodePtr Parser::expose_statement() {
    int line = previous_token.line;
    if (match({TokenType::FN})) {
        auto node = fn_definition("function");
        if (auto fn_node = std::dynamic_pointer_cast<FnDefNode>(node)) {
            fn_node->is_exposed = true;
        }
        return node;
    }
    if (match({TokenType::DEC, TokenType::STR, TokenType::BIN, TokenType::LN, TokenType::DIM, TokenType::ANY})) {
        auto node = var_declaration();
        if (auto var_node = std::dynamic_pointer_cast<VarDeclarationNode>(node)) {
            var_node->is_exposed = true;
        }
        return node;
    }
    if (match({TokenType::INS})) {
        auto node = class_definition();
        return node;
    }
    if (match({TokenType::STRUCT})) {
        auto node = struct_definition();
        return node;
    }
    throw std::runtime_error("Expect 'fn', 'dec', 'str', 'bin', 'ln', 'dim', 'any', 'ins', or 'struct' after 'expose'.");
}

AstNodePtr Parser::if_statement() {
    int line = previous_token.line;
    AstNodePtr condition = expression();
    consume(TokenType::THEN, msg(Msg::PARSE_THEN_IF));
    std::vector<AstNodePtr> then_branch;
    auto not_endif = [&]() { return !check(TokenType::ELSE) && !check(TokenType::ELIF) && !check(TokenType::ENDIF) && !check(TokenType::END) && !check(TokenType::END_OF_FILE); };
    while (not_endif()) { then_branch.push_back(declaration()); }
    std::vector<AstNodePtr> else_branch;
    // Parse elif chain into nested if statements inside else_branch
    std::vector<AstNodePtr>* elif_target = &else_branch;
    while (match({TokenType::ELIF})) {
        AstNodePtr elif_cond = expression();
        consume(TokenType::THEN, msg(Msg::PARSE_THEN_IF));
        std::vector<AstNodePtr> elif_then;
        while (not_endif()) { elif_then.push_back(declaration()); }
        auto elif_node = std::make_shared<IfStatementNode>(line, elif_cond, elif_then, std::vector<AstNodePtr>{});
        elif_target->push_back(elif_node);
        // Next elif goes into the else branch of the previous elif_node
        elif_target = &((static_cast<IfStatementNode*>(elif_node.get()))->else_branch);
    }
    if (match({TokenType::ELSE})) {
        // If we had elif chain, else belongs to the last elif's else branch
        if (elif_target != &else_branch) {
            while (!check(TokenType::ENDIF) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { elif_target->push_back(declaration()); }
        } else {
            while (!check(TokenType::ENDIF) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { else_branch.push_back(declaration()); }
        }
    }
    if (!match({TokenType::ENDIF, TokenType::END})) throw std::runtime_error(msg(Msg::PARSE_ENDIF));
    return std::make_shared<IfStatementNode>(line, condition, then_branch, else_branch);
}

AstNodePtr Parser::while_statement() {
    int line = previous_token.line;
    AstNodePtr condition = expression();
    consume(TokenType::DO, msg(Msg::PARSE_DO_WHILE));
    std::vector<AstNodePtr> do_branch;
    while (!check(TokenType::FINALLY) && !check(TokenType::FIN) && !check(TokenType::ENDWHILE) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { do_branch.push_back(declaration()); }
    std::vector<AstNodePtr> finally_branch;
    if (match({TokenType::FINALLY, TokenType::FIN})) { while (!check(TokenType::ENDWHILE) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { finally_branch.push_back(declaration()); } }
    if (!match({TokenType::ENDWHILE, TokenType::END})) throw std::runtime_error(msg(Msg::PARSE_ENDWHILE));
    return std::make_shared<WhileStatementNode>(line, condition, do_branch, finally_branch);
}

AstNodePtr Parser::loop_statement() {
    int line = previous_token.line;
    std::string index_var_name;
    if (match({TokenType::LPAREN})) {
        consume(TokenType::IDENTIFIER, "Expect loop index variable name in parentheses.");
        index_var_name = previous_token.lexeme;
        consume(TokenType::RPAREN, "Expect ')' after loop index variable name.");
    }
    std::vector<AstNodePtr> body;
    while (!check(TokenType::FOR) && !check(TokenType::UNTIL) && !check(TokenType::ENDLOOP) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) {
        body.push_back(declaration());
    }
    if (match({TokenType::FOR})) {
        AstNodePtr count_expr = expression();
        consume(TokenType::TIMES, "Expect 'times' after 'for' loop count.");
        return std::make_shared<LoopForNode>(line, index_var_name, body, count_expr);
    } else if (match({TokenType::UNTIL})) {
        AstNodePtr condition = expression();
        return std::make_shared<LoopUntilNode>(line, index_var_name, body, condition);
    } else if (match({TokenType::ENDLOOP, TokenType::END})) {
        return std::make_shared<LoopUntilNode>(line, index_var_name, body, nullptr);
    } else {
        throw std::runtime_error("Unterminated 'loop' block. Expect 'for', 'until', or 'endloop'.");
    }
}

AstNodePtr Parser::break_statement() { return std::make_shared<BreakNode>(previous_token.line); }

AstNodePtr Parser::await_statement() {
    int line = previous_token.line;
    AstNodePtr condition = expression();
    consume(TokenType::THEN, msg(Msg::PARSE_THEN_AWAIT));
    std::vector<AstNodePtr> then_branch;
    while (!check(TokenType::ENDAWAIT) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { then_branch.push_back(declaration()); }
    if (!match({TokenType::ENDAWAIT, TokenType::END})) throw std::runtime_error(msg(Msg::PARSE_ENDAWAIT));
    return std::make_shared<AwaitStatementNode>(line, condition, then_branch);
}

AstNodePtr Parser::try_statement() {
    int line = previous_token.line;
    std::vector<AstNodePtr> try_branch;
    while (!check(TokenType::CATCH) && !check(TokenType::END_OF_FILE)) { try_branch.push_back(declaration()); }
    consume(TokenType::CATCH, msg(Msg::PARSE_CATCH_TRY));
    consume(TokenType::IDENTIFIER, msg(Msg::PARSE_VAR_CATCH));
    std::string exception_var = previous_token.lexeme;
    std::vector<AstNodePtr> catch_branch;
    while (!check(TokenType::FINALLY) && !check(TokenType::FIN) && !check(TokenType::ENDTRY) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { catch_branch.push_back(declaration()); }
    std::vector<AstNodePtr> finally_branch;
    if (match({TokenType::FINALLY, TokenType::FIN})) { while (!check(TokenType::ENDTRY) && !check(TokenType::END) && !check(TokenType::END_OF_FILE)) { finally_branch.push_back(declaration()); } }
    if (!match({TokenType::ENDTRY, TokenType::END})) throw std::runtime_error(msg(Msg::PARSE_ENDTRY));
    return std::make_shared<TryCatchNode>(line, try_branch, exception_var, catch_branch, finally_branch);
}

AstNodePtr Parser::for_in_statement() {
    int line = previous_token.line;
    consume(TokenType::IDENTIFIER, "Expect variable name after 'for'.");
    std::string var_name = previous_token.lexeme;
    consume(TokenType::IN, "Expect 'in' after variable name in 'for'.");
    AstNodePtr iterable = expression();
    consume(TokenType::DO, "Expect 'do' after iterable in 'for'.");
    std::vector<AstNodePtr> body;
    while (!check(TokenType::END) && !check(TokenType::END_OF_FILE)) { body.push_back(declaration()); }
    if (!match({TokenType::END})) throw std::runtime_error("Expect 'end' after for-in body.");
    return std::make_shared<ForInNode>(line, var_name, iterable, body);
}

AstNodePtr Parser::raise_statement() { int line = previous_token.line; return std::make_shared<RaiseNode>(line, expression()); }
AstNodePtr Parser::say_statement() {
    int line = previous_token.line;
    consume(TokenType::LPAREN, msg(Msg::PARSE_LPAREN_SAY));
    AstNodePtr value = expression();
    consume(TokenType::RPAREN, msg(Msg::PARSE_RPAREN_EXPR));
    return std::make_shared<SayNode>(line, value);
}

AstNodePtr Parser::return_statement() {
    int line = previous_token.line;
    ValuePtr v = std::make_shared<NullValue>();
    AstNodePtr val_node = std::make_shared<LiteralNode>(line, v);
    if (!check(TokenType::ENDFN) && !check(TokenType::ENDIF) && !check(TokenType::ENDWHILE) && !check(TokenType::ENDTRY) && !check(TokenType::END) && !check(TokenType::ELIF) && !check(TokenType::ELSE)) {
        val_node = expression();
    }
    return std::make_shared<ReturnNode>(line, val_node);
}

AstNodePtr Parser::swap_statement() {
    int line = current_token.line;
    advance();
    consume(TokenType::LPAREN, "Expect '(' after 'swap'.");
    AstNodePtr left_arg = expression();
    consume(TokenType::COMMA, "Expect ',' between swap arguments.");
    AstNodePtr right_arg = expression();
    consume(TokenType::RPAREN, "Expect ')' after swap arguments.");
    if (!dynamic_cast<VariableNode*>(left_arg.get()) && !dynamic_cast<SubscriptNode*>(left_arg.get())) {
        throw std::runtime_error("First argument to swap must be an assignable variable or list element.");
    }
    if (!dynamic_cast<VariableNode*>(right_arg.get()) && !dynamic_cast<SubscriptNode*>(right_arg.get())) {
        throw std::runtime_error("Second argument to swap must be an assignable variable or list element.");
    }
    return std::make_shared<SwapNode>(line, left_arg, right_arg);
}

AstNodePtr Parser::expression_statement() {
    int line = current_token.line;
    return std::make_shared<ExpressionStatementNode>(line, expression());
}

AstNodePtr Parser::expression() { return assignment(); }

AstNodePtr Parser::assignment() {
    AstNodePtr expr = logical_or();
    // Compound assignment: += -= *= /= ^= %=
    TokenType compound_ops[] = {TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                                 TokenType::SLASH_EQUAL, TokenType::CARET_EQUAL, TokenType::MODULO_EQUAL};
    TokenType binary_ops[] = {TokenType::PLUS, TokenType::MINUS, TokenType::STAR,
                              TokenType::SLASH, TokenType::CARET, TokenType::MODULO};
    for (int i = 0; i < 6; ++i) {
        if (match({compound_ops[i]})) {
            int line = previous_token.line;
            if (!dynamic_cast<VariableNode*>(expr.get()) && !dynamic_cast<SubscriptNode*>(expr.get()) && !dynamic_cast<GetNode*>(expr.get())) {
                throw std::runtime_error(msg(Msg::INVALID_ASSIGN));
            }
            AstNodePtr right = assignment();
            Token op_token = {binary_ops[i], "", line};
            auto binary = std::make_shared<BinaryOpNode>(line, expr, op_token, right);
            return std::make_shared<AssignmentNode>(line, expr, binary);
        }
    }
    if (match({TokenType::EQUAL})) {
        int line = previous_token.line;
        AstNodePtr value = assignment();
        if (dynamic_cast<VariableNode*>(expr.get()) || dynamic_cast<SubscriptNode*>(expr.get()) || dynamic_cast<GetNode*>(expr.get())) {
            return std::make_shared<AssignmentNode>(line, expr, value);
        }
        throw std::runtime_error(msg(Msg::INVALID_ASSIGN));
    }
    return expr;
}

AstNodePtr Parser::logical_or() {
    AstNodePtr expr = logical_and();
    while (match({TokenType::OR})) {
        Token op = previous_token;
        expr = std::make_shared<LogicalOpNode>(op.line, expr, op, logical_and());
    }
    return expr;
}

AstNodePtr Parser::logical_and() {
    AstNodePtr expr = equality();
    while (match({TokenType::AND})) {
        Token op = previous_token;
        expr = std::make_shared<LogicalOpNode>(op.line, expr, op, equality());
    }
    return expr;
}

AstNodePtr Parser::equality() {
    AstNodePtr expr = comparison();
    while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) {
        Token op = previous_token;
        expr = std::make_shared<BinaryOpNode>(op.line, expr, op, comparison());
    }
    return expr;
}

AstNodePtr Parser::comparison() {
    AstNodePtr expr = term();
    while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL})) {
        Token op = previous_token;
        expr = std::make_shared<BinaryOpNode>(op.line, expr, op, term());
    }
    return expr;
}

AstNodePtr Parser::term() {
    AstNodePtr expr = factor();
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        Token op = previous_token;
        expr = std::make_shared<BinaryOpNode>(op.line, expr, op, factor());
    }
    return expr;
}

AstNodePtr Parser::factor() {
    AstNodePtr expr = power();
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::MODULO})) {
        Token op = previous_token;
        expr = std::make_shared<BinaryOpNode>(op.line, expr, op, power());
    }
    return expr;
}

AstNodePtr Parser::power() {
    AstNodePtr expr = typecast();
    while (match({TokenType::CARET})) {
        Token op = previous_token;
        expr = std::make_shared<BinaryOpNode>(op.line, expr, op, typecast());
    }
    return expr;
}

AstNodePtr Parser::typecast() {
    AstNodePtr expr = unary();
    if (match({TokenType::AS})) {
        int line = previous_token.line;
            if (match({TokenType::DEC, TokenType::STR, TokenType::BIN, TokenType::LN, TokenType::DIM})) {
            return std::make_shared<TypeConversionNode>(line, expr, previous_token);
        } else {
            throw std::runtime_error("Expect 'dec', 'str', 'bin', 'ln', or 'dim' after 'as' for type conversion.");
        }
    }
    return expr;
}

AstNodePtr Parser::unary() {
    if (match({TokenType::NOT, TokenType::MINUS})) {
        Token op = previous_token;
        return std::make_shared<UnaryOpNode>(op.line, op, unary());
    }
    return call();
}

AstNodePtr Parser::call() {
    AstNodePtr expr = primary();
    while (true) {
        if (match({TokenType::LPAREN})) { expr = finish_call(expr); }
        else if (match({TokenType::LBRACKET})) { expr = finish_subscript(expr); }
        else if (match({TokenType::DOT})) {
            consume(TokenType::IDENTIFIER, msg(Msg::PARSE_PROP_NAME));
            std::string name = previous_token.lexeme;
            expr = std::make_shared<GetNode>(previous_token.line, expr, name);
        } else { break; }
    }
    return expr;
}

AstNodePtr Parser::finish_call(AstNodePtr callee) {
    int line = previous_token.line;
    std::vector<AstNodePtr> arguments;
    if (!check(TokenType::RPAREN)) {
        do {
            if (arguments.size() >= 255) throw std::runtime_error(msg(Msg::PARSE_TOO_MANY_ARGS));
            arguments.push_back(expression());
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RPAREN, msg(Msg::PARSE_RPAREN_PARAMS));
    return std::make_shared<CallNode>(line, callee, arguments);
}

AstNodePtr Parser::finish_subscript(AstNodePtr object) {
    int line = previous_token.line;
    AstNodePtr part1 = nullptr, part2 = nullptr, part3 = nullptr;
    if (!check(TokenType::COLON) && !check(TokenType::RBRACKET)) { part1 = expression(); }
    if (match({TokenType::COLON})) {
        if (!check(TokenType::COLON) && !check(TokenType::RBRACKET)) { part2 = expression(); }
        if (match({TokenType::COLON})) { if (!check(TokenType::RBRACKET)) { part3 = expression(); } }
        consume(TokenType::RBRACKET, msg(Msg::PARSE_RBRACKET_IDX));
        return std::make_shared<SubscriptNode>(line, object, part1, part2, part3, true);
    } else {
        consume(TokenType::RBRACKET, msg(Msg::PARSE_RBRACKET_IDX));
        return std::make_shared<SubscriptNode>(line, object, part1, nullptr, nullptr, false);
    }
}

AstNodePtr Parser::list_literal() {
    int line = previous_token.line;
    std::vector<AstNodePtr> elements;
    if (!check(TokenType::RBRACKET)) {
        do { elements.push_back(expression()); } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RBRACKET, msg(Msg::PARSE_RBRACKET_LN));
    return std::make_shared<ListLiteralNode>(line, elements);
}

AstNodePtr Parser::dim_literal() {
    int line = previous_token.line;
    std::vector<std::pair<AstNodePtr, AstNodePtr>> entries;
    if (!check(TokenType::RBRACE)) {
        do {
            AstNodePtr key = expression();
            consume(TokenType::COLON, "Expect ':' after key in dim literal.");
            AstNodePtr value = expression();
            entries.push_back({key, value});
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RBRACE, "Expect '}' after dim literal.");
    return std::make_shared<DimLiteralNode>(line, entries);
}

AstNodePtr Parser::primary() {
    int line = current_token.line;
    if (match({TokenType::FN})) return fn_lambda(line);
    if (match({TokenType::NUMBER})) return std::make_shared<LiteralNode>(line, std::make_shared<NumberValue>(BigNumber(previous_token.lexeme)));
    if (match({TokenType::STRING})) return std::make_shared<LiteralNode>(line, std::make_shared<StringValue>(previous_token.lexeme));
    if (match({TokenType::HEX_LITERAL})) return std::make_shared<LiteralNode>(line, std::make_shared<BinaryValue>(previous_token.lexeme));
    if (match({TokenType::NULL_LITERAL})) return std::make_shared<LiteralNode>(line, std::make_shared<NullValue>());
    if (match({TokenType::LBRACKET})) return list_literal();
    if (match({TokenType::LBRACE})) return dim_literal();
    if (match({TokenType::IDENTIFIER})) return std::make_shared<VariableNode>(line, previous_token.lexeme);
    if (match({TokenType::ASK})) {
        consume(TokenType::LPAREN, msg(Msg::PARSE_LPAREN_ASK));
        AstNodePtr prompt = expression();
        consume(TokenType::RPAREN, msg(Msg::PARSE_RPAREN_PROMPT));
        auto ask_node = std::make_shared<InpNode>(line, prompt);
        if (match({TokenType::AS})) {
            consume(TokenType::IDENTIFIER, "Expect variable name for assignment after 'as'.");
            std::string var_name = previous_token.lexeme;
            auto var_node = std::make_shared<VariableNode>(previous_token.line, var_name);
            return std::make_shared<AssignmentNode>(line, var_node, ask_node);
        }
        return ask_node;
    }
    if (match({TokenType::LPAREN})) {
        AstNodePtr expr = expression();
        consume(TokenType::RPAREN, msg(Msg::PARSE_RPAREN_EXPR));
        return expr;
    }
    throw std::runtime_error(msg(Msg::PARSE_EXPR));
}
