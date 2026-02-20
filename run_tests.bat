@echo off
REM Smart Ticket Engine - Unit Test Runner (Windows)
REM Compiles and runs comprehensive test suite

echo.
echo ╔════════════════════════════════════════════════════════════════════╗
echo ║     SMART TICKET ENGINE - TEST SUITE RUNNER (Windows)              ║
echo ╚════════════════════════════════════════════════════════════════════╝
echo.

REM Step 1: Clean previous builds
echo [1/3] Cleaning previous builds...
if exist test_runner.exe del test_runner.exe
echo ✓ Clean complete
echo.

REM Step 2: Compile test suite
echo [2/3] Compiling test suite...
echo       Compiling: main.c + test_queue.c

gcc -DTESTING main.c test_queue.c -o test_runner.exe
if errorlevel 1 (
    echo ✗ Compilation failed!
    pause
    exit /b 1
)

echo ✓ Compilation successful
echo.

REM Step 3: Run tests
echo [3/3] Running tests...
echo.

test_runner.exe
set TEST_RESULT=%errorlevel%

echo.

REM Cleanup
echo Cleaning up test executable...
del test_runner.exe

if %TEST_RESULT%==0 (
    echo ════════════════════════════════════════════════════════════════════
    echo   ALL TESTS PASSED! ✅
    echo ════════════════════════════════════════════════════════════════════
) else (
    echo ════════════════════════════════════════════════════════════════════
    echo   SOME TESTS FAILED! ❌
    echo ════════════════════════════════════════════════════════════════════
)

echo.
pause
exit /b %TEST_RESULT%
