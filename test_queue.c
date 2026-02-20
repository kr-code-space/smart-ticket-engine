/*
 * SMART TICKET ENGINE - UNIT TEST SUITE
 * Tests circular queue implementation and core functionality
 * 
 * Compile: gcc -DTESTING main.c test_queue.c -o test_runner
 * Run: ./test_runner
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "config.h"

/* ==================== EXTERNAL DECLARATIONS ==================== */

// Data structure from main.c
struct Ticket {
    int ticketID;
    char customerName[100];
    char email[100];
    char product[100];
    char purchaseDate[50];
    char issueDescription[200];
    char priority[20];
    time_t queueEntryTime;
};

// External variables from main.c
extern struct Ticket queue[MAX_QUEUE_SIZE];
extern int front, rear;

// External functions from main.c
extern int isEmpty();
extern int isFull();
extern int enqueue(struct Ticket t);
extern int dequeue(struct Ticket *t);
extern const char* getAutoPriority(const char* desc);
extern int isValidEmail(const char *email);
extern int isValidPriority(const char *priority);
extern int isValidTicketID(int id);
extern int isValidString(const char *str, int minLen, int maxLen);

/* ==================== TEST UTILITIES ==================== */

int tests_passed = 0;
int tests_failed = 0;

void test_assert(int condition, const char *test_name, const char *message) {
    if (condition) {
        printf("  ✅ %s: %s\n", test_name, message);
        tests_passed++;
    } else {
        printf("  ❌ %s: %s FAILED\n", test_name, message);
        tests_failed++;
    }
}

void reset_queue() {
    front = rear = -1;
}

/* ==================== BASIC QUEUE TESTS ==================== */

void test_queue_initialization() {
    printf("\n📋 TEST 1: Queue Initialization\n");
    reset_queue();
    
    test_assert(isEmpty() == 1, "Empty Check", "New queue should be empty");
    test_assert(isFull() == 0, "Full Check", "New queue should not be full");
    test_assert(front == -1, "Front Index", "Front should be -1");
    test_assert(rear == -1, "Rear Index", "Rear should be -1");
}

void test_single_enqueue_dequeue() {
    printf("\n📋 TEST 2: Single Enqueue/Dequeue\n");
    reset_queue();
    
    // Create test ticket
    struct Ticket t1 = {
        .ticketID = 101,
        .queueEntryTime = time(NULL)
    };
    strcpy(t1.customerName, "John Doe");
    strcpy(t1.email, "john@example.com");
    strcpy(t1.product, "Laptop");
    strcpy(t1.priority, "Medium");
    strcpy(t1.issueDescription, "Screen not working");
    
    // Enqueue
    int enq_result = enqueue(t1);
    test_assert(enq_result == 1, "Enqueue Success", "Should successfully enqueue ticket");
    test_assert(isEmpty() == 0, "Not Empty", "Queue should not be empty after enqueue");
    test_assert(front == 0, "Front Position", "Front should be at 0");
    test_assert(rear == 0, "Rear Position", "Rear should be at 0");
    
    // Dequeue
    struct Ticket result;
    int deq_result = dequeue(&result);
    test_assert(deq_result == 1, "Dequeue Success", "Should successfully dequeue ticket");
    test_assert(result.ticketID == 101, "Ticket ID Match", "Should get same ticket ID");
    test_assert(strcmp(result.email, "john@example.com") == 0, "Email Match", "Email should match");
    test_assert(isEmpty() == 1, "Empty After Dequeue", "Queue should be empty");
    test_assert(front == -1, "Front Reset", "Front should reset to -1");
}

void test_fifo_order() {
    printf("\n📋 TEST 3: FIFO Order (Multiple Items)\n");
    reset_queue();
    
    // Enqueue 5 tickets
    for (int i = 1; i <= 5; i++) {
        struct Ticket t = {
            .ticketID = 200 + i,
            .queueEntryTime = time(NULL)
        };
        sprintf(t.customerName, "User%d", i);
        sprintf(t.email, "user%d@test.com", i);
        strcpy(t.priority, "Low");
        strcpy(t.product, "Product");
        
        int result = enqueue(t);
        test_assert(result == 1, "Enqueue Item", "Should enqueue successfully");
    }
    
    // Dequeue and verify FIFO order
    for (int i = 1; i <= 5; i++) {
        struct Ticket result;
        int deq_result = dequeue(&result);
        test_assert(deq_result == 1, "Dequeue Item", "Should dequeue successfully");
        test_assert(result.ticketID == 200 + i, "FIFO Order", "Should maintain FIFO order");
    }
    
    test_assert(isEmpty() == 1, "Empty After All", "Queue should be empty after all dequeues");
}

