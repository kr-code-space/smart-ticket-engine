# Quick Start Guide
## Smart Ticket Engine - Complete Setup Instructions

This guide will help you set up and run the Smart Ticket Engine in under 5 minutes.

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Running the System](#running-the-system)
- [Testing](#testing)
- [Default Credentials](#default-credentials)
- [Troubleshooting](#troubleshooting)
- [Platform-Specific Notes](#platform-specific-notes)

---

## Prerequisites

### Required Software

**1. GCC Compiler (for C code)**

| Platform | How to Install |
|----------|---------------|
| **Linux** | Usually pre-installed. Verify: `gcc --version` |
| **macOS** | Install Xcode Command Line Tools: `xcode-select --install` |
| **Windows** | Install MinGW or use WSL (Windows Subsystem for Linux) |

**2. Python 3.x**

| Platform | How to Install |
|----------|---------------|
| **Linux** | `sudo apt install python3 python3-pip` (Ubuntu/Debian) |
| **macOS** | Download from [python.org](https://www.python.org/downloads/) |
| **Windows** | Download from [python.org](https://www.python.org/downloads/) |

Verify installation:
```bash
python --version    # or python3 --version
pip --version       # or pip3 --version
```

**3. Flask (Python web framework)**

Install via pip:
```bash
pip install flask
# or
pip install -r requirements.txt
```

---

## Installation

### Step 1: Extract Project

```bash
# If you have the ZIP file
unzip smart-ticket-engine.zip
cd smart-ticket-engine

# If you cloned from Git
cd smart-ticket-engine
```

---

### Step 2: Install Python Dependencies

```bash
# Install Flask
pip install -r requirements.txt

# Or install manually
pip install Flask==3.0.0
```

**Verify installation:**
```bash
python -c "import flask; print(flask.__version__)"
# Should output: 3.0.0 (or similar)
```

---

### Step 3: Compile C Engine

**Linux/Mac:**
```bash
gcc main.c -o engine
```

**Windows (MinGW):**
```bash
gcc main.c -o engine.exe
```

**Verify compilation:**
```bash
# Linux/Mac
ls -lh engine
# Should show the compiled binary

# Windows
dir engine.exe
# Should show the executable file
```

**Common compilation errors:**
- `gcc: command not found` - GCC not installed
- `undefined reference` - Missing libraries (usually not needed for this project)

---

### Step 4: (Optional) Compile Test Suite

**Linux/Mac:**
```bash
gcc test_queue.c -o test_runner
```

**Windows:**
```bash
gcc test_queue.c -o test_runner.exe
```

---

## Running the System

### Two-Process Architecture

The system requires **two processes running simultaneously**:
1. **C Backend** (queue engine) - Handles data structures
2. **Python Frontend** (web server) - Provides user interface

---

### Start the System

**Terminal 1: Start C Backend**
```bash
cd smart-ticket-engine

# Linux/Mac
./engine

# Windows
engine.exe
```

You should see:
```
SECURITY INFORMATION - DSA Ticketing System
==============================================
All security fixes applied:
   - Password hashing enabled (SHA-256)
   - XSS protection active (HTML escaping)
   - Random secret key generated
...
System starting...
Signal handlers registered (Ctrl+C for graceful shutdown)
Loaded 0 tickets from CSV
System ready. Press Ctrl+C for graceful shutdown.
```

**Terminal 2: Start Flask Frontend**
```bash
cd smart-ticket-engine
python server.py
```

You should see:
```
SECURITY INFORMATION - DSA Ticketing System
==============================================
All security fixes applied:
...
Starting Flask server on http://localhost:5000
   Press Ctrl+C to stop
 * Running on http://127.0.0.1:5000
```

---

### Access the Application

Open your web browser and navigate to:

**User Interface** (Submit Tickets):
```
http://localhost:5000
```

**Admin Dashboard** (Manage Tickets):
```
http://localhost:5000/admin
```

**Activity Log** (Audit Trail):
```
http://localhost:5000/activity_log
```

---

## Default Credentials

### Admin Login

| Username | Password | Role |
|----------|----------|------|
| admin | Admin@DSA2025! | Super Admin |
| manager1 | Manager@123 | Manager |
| support1 | Support@123 | Support Agent |
| analyst1 | Analyst@123 | Data Analyst |
| supervisor1 | Supervisor@123 | Supervisor |

**Important:** Change these passwords before any production deployment.

See `CREDENTIALS.md` for security guidelines.

---

## Testing

### Run Automated Tests

**Linux/Mac:**
```bash
chmod +x run_tests.sh
./run_tests.sh
```

**Windows:**
```bash
run_tests.bat
```

### Expected Output

```
Running 12 unit tests...

Test 1: Queue Initialization          PASSED
Test 2: Single Enqueue/Dequeue        PASSED
Test 3: FIFO Order (Multiple Items)   PASSED
Test 4: Circular Wraparound           PASSED
Test 5: Queue Full Condition          PASSED
Test 6: Dequeue from Empty Queue      PASSED
Test 7: Auto Priority Detection       PASSED
Test 8: Email Validation              PASSED
Test 9: Priority Validation           PASSED
Test 10: Ticket ID Validation         PASSED
Test 11: String Validation            PASSED
Test 12: Rapid Enqueue/Dequeue        PASSED

=======================================
All 12 tests passed
Test Coverage: 100% (queue operations)
=======================================
```

---

## Troubleshooting

### Common Issues

#### 1. "Port 5000 already in use"

**Problem:** Another application is using port 5000

**Solution:**
```bash
# Option 1: Kill the process using port 5000
# Linux/Mac
lsof -i :5000
kill -9 <PID>

# Windows
netstat -ano | findstr :5000
taskkill /PID <PID> /F

# Option 2: Change Flask port
# Edit server.py, last line:
socketio.run(app, debug=True, port=5001)  # Change 5000 to 5001
```

---

#### 2. "gcc: command not found"

**Problem:** GCC compiler not installed

**Solution:**
```bash
# Linux (Ubuntu/Debian)
sudo apt update
sudo apt install build-essential

# macOS
xcode-select --install

# Windows
# Install MinGW: https://sourceforge.net/projects/mingw/
# Or use WSL: https://docs.microsoft.com/en-us/windows/wsl/install
```

---

#### 3. "ModuleNotFoundError: No module named 'flask'"

**Problem:** Flask not installed

**Solution:**
```bash
pip install flask
# or
pip3 install flask
# or
python -m pip install flask
```

---

#### 4. "Permission denied" when running ./engine

**Problem:** File doesn't have execute permissions (Linux/Mac only)

**Solution:**
```bash
chmod +x engine
chmod +x run_tests.sh
```

---

#### 5. CSV files not updating

**Problem:** C engine and Flask server not communicating

**Solution:**
1. Make sure **both processes are running** (check both terminals)
2. Check if `customer_support_tickets_updated.csv` exists
3. Try stopping both processes (Ctrl+C) and restarting
4. Check for error messages in terminal output

---

#### 6. Admin login fails

**Problem:** Credentials might be incorrect or `admins.csv` corrupted

**Solution:**
```bash
# Delete admins.csv and restart
rm admins.csv       # Linux/Mac
del admins.csv      # Windows

# Restart Flask server - it will regenerate admins.csv
python server.py
```

Default credentials in `CREDENTIALS.md`

---

### Platform-Specific Notes

#### Linux

**Dependencies:**
```bash
# Ubuntu/Debian
sudo apt install gcc python3 python3-pip

# Fedora/RHEL
sudo dnf install gcc python3 python3-pip
```

**File Permissions:**
```bash
chmod +x engine
chmod +x run_tests.sh
```

---

#### macOS

**Dependencies:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Python 3 (if not already installed)
brew install python3
```

**Note:** macOS might block the executable. Go to System Preferences → Security & Privacy → Allow.

---

#### Windows

**Option 1: MinGW (Native Windows)**

1. Install MinGW: https://sourceforge.net/projects/mingw/
2. Add to PATH: `C:\MinGW\bin`
3. Compile: `gcc main.c -o engine.exe`
4. Run: `engine.exe`

**Option 2: WSL (Recommended)**

1. Install WSL: `wsl --install`
2. Install Ubuntu from Microsoft Store
3. Follow Linux instructions inside WSL

**Path Issues:**
- Use backslashes: `cd C:\Users\YourName\smart-ticket-engine`
- Or forward slashes work in PowerShell: `cd C:/Users/YourName/smart-ticket-engine`

---

## Verification Checklist

After setup, verify everything works:

```
[ ] GCC installed: gcc --version
[ ] Python installed: python --version
[ ] Flask installed: pip show flask
[ ] C engine compiled: ls engine (or dir engine.exe)
[ ] C engine runs: ./engine (should start without errors)
[ ] Flask server runs: python server.py (should show "Running on...")
[ ] Can access http://localhost:5000
[ ] Can login at http://localhost:5000/admin
[ ] Tests pass: ./run_tests.sh (all 12 tests pass)
```

---

## Quick Test Workflow

To verify the system works end-to-end:

1. **Start both processes** (C engine + Flask server)
2. **Submit a ticket** at http://localhost:5000
3. **Login as admin** at http://localhost:5000/admin
4. **View the ticket** in the dashboard
5. **Resolve the ticket** by clicking "Resolve Next Ticket"
6. **Check activity log** at http://localhost:5000/activity_log

If all steps work, setup is successful.

---

## Next Steps

- Read `ENHANCEMENTS.md` for feature documentation
- Check `CHANGELOG.md` for version history
- Explore `CREDENTIALS.md` for security guidelines
- Review `README.md` for architecture overview

---

## Still Having Issues?

1. **Check Terminal Output** - Error messages usually indicate the problem
2. **Verify Prerequisites** - GCC, Python, Flask all installed?
3. **Check File Permissions** - Can you execute the files?
4. **Port Conflicts** - Is something else using port 5000?
5. **Path Issues** - Are you in the right directory?

**Common verification command:**
```bash
pwd  # Should show .../smart-ticket-engine
ls   # Should show main.c, server.py, etc.
```

---

## Quick Reference

```bash
# Compile
gcc main.c -o engine

# Run C Engine
./engine  # or engine.exe on Windows

# Run Flask Server
python server.py

# Run Tests
./run_tests.sh  # or run_tests.bat on Windows

# Stop Everything
Ctrl+C in each terminal
```

---

**Setup Time: Approximately 5 minutes**

If you encounter any issues not covered here, check the error message carefully - it usually indicates exactly what is wrong.