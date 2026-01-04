#!/usr/bin/env bash

# Complete testing in Docker (Linux x86-64 environment)

set -e

echo "Building Docker image..."
docker build -t karukatta-test . -q

echo ""
echo "Running compilation tests..."
docker run --rm karukatta-test bash -c "
    cd /karukatta

    echo 'Building compiler...'
    g++ -std=c++17 main.cpp -o build/karukatta

    echo 'Testing example programs...'
    cd example

    # Test each example and run it
    echo ''
    echo 'Test 1: Basic Arithmetic'
    ../build/karukatta 01_basic_arithmetic.kar -o test1
    ./test1
    echo \"Exit code: \$?\" # Should be 13

    echo ''
    echo 'Test 2: Operator Precedence'
    ../build/karukatta 02_operator_precedence.kar -o test2
    ./test2
    echo \"Exit code: \$?\" # Should be 20

    echo ''
    echo 'Test 3: Conditionals'
    ../build/karukatta 03_conditionals.kar -o test3
    ./test3
    echo \"Exit code: \$?\" # Should be 42

    echo ''
    echo 'Test 4: Scoping'
    ../build/karukatta 04_scoping.kar -o test4
    ./test4
    echo \"Exit code: \$?\" # Should be 60

    echo ''
    echo 'Test 5: Shadowing'
    ../build/karukatta 05_shadowing.kar -o test5
    ./test5
    echo \"Exit code: \$?\" # Should be 15

    echo ''
    echo 'Test 6: Complex Expression'
    ../build/karukatta 06_complex_expression.kar -o test6
    ./test6
    echo \"Exit code: \$?\" # Should be 41

    echo ''
    echo 'Test 7: Comparisons'
    ../build/karukatta 07_comparisons.kar -o test7
    ./test7
    echo \"Exit code: \$?\" # Should be 2

    echo ''
    echo 'Test 8: Else Clause'
    ../build/karukatta 08_else_clause.kar -o test8
    ./test8
    echo \"Exit code: \$?\" # Should be 42

    echo ''
    echo 'Test 9: While Loop'
    ../build/karukatta 09_while_loop.kar -o test9
    ./test9
    echo \"Exit code: \$?\" # Should be 42

    echo ''
    echo 'Test 10: Original Example'
    ../build/karukatta example.kar -o test10
    ./test10
    echo \"Exit code: \$?\" # Should be 5

    echo ''
    echo 'All tests completed!'
"

echo ""
echo "Docker tests complete!"
