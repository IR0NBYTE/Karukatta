FROM ubuntu:22.04

# Install build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    nasm \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /karukatta

# Copy source
COPY . .

# Build compiler
RUN g++ -std=c++17 main.cpp -o build/karukatta

# Set entry point
CMD ["/bin/bash"]
