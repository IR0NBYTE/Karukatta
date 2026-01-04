#pragma once

#include <iostream>
#include <string>
#include <sstream>

using namespace std;

/*
    Karukatta Compiler Error Reporting
    Provides rich error messages with source location context
*/

struct SourceLocation {
    size_t line;
    size_t column;
    string filename;

    SourceLocation() : line(1), column(1), filename("") {}

    SourceLocation(size_t l, size_t c, const string& f)
        : line(l), column(c), filename(f) {}
};

class ErrorReporter {
public:
    static void error(const SourceLocation& loc, const string& message) {
        cerr << "\033[1;31merror:\033[0m ";
        if (!loc.filename.empty()) {
            cerr << loc.filename << ":";
        }
        cerr << loc.line << ":" << loc.column << ": " << message << "\n";
        exit(EXIT_FAILURE);
    }

    static void error(const string& message) {
        cerr << "\033[1;31merror:\033[0m " << message << "\n";
        exit(EXIT_FAILURE);
    }

    static void warn(const SourceLocation& loc, const string& message) {
        cerr << "\033[1;33mwarning:\033[0m ";
        if (!loc.filename.empty()) {
            cerr << loc.filename << ":";
        }
        cerr << loc.line << ":" << loc.column << ": " << message << "\n";
    }
};
