#pragma once
#include <string>
#include <map>
#include <cctype>

enum class TokenType {
    DEC, STR, BIN, LN, ANY, DIM,
    IF, THEN, ELSE, ELIF, ENDIF, END, WHILE, DO, FINALLY, ENDWHILE, FN, ENDFN, RETURN, SAY, ASK,
    FIN,
    TRY, CATCH, ENDTRY, RAISE,
    AWAIT, ENDAWAIT,
    INS, CONTAINS, ENDINS, STRUCT, IN,
    USING, AS, REQUIRE, EXPOSE,
    LOOP, FOR, TIMES, UNTIL, ENDLOOP, BREAK, CONTINUE,
    IDENTIFIER, NUMBER, STRING, HEX_LITERAL,
    EQUAL, EQUAL_EQUAL, BANG_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL, ARROW,
    PLUS, MINUS, STAR, SLASH, LPAREN, RPAREN, COMMA, CARET, MODULO,
    PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL, CARET_EQUAL, MODULO_EQUAL,
    LBRACKET, RBRACKET,
    LBRACE, RBRACE,
    DOT, COLON,
    NULL_LITERAL,
    NOT, AND, OR,
    END_OF_FILE, UNKNOWN
};

struct Token { TokenType type; std::string lexeme; int line; };

class Tokenizer {
public:
    Tokenizer(const std::string& source);
    Token next_token();
private:
    const std::string& source;
    size_t start, current;
    int line;
    std::map<std::string, TokenType> keywords;

    bool is_at_end();
    char advance();
    char peek();
    char peek_next();
    bool match(char expected);
    void skip_whitespace();
    Token make_token(TokenType type, const std::string& msg = "");
    Token identifier();
    Token number();
    Token hex_literal();
    Token string(char quote);
};
