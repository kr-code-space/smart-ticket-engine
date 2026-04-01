# Smart Ticket Engine
### Customer Support System with Circular Queue Implementation

> A full-stack ticketing system built on a **custom circular queue in C**, with a Python Flask web interface. Developed as a semester project to apply data structures in a real, working application.

**Stack:** C · Python Flask · HTML/CSS/JS | **Tests:** 12 automated unit tests | **Team:** 4 students

---

## Overview

Customers submit support tickets through a web form. The C backend processes them in FIFO order using a circular queue, while a Flask web interface allows users and admins to interact with the system.

The project was built to apply data structures in a practical application and understand how backend processing integrates with a web interface.

---

## Features

**Core System**
- **Circular Queue** — O(1) enqueue/dequeue with wraparound logic, supports up to 10,000 tickets in the queue
- **FIFO Guarantee** — strict ordering ensures no ticket is skipped or starved
- **Auto-Escalation** — tickets automatically climb Low → Medium → High → Critical every 24 hours; any ticket older than 72h is force-escalated to Critical regardless of starting priority
- **Duplicate Detection** — prevents spam by blocking same-email + similar-issue resubmissions
- **Multi-Admin System** — 5 role-based admin accounts with timestamped activity audit logs
- **Customer History** — retrieves a customer's past tickets on new submission for context

**Engineering Quality**
- **12 Unit Tests** — cover queue init, FIFO ordering, circular wraparound, overflow/underflow, and all input validators
- **Input Validation** — email format, ticket ID range (1–999,999), string length and content checks applied everywhere
- **Defensive Programming** — NULL pointer checks and errno-based error reporting throughout the C codebase
- **Graceful Shutdown** — SIGINT/SIGTERM handlers save queue state to CSV and regenerate the admin dashboard before exit
- **Security** — SHA-256 password hashing, session-based auth, XSS prevention via HTML escaping

---

## Technical Highlights

### Circular Queue — O(1) Operations

```c
// O(1) Enqueue with circular wraparound
int enqueue(struct Ticket t) {
    if (isFull()) return 0;
    if (isEmpty()) front = 0;
    rear = (rear + 1) % MAX_QUEUE_SIZE;
    queue[rear] = t;
    return 1;
}

// O(1) Dequeue maintaining FIFO order
int dequeue(struct Ticket *t) {
    if (isEmpty()) return 0;
    *t = queue[front];
    if (front == rear) { front = rear = -1; }
    else { front = (front + 1) % MAX_QUEUE_SIZE; }
    return 1;
}
```
We chose a circular queue over a linear array to eliminate O(n) element shifting and enable efficient memory reuse without fragmentation.

---

### Auto-Escalation Algorithm — Prevents Starvation

```c
void escalateOldTickets() {
    double hours = difftime(now, queue[i].queueEntryTime) / 3600.0;

    if (hours >= 72)                              strcpy(priority, "Critical"); // safety net
    else if (hours >= 24 && Low)                  strcpy(priority, "Medium");
    else if (hours >= 48 && Medium)               strcpy(priority, "High");
    else if (hours >= 24 && High)                 strcpy(priority, "Critical");
}
```
The system periodically checks waiting tickets and escalates priority based on waiting time — ensuring that no ticket remains unaddressed regardless of its initial priority.

---

## Project Structure
```
smart-ticket-engine/
├── main.c            # circular queue processing engine (C)
├── config.h          # configuration definitions
├── server.py         # Flask web server
├── test_queue.c      # unit tests
├── data_generator.c  # synthetic data generator
├── templates/        # HTML templates (user + admin interface)
├── static/           # CSS, JavaScript and assets
├── run_tests.sh      # Linux/Mac test runner
├── run_tests.bat     # Windows test runner
└── .gitignore
```

---

## Getting Started

```bash
# 1. Compile and start the C backend
gcc -o main main.c -lm
./main

# 2. In a new terminal, start the Flask server
pip install -r requirements.txt
python server.py
```

| Interface | URL |
|---|---|
| Customer Portal | http://localhost:5000 |
| Admin Dashboard | http://localhost:5000/admin |
| Activity Log | http://localhost:5000/activity_log |

> Admin credentials are in `CREDENTIALS.md`

```bash
# Run all unit tests
./run_tests.sh      # Linux/Mac
run_tests.bat       # Windows

# Expected output: All 12 tests passed ✓
```

---

## Known Limitations

This is an academic project — we made deliberate scope trade-offs:

- **Storage:** CSV-based persistence; no database, no concurrent write safety, no transaction support
- **Scalability:** Single-threaded, in-memory queue (resets on restart), no horizontal scaling
- **Auth:** Basic session management; SHA-256 hashing used (bcrypt/Argon2 would be the production standard); no rate limiting or HTTPS enforcement

---

## Future Enhancements

- **Database migration** — replace CSV with PostgreSQL or SQLite for persistence and query support
- **Real-time updates** — swap 5-second polling for WebSockets
- **Containerization** — Dockerize and add a CI/CD pipeline
- **Auth upgrade** — JWT tokens + bcrypt password hashing
- **Notifications** — email alerts and SLA breach tracking

---

## Team

- Kaushiki Roy
- Sourav Mondal
- Santanu Das
- Riya Maitra 

## Project Coordination

Repository maintained and coordinated by Kaushiki Roy. Development was carried out collaboratively by the team.
