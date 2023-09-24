#include "./pkg/gen.cpp"
#include <iostream>
#include <fstream>
#include <optional>
#include <vector> 
#include <sstream>
#include <string>

using namespace std;

enum class TokenType {
    _return,
    int_lit,
    semi
};

typedef struct Token {
    TokenType type; 
    optional<string> value; 
} Token;

void fail() {
    cerr << "Syntax error!\n";
    exit(EXIT_FAILURE);
}

vector<Token> Tokenize(const string& line) {
    vector<Token> tokens;
    string buffer;
    if (isdigit(line[0]))
        fail();
    for (size_t i = 0; i < line.size(); ++i) {
        char current = line[i];

        if (isalpha(current)) {
            buffer += current;
            while (isalpha(line[++i])) {
                buffer += line[i];
            }
            i--;
            if (buffer == "return") {
                tokens.push_back({TokenType::_return, ""});
                buffer.clear();
            } else
                fail();
        } else if (isdigit(current)) {
            buffer += current;
            while (isdigit(line[++i])) {
                buffer += line[i];
            }
            i--;
            tokens.push_back({TokenType::int_lit, buffer});
            buffer.clear();
        } else if (current == ';') {
            tokens.push_back({TokenType::semi, ""});
        } else if (!isspace(current)) fail();
    }
    return tokens;
}


string Tokens_To_ASM(const vector<Token>& tokens) {
    stringstream output;
    
    for(int i = 0; i < tokens.size(); i++) {
        if (tokens[i].type == TokenType::_return) {
            output << "    mov rax, 60\n";
            output << "    mov rdi, " << tokens[i + 1].value.value() << "\n";
            output << "    syscall";
        }
    }
    return output.str(); 
}

int main(int argc, char const *argv[]) {

    if (argc != 2)
        cout << "Usage: karukatta <file.kar>\n";
    else {
        string filepath = argv[1];
        ifstream inputFile;
        inputFile.open(filepath);

        if (!inputFile.is_open()) {
            cerr << "Failed to open the file." << "\n";
            exit(EXIT_FAILURE);
        }
        ofstream outputFile("code.asm");
        if (!outputFile.is_open()) {
            cerr << "Failed to open the file." << std::endl;
            exit(EXIT_FAILURE);
        }
        outputFile << "global _start\n\n";
        outputFile << "_start:\n";
        string line;
        while (getline(inputFile, line)) {
            // cout << line << "\n";
            vector<Token> tks = Tokenize(line);
            // for(Token u : tk) {
            //     cout <<  static_cast<int>(u.type) << " ";
            //     cout << u.value.value() << "\n";
            // }
            outputFile << Tokens_To_ASM(tks);
        }   
        outputFile.close();
        inputFile.close();
        system("nasm -felf64 code.asm");
        system("ld -o code code.o");
        system("chmod +x code");
    }
    

    return 0;
}



