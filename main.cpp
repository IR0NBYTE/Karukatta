#include <iostream>
#include <fstream>
#include <optional>
#include <vector> 
#include <sstream>
#include "./pkg/gen.hpp"

using namespace std;
/*
    Karukatta Compiler 2023.
    
*/

void usage() {
    cout << "Usage: karukatta <file.kar> -o nameOfTheoutputBin\n";
}

int main(int argc, char const *argv[]) {

    if (argc != 4)
        usage();
    else {
        string filePath = argv[1];
        string flag = argv[2];
        string outName = argv[3];
        if (flag != "-o")
            usage();
        
        std::string srcCode;
        {
            std::stringstream contents_stream;
            std::fstream input(argv[1], std::ios::in);
            contents_stream << input.rdbuf();
            srcCode = contents_stream.str();
        }
        Lexer lexi(move(srcCode));
        vector<Token> tks = lexi.lexerize();

        Parser parsi(move(tks));
        optional<NodeProg> prog = parsi.parse_prog();

        if (!prog.has_value()) {
            cerr << "Syntaxe Error !" << "\n";
            exit(EXIT_FAILURE);
        }

        Generator gen(prog.value()); {
            ofstream outputFile(outName + ".asm");
            if (!outputFile.is_open()) {
                cerr << "Failed to create the output file." << std::endl;
                exit(EXIT_FAILURE);
            }
            outputFile << gen.gen_prog();
            outputFile.close();
        }

        string cmd = "nasm -felf64 " + outName + ".asm"; 
        system(cmd.c_str());
        cmd ="ld -o " + outName + " " + outName + ".o";
        system(cmd.c_str());
        cmd = "chmod +x "+ outName;
        system(cmd.c_str());
    }
    

    return 0;
}



