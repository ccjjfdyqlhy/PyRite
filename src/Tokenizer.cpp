#include "Tokenizer.hpp"
#include "msg_cn.hpp"

Tokenizer::Tokenizer(const std::string& source) : source(source), start(0), current(0), line(1) {
    keywords["any"] = TokenType::ANY;
    keywords["dim"] = TokenType::DIM;
    keywords["nul"] = TokenType::NULL_LITERAL;
    keywords["dec"] = TokenType::DEC; keywords["str"] = TokenType::STR; keywords["bin"] = TokenType::BIN; keywords["ln"] = TokenType::LN;
    keywords["if"] = TokenType::IF; keywords["then"] = TokenType::THEN; keywords["else"] = TokenType::ELSE; keywords["elif"] = TokenType::ELIF; keywords["endif"] = TokenType::ENDIF; keywords["end"] = TokenType::END;
    keywords["while"] = TokenType::WHILE; keywords["do"] = TokenType::DO; keywords["finally"] = TokenType::FINALLY; keywords["fin"] = TokenType::FIN; keywords["endwhile"] = TokenType::ENDWHILE; 
    keywords["fn"] = TokenType::FN; keywords["endfn"] = TokenType::ENDFN; keywords["return"] = TokenType::RETURN; keywords["say"] = TokenType::SAY; 
    keywords["ask"] = TokenType::ASK;
    keywords["await"] = TokenType::AWAIT; keywords["endawait"] = TokenType::ENDAWAIT;
    keywords["try"] = TokenType::TRY; keywords["catch"] = TokenType::CATCH; keywords["endtry"] = TokenType::ENDTRY; keywords["raise"] = TokenType::RAISE;
    keywords["ins"] = TokenType::INS; keywords["contains"] = TokenType::CONTAINS; keywords["endins"] = TokenType::ENDINS;
    keywords["struct"] = TokenType::STRUCT; keywords["in"] = TokenType::IN;
    keywords["as"] = TokenType::AS; keywords["using"] = TokenType::USING;
    keywords["require"] = TokenType::REQUIRE; keywords["expose"] = TokenType::EXPOSE;
    keywords["loop"] = TokenType::LOOP; keywords["for"] = TokenType::FOR; keywords["times"] = TokenType::TIMES;
    keywords["until"] = TokenType::UNTIL; keywords["endloop"] = TokenType::ENDLOOP; keywords["break"] = TokenType::BREAK; keywords["continue"] = TokenType::CONTINUE;
    keywords["not"] = TokenType::NOT; keywords["and"] = TokenType::AND; keywords["or"] = TokenType::OR;
}

Token Tokenizer::next_token() {
    skip_whitespace(); start = current; if (is_at_end()) return make_token(TokenType::END_OF_FILE);
    char c = advance(); if (isalpha(c) || c == '_') return identifier();
    if (isdigit(c)) {
        if (c == '0' && (peek() == 'x' || peek() == 'X')) return hex_literal();
        return number();
    }
    switch (c) {
        case '(': return make_token(TokenType::LPAREN); case ')': return make_token(TokenType::RPAREN); case ',': return make_token(TokenType::COMMA);
        case '+': return make_token(match('=') ? TokenType::PLUS_EQUAL : TokenType::PLUS);
        case '*': return make_token(match('=') ? TokenType::STAR_EQUAL : TokenType::STAR);
        case '/': if (peek() == '/') { while (peek() != '\n' && !is_at_end()) advance(); return next_token(); } return make_token(match('=') ? TokenType::SLASH_EQUAL : TokenType::SLASH);
        case '^': return make_token(match('=') ? TokenType::CARET_EQUAL : TokenType::CARET);
        case '%': return make_token(match('=') ? TokenType::MODULO_EQUAL : TokenType::MODULO);
        case '=': return make_token(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
        case '-': return make_token(match('>') ? TokenType::ARROW : TokenType::MINUS);
        case '!': return make_token(match('=') ? TokenType::BANG_EQUAL : TokenType::UNKNOWN);
        case '<': return make_token(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS); case '>': return make_token(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
        case '"': case '\'': return string(c);
        case '[': return make_token(TokenType::LBRACKET); case ']': return make_token(TokenType::RBRACKET);
        case '{': return make_token(TokenType::LBRACE); case '}': return make_token(TokenType::RBRACE);
        case '.': return make_token(TokenType::DOT); case ':': return make_token(TokenType::COLON);
    }
    return make_token(TokenType::UNKNOWN, msg(Msg::PARSE_UNEXPECTED));
}

bool Tokenizer::is_at_end() { return current >= source.length(); }
char Tokenizer::advance() { return source[current++]; }
char Tokenizer::peek() { return is_at_end() ? '\0' : source[current]; }
char Tokenizer::peek_next() { return current + 1 >= source.length() ? '\0' : source[current + 1]; }
bool Tokenizer::match(char expected) { if (is_at_end() || source[current] != expected) return false; current++; return true; }

void Tokenizer::skip_whitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ': case '\r': case '\t': advance(); break;
            case '\n': line++; advance(); break;
            case '#': { advance(); while (peek() != '#' && !is_at_end()) { if (peek() == '\n') line++; advance(); } if (!is_at_end()) advance(); break; }
            case '/': if (peek_next() == '/') { while (peek() != '\n' && !is_at_end()) advance(); break; } return;
            default: return;
        }
    }
}

Token Tokenizer::make_token(TokenType type, const std::string& msg) {
    return Token{type, msg.empty() ? source.substr(start, current - start) : msg, line};
}

Token Tokenizer::identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    std::string text = source.substr(start, current - start);
    auto it = keywords.find(text);
    return make_token(it != keywords.end() ? it->second : TokenType::IDENTIFIER);
}

Token Tokenizer::number() {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peek_next())) { advance(); while (isdigit(peek())) advance(); }
    return make_token(TokenType::NUMBER);
}

Token Tokenizer::hex_literal() {
    advance(); while (isxdigit(peek())) advance();
    return make_token(TokenType::HEX_LITERAL);
}

Token Tokenizer::string(char quote) {
    std::string result;
    while (peek() != quote && !is_at_end()) {
        if (peek() == '\n') line++;
        if (peek() == '\\') {
            advance();
            switch (peek()) {
                case 'n': result += '\n'; advance(); break;
                case 't': result += '\t'; advance(); break;
                case 'r': result += '\r'; advance(); break;
                case '\\': result += '\\'; advance(); break;
                case '\'': result += '\''; advance(); break;
                case '"': result += '"'; advance(); break;
                case '0': result += '\0'; advance(); break;
                default: result += '\\'; break;
            }
        } else {
            result += advance();
        }
    }
    if (is_at_end()) return make_token(TokenType::UNKNOWN, msg(Msg::PARSE_UNTERM_STR));
    advance();
    return Token{TokenType::STRING, result, line};
}
