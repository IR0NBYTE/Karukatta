#pragma once

#include <iostream>
#include <fstream>
#include <optional>
#include <vector>
#include <sstream>
#include <string>
#include "error.hpp"

using namespace std;

enum class TokenType {
    exit,
    int_lit,
    semi,
    open_paren,
    close_paren,
    ident,
    let,
    eq,
    plus,
    star,
    minus,
    fslash,
    open_curly,
    close_curly,
    if_,
    else_,
    while_,
    // Comparison operators
    eq_eq,        // ==
    neq,          // !=
    less,         // <
    less_eq,      // <=
    greater,      // >
    greater_eq    // >=
};

optional<int> bin_prec(TokenType type)
{
    switch (type) {
    // Comparison operators (lowest precedence)
    case TokenType::eq_eq:
    case TokenType::neq:
    case TokenType::less:
    case TokenType::less_eq:
    case TokenType::greater:
    case TokenType::greater_eq:
        return 0;
    // Addition/Subtraction
    case TokenType::minus:
    case TokenType::plus:
        return 1;
    // Multiplication/Division (highest precedence)
    case TokenType::fslash:
    case TokenType::star:
        return 2;
    default:
        return {};
    }
}

struct Token {
    TokenType type;
    optional<string> value {};
    SourceLocation loc;
};

class Lexer {
public:
    inline explicit Lexer(string src, string filename = "<input>")
        : m_src(std::move(src)), m_filename(std::move(filename))
    {
    }

    inline vector<Token> lexerize()
    {
        vector<Token> tokens;
        string buf;
        while (peek().has_value()) {
            if (isalpha(peek().value())) {
                SourceLocation loc = current_location();
                buf.push_back(consume());
                while (peek().has_value() && isalnum(peek().value())) {
                    buf.push_back(consume());
                }
                if (buf == "exit") {
                    tokens.push_back({ .type = TokenType::exit, .value = {}, .loc = loc });
                    buf.clear();
                }
                else if (buf == "let") {
                    tokens.push_back({ .type = TokenType::let, .value = {}, .loc = loc });
                    buf.clear();
                }
                else if (buf == "if") {
                    tokens.push_back({ .type = TokenType::if_, .value = {}, .loc = loc });
                    buf.clear();
                }
                else if (buf == "else") {
                    tokens.push_back({ .type = TokenType::else_, .value = {}, .loc = loc });
                    buf.clear();
                }
                else if (buf == "while") {
                    tokens.push_back({ .type = TokenType::while_, .value = {}, .loc = loc });
                    buf.clear();
                }
                else {
                    tokens.push_back({ .type = TokenType::ident, .value = buf, .loc = loc });
                    buf.clear();
                }
            }
            else if (isdigit(peek().value())) {
                SourceLocation loc = current_location();
                buf.push_back(consume());
                while (peek().has_value() && isdigit(peek().value())) {
                    buf.push_back(consume());
                }
                tokens.push_back({ .type = TokenType::int_lit, .value = buf, .loc = loc });
                buf.clear();
            }
            else if (peek().value() == '(') {
                SourceLocation loc = current_location();
                consume();
                tokens.push_back({ .type = TokenType::open_paren, .value = {}, .loc = loc });
            }
            else if (peek().value() == ')') {
                SourceLocation loc = current_location();
                consume();
                tokens.push_back({ .type = TokenType::close_paren, .value = {}, .loc = loc });
            }
            else if (peek().value() == ';') {
                SourceLocation loc = current_location();
                consume();
                tokens.push_back({ .type = TokenType::semi, .value = {}, .loc = loc });
            }
            else if (peek().value() == '=') {
                SourceLocation loc = current_location();
                consume();
                if (peek().has_value() && peek().value() == '=') {
                    consume();
                    tokens.push_back({ .type = TokenType::eq_eq, .value = {}, .loc = loc });
                } else {
                    tokens.push_back({ .type = TokenType::eq, .value = {}, .loc = loc });
                }
            }
            else if (peek().value() == '!') {
                SourceLocation loc = current_location();
                consume();
                if (peek().has_value() && peek().value() == '=') {
                    consume();
                    tokens.push_back({ .type = TokenType::neq, .value = {}, .loc = loc });
                } else {
                    ErrorReporter::error(loc, "Unexpected character: '!' (did you mean '!='?)");
                }
            }
            else if (peek().value() == '<') {
                SourceLocation loc = current_location();
                consume();
                if (peek().has_value() && peek().value() == '=') {
                    consume();
                    tokens.push_back({ .type = TokenType::less_eq, .value = {}, .loc = loc });
                } else {
                    tokens.push_back({ .type = TokenType::less, .value = {}, .loc = loc });
                }
            }
            else if (peek().value() == '>') {
                SourceLocation loc = current_location();
                consume();
                if (peek().has_value() && peek().value() == '=') {
                    consume();
                    tokens.push_back({ .type = TokenType::greater_eq, .value = {}, .loc = loc });
                } else {
                    tokens.push_back({ .type = TokenType::greater, .value = {}, .loc = loc });
                }
            }
            else if (peek().value() == '+') {
                SourceLocation loc = current_location();
                consume();
                tokens.push_back({ .type = TokenType::plus, .value = {}, .loc = loc });
            }
            else if (peek().value() == '*') {
                SourceLocation loc = current_location();
                consume();
                tokens.push_back({ .type = TokenType::star, .value = {}, .loc = loc });
            }
            else if (peek().value() == '-') {
                SourceLocation loc = current_location();
                consume();
                tokens.push_back({ .type = TokenType::minus, .value = {}, .loc = loc });
            }
            else if (peek().value() == '/') {
                SourceLocation loc = current_location();
                consume();
                // Check for comment  //
                if (peek().has_value() && peek().value() == '/') {
                    // Skip until end of line
                    while (peek().has_value() && peek().value() != '\n') {
                        consume();
                    }
                } else {
                    tokens.push_back({ .type = TokenType::fslash, .value = {}, .loc = loc });
                }
            }
            else if (peek().value() == '{') {
                SourceLocation loc = current_location();
                consume();
                tokens.push_back({ .type = TokenType::open_curly, .value = {}, .loc = loc });
            }
            else if (peek().value() == '}') {
                SourceLocation loc = current_location();
                consume();
                tokens.push_back({ .type = TokenType::close_curly, .value = {}, .loc = loc });
            }
            else if (isspace(peek().value())) {
                consume();
            }
            else {
                ErrorReporter::error(current_location(),
                    string("Unexpected character: '") + peek().value() + "'");
            }
        }
        m_index = 0;
        return tokens;
    }

private:
    [[nodiscard]] inline optional<char> peek(int offset = 0) const
    {
        if (m_index + offset >= m_src.length()) {
            return {};
        }
        else {
            return m_src.at(m_index + offset);
        }
    }

    inline char consume()
    {
        char c = m_src.at(m_index++);
        if (c == '\n') {
            m_line++;
            m_column = 1;
        } else {
            m_column++;
        }
        return c;
    }

    inline SourceLocation current_location() const
    {
        return SourceLocation(m_line, m_column, m_filename);
    }

    const string m_src;
    const string m_filename;
    size_t m_index = 0;
    size_t m_line = 1;
    size_t m_column = 1;
};