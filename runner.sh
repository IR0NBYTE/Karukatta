#!/usr/bin/env bash

RED='\033[0;31m'  # Red text
BOLD='\033[1m'    # Bold text
RESET='\033[0m'    # Reset text formatting

GREEN='\033[0;32m'  # Green text
BOLD2='\033[1m'      # Bold text
RESET2='\033[0m'      # Reset text formatting

# Function to print a fancy log message
print_log() {
    echo -e "${GREEN}${BOLD2}Log:${RESET2} $1"
}

print_error() {
    echo -e "${RED}${BOLD}Error:${RESET} $1"
}

mkdir -p "build"

# Detect OS and compile accordingly
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS - compile without -static
    g++ -std=c++17 main.cpp -o ./build/karukatta
else
    # Linux - compile with -static
    g++ -std=c++17 main.cpp -o ./build/karukatta -s -static
fi 

if [ $? -eq 0 ]; then
    print_log "Compiling Terminated Successfully!"
    print_log "Running!"
    chmod +x ./build/karukatta
    ./build/karukatta $1 $2 $3
else
    print_error "Compiling Karukatta!"
fi