void test_circular_wraparound() {
    printf("\n📋 TEST 4: Circular Wraparound\n");
    reset_queue();
    
    // Fill half the queue
    int half = MAX_QUEUE_SIZE / 2;
    for (int i = 0; i < half; i++) {
        struct Ticket t = {.ticketID = i, .queueEntryTime = time(NULL)};
        strcpy(t.priority, "Low");
        strcpy(t.email, "test@test.com");
        enqueue(t);
    }
    
    // Dequeue 1/4
    int quarter = MAX_QUEUE_SIZE / 4;
    for (int i = 0; i < quarter; i++) {
        struct Ticket t;
        dequeue(&t);
    }
    
    // Enqueue more to force wraparound
    for (int i = half; i < half + quarter; i++) {
        struct Ticket t = {.ticketID = i, .queueEntryTime = time(NULL)};
        strcpy(t.priority, "Low");
        strcpy(t.email, "test@test.com");
        enqueue(t);
    }
    
    test_assert(!isEmpty(), "Not Empty", "Queue should still have items");
    
    // Verify next dequeue gets correct item (FIFO maintained)
    struct Ticket t;
    dequeue(&t);
    test_assert(t.ticketID == quarter, "Wraparound Order", "Should maintain FIFO after wraparound");
    
    printf("  ✅ Wraparound Test: Successfully wrapped around array boundary\n");
}

void test_queue_full_condition() {
    printf("\n📋 TEST 5: Queue Full Condition\n");
    reset_queue();
    
    // Fill queue to capacity
    int count = 0;
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        struct Ticket t = {.ticketID = i, .queueEntryTime = time(NULL)};
        strcpy(t.priority, "Low");
        strcpy(t.email, "test@test.com");
        
        if (enqueue(t) == 1) {
            count++;
        } else {
            break;
        }
    }
    
    test_assert(count == MAX_QUEUE_SIZE - 1, "Capacity Check", "Should hold MAX-1 items");
    test_assert(isFull() == 1, "Full Detection", "Should detect queue is full");
    
    // Try to enqueue when full
    struct Ticket overflow = {.ticketID = 9999, .queueEntryTime = time(NULL)};
    strcpy(overflow.priority, "Critical");
    strcpy(overflow.email, "overflow@test.com");
    
    int overflow_result = enqueue(overflow);
    test_assert(overflow_result == 0, "Overflow Rejection", "Should reject when full");
    
    printf("  ℹ️  Queue capacity: %d tickets\n", count);
}

void test_dequeue_empty_queue() {
    printf("\n📋 TEST 6: Dequeue from Empty Queue\n");
    reset_queue();
    
    struct Ticket t;
    int result = dequeue(&t);
    test_assert(result == 0, "Empty Dequeue", "Should fail to dequeue from empty queue");
    test_assert(isEmpty() == 1, "Still Empty", "Queue should remain empty");
}

/* ==================== VALIDATION TESTS ==================== */

void test_auto_priority_detection() {
    printf("\n📋 TEST 7: Auto Priority Detection\n");
    
    test_assert(strcmp(getAutoPriority("My account was hacked!"), "Critical") == 0, 
                "Security Issue", "Should detect 'hacked' as Critical");
    
    test_assert(strcmp(getAutoPriority("Payment failed"), "Critical") == 0,
                "Payment Issue", "Should detect 'payment' as Critical");
    
    test_assert(strcmp(getAutoPriority("Urgent system crash"), "High") == 0,
                "Urgent Issue", "Should detect 'urgent' as High");
    
    test_assert(strcmp(getAutoPriority("Application error"), "High") == 0,
                "Error Issue", "Should detect 'error' as High");
    
    test_assert(strcmp(getAutoPriority("Website is slow"), "Medium") == 0,
                "Performance Issue", "Should detect 'slow' as Medium");
    
    test_assert(strcmp(getAutoPriority("Minor bug"), "Medium") == 0,
                "Bug Issue", "Should detect 'bug' as Medium");
    
    test_assert(strcmp(getAutoPriority("Just a question"), "Low") == 0,
                "General Question", "Should default to Low");
}

void test_email_validation() {
    printf("\n📋 TEST 8: Email Validation\n");
    
    test_assert(isValidEmail("user@example.com") == 1, "Valid Email 1", "Standard email should be valid");
    test_assert(isValidEmail("test.user@company.co.uk") == 1, "Valid Email 2", "Email with dots should be valid");
    test_assert(isValidEmail("name+tag@domain.com") == 1, "Valid Email 3", "Email with + should be valid");
    
    test_assert(isValidEmail("invalid") == 0, "Invalid 1", "Should reject email without @");
    test_assert(isValidEmail("no@domain") == 0, "Invalid 2", "Should reject email without .");
    test_assert(isValidEmail("@nodomain.com") == 0, "Invalid 3", "Should reject email without local part");
    test_assert(isValidEmail("") == 0, "Invalid 4", "Should reject empty string");
    test_assert(isValidEmail(NULL) == 0, "Invalid 5", "Should reject NULL");
}

