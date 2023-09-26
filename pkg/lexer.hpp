#pragma once

#include <iostream>
#include <fstream>
#include <optional>
#include <vector> 
#include <sstream>
#include <string>

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
    if_
};

optional<int> bin_prec(TokenType type)
{
    switch (type) {
    case TokenType::minus:
    case TokenType::plus:
        return 0;
    case TokenType::fslash:
    case TokenType::star:
        return 1;
    default:
        return {};
    }
}

struct Token {
    TokenType type;
    optional<string> value {};
};

class Lexer {
public:
    inline explicit Lexer(string src)
        : m_src(move(src))
    {
    }

    inline vector<Token> lexerize()
    {
        vector<Token> tokens;
        string buf;
        while (peek().has_value()) {
            if (isalpha(peek().value())) {
                buf.push_back(consume());
                while (peek().has_value() && isalnum(peek().value())) {
                    buf.push_back(consume());
                }
                if (buf == "exit") {
                    tokens.push_back({ .type = TokenType::exit });
                    buf.clear();
                }
                else if (buf == "let") {
                    tokens.push_back({ .type = TokenType::let });
                    buf.clear();
                }
                else if (buf == "if") {
                    tokens.push_back({ .type = TokenType::if_ });
                    buf.clear();
                }
                else {
                    tokens.push_back({ .type = TokenType::ident, .value = buf });
                    buf.clear();
                }
            }
            else if (isdigit(peek().value())) {
                buf.push_back(consume());
                while (peek().has_value() && isdigit(peek().value())) {
                    buf.push_back(consume());
                }
                tokens.push_back({ .type = TokenType::int_lit, .value = buf });
                buf.clear();
            }
            else if (peek().value() == '(') {
                consume();
                tokens.push_back({ .type = TokenType::open_paren });
            }
            else if (peek().value() == ')') {
                consume();
                tokens.push_back({ .type = TokenType::close_paren });
            }
            else if (peek().value() == ';') {
                consume();
                tokens.push_back({ .type = TokenType::semi });
            }
            else if (peek().value() == '=') {
                consume();
                tokens.push_back({ .type = TokenType::eq });
            }
            else if (peek().value() == '+') {
                consume();
                tokens.push_back({ .type = TokenType::plus });
            }
            else if (peek().value() == '*') {
                consume();
                tokens.push_back({ .type = TokenType::star });
            }
            else if (peek().value() == '-') {
                consume();
                tokens.push_back({ .type = TokenType::minus });
            }
            else if (peek().value() == '/') {
                consume();
                tokens.push_back({ .type = TokenType::fslash });
            }
            else if (peek().value() == '{') {
                consume();
                tokens.push_back({ .type = TokenType::open_curly });
            }
            else if (peek().value() == '}') {
                consume();
                tokens.push_back({ .type = TokenType::close_curly });
            }
            else if (isspace(peek().value())) {
                consume();
            }
            else {
                cerr << "You messed up!" << endl;
                exit(EXIT_FAILURE);
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
        return m_src.at(m_index++);
    }

    const string m_src;
    size_t m_index = 0;
};