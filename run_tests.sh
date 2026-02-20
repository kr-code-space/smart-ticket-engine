#!/bin/bash

# Smart Ticket Engine - Unit Test Runner
# Compiles and runs comprehensive test suite

echo ""
echo "╔════════════════════════════════════════════════════════════════════╗"
echo "║     SMART TICKET ENGINE - TEST SUITE RUNNER                        ║"
echo "╚════════════════════════════════════════════════════════════════════╝"
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Step 1: Clean previous builds
echo -e "${YELLOW}[1/3]${NC} Cleaning previous builds..."
rm -f test_runner test_runner.exe
echo -e "${GREEN}✓${NC} Clean complete"
echo ""

# Step 2: Compile test suite
echo -e "${YELLOW}[2/3]${NC} Compiling test suite..."
echo "      Compiling: main.c + test_queue.c"

# Try GCC first
if command -v gcc &> /dev/null; then
    gcc -DTESTING main.c test_queue.c -o test_runner -lm
    COMPILE_RESULT=$?
elif command -v cc &> /dev/null; then
    cc -DTESTING main.c test_queue.c -o test_runner -lm
    COMPILE_RESULT=$?
else
    echo -e "${RED}✗${NC} Error: No C compiler found (gcc or cc required)"
    exit 1
fi

if [ $COMPILE_RESULT -ne 0 ]; then
    echo -e "${RED}✗${NC} Compilation failed!"
    exit 1
fi

echo -e "${GREEN}✓${NC} Compilation successful"
echo ""

# Step 3: Run tests
echo -e "${YELLOW}[3/3]${NC} Running tests..."
echo ""

./test_runner
TEST_RESULT=$?

echo ""

# Cleanup
echo "Cleaning up test executable..."
rm -f test_runner test_runner.exe

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}════════════════════════════════════════════════════════════════════"
    echo -e "  ALL TESTS PASSED! ✅"
    echo -e "════════════════════════════════════════════════════════════════════${NC}"
    exit 0
else
    echo -e "${RED}════════════════════════════════════════════════════════════════════"
    echo -e "  SOME TESTS FAILED! ❌"
    echo -e "════════════════════════════════════════════════════════════════════${NC}"
    exit 1
fi