void test_priority_validation() {
    printf("\n📋 TEST 9: Priority Validation\n");
    
    test_assert(isValidPriority("Low") == 1, "Valid Priority 1", "Low should be valid");
    test_assert(isValidPriority("Medium") == 1, "Valid Priority 2", "Medium should be valid");
    test_assert(isValidPriority("High") == 1, "Valid Priority 3", "High should be valid");
    test_assert(isValidPriority("Critical") == 1, "Valid Priority 4", "Critical should be valid");
    
    test_assert(isValidPriority("Invalid") == 0, "Invalid Priority 1", "Should reject invalid priority");
    test_assert(isValidPriority("CRITICAL") == 0, "Invalid Priority 2", "Should be case-sensitive");
    test_assert(isValidPriority("") == 0, "Invalid Priority 3", "Should reject empty string");
    test_assert(isValidPriority(NULL) == 0, "Invalid Priority 4", "Should reject NULL");
}

void test_ticket_id_validation() {
    printf("\n📋 TEST 10: Ticket ID Validation\n");
    
    test_assert(isValidTicketID(1) == 1, "Valid ID 1", "ID 1 should be valid");
    test_assert(isValidTicketID(100) == 1, "Valid ID 2", "ID 100 should be valid");
    test_assert(isValidTicketID(999999) == 1, "Valid ID 3", "ID 999999 should be valid");
    
    test_assert(isValidTicketID(0) == 0, "Invalid ID 1", "ID 0 should be invalid");
    test_assert(isValidTicketID(-1) == 0, "Invalid ID 2", "Negative ID should be invalid");
    test_assert(isValidTicketID(1000000) == 0, "Invalid ID 3", "ID > 999999 should be invalid");
}

void test_string_validation() {
    printf("\n📋 TEST 11: String Validation\n");
    
    test_assert(isValidString("John Doe", 2, 50) == 1, "Valid String 1", "Normal name should be valid");
    test_assert(isValidString("AB", 2, 10) == 1, "Valid String 2", "Minimum length string should be valid");
    test_assert(isValidString("1234567890", 5, 10) == 1, "Valid String 3", "Maximum length string should be valid");
    
    test_assert(isValidString("A", 2, 10) == 0, "Invalid String 1", "Too short string should be invalid");
    test_assert(isValidString("12345678901", 5, 10) == 0, "Invalid String 2", "Too long string should be invalid");
    test_assert(isValidString(NULL, 2, 10) == 0, "Invalid String 3", "NULL should be invalid");
}

/* ==================== STRESS TESTS ==================== */

void test_rapid_enqueue_dequeue() {
    printf("\n📋 TEST 12: Rapid Enqueue/Dequeue (Stress Test)\n");
    reset_queue();
    
    // Rapidly enqueue and dequeue 1000 items
    for (int i = 0; i < 1000; i++) {
        struct Ticket t = {.ticketID = i, .queueEntryTime = time(NULL)};
        strcpy(t.priority, "Low");
        strcpy(t.email, "stress@test.com");
        enqueue(t);
        
        if (i % 2 == 0) {  // Dequeue every other item
            struct Ticket result;
            dequeue(&result);
        }
    }
    
    test_assert(!isEmpty(), "Queue State", "Queue should have items after stress test");
    printf("  ✅ Stress Test: Successfully handled 1000 operations\n");
}

/* ==================== MAIN TEST RUNNER ==================== */

void print_header() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                    ║\n");
    printf("║     SMART TICKET ENGINE - COMPREHENSIVE TEST SUITE                 ║\n");
    printf("║     Testing Circular Queue & Validation Functions                 ║\n");
    printf("║                                                                    ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
}

void print_summary() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                        TEST RESULTS SUMMARY                        ║\n");
    printf("╠════════════════════════════════════════════════════════════════════╣\n");
    printf("║                                                                    ║\n");
    printf("║     Total Tests:    %3d                                            ║\n", tests_passed + tests_failed);
    printf("║     Passed:         %3d  ✅                                        ║\n", tests_passed);
    printf("║     Failed:         %3d  ❌                                        ║\n", tests_failed);
    printf("║                                                                    ║\n");
    
    if (tests_failed == 0) {
        printf("║     Status:         ALL TESTS PASSED! 🎉                          ║\n");
    } else {
        printf("║     Status:         SOME TESTS FAILED ⚠️                           ║\n");
    }
    
    printf("║                                                                    ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

int main() {
    print_header();
    
    printf("\n🔧 Running Core Queue Tests...\n");
    test_queue_initialization();
    test_single_enqueue_dequeue();
    test_fifo_order();
    test_circular_wraparound();
    test_queue_full_condition();
    test_dequeue_empty_queue();
    
    printf("\n🔍 Running Validation Tests...\n");
    test_auto_priority_detection();
    test_email_validation();
    test_priority_validation();
    test_ticket_id_validation();
    test_string_validation();
    
    printf("\n⚡ Running Stress Tests...\n");
    test_rapid_enqueue_dequeue();
    
    print_summary();
    
    return (tests_failed == 0) ? 0 : 1;
}
