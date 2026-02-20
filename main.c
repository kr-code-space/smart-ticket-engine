#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#ifdef _WIN32
    #include <windows.h>  // Windows
#else
    #include <unistd.h>   // Linux
#endif
#include <strings.h>
#include "config.h"

#define MAX MAX_QUEUE_SIZE

/* ==================== GRACEFUL SHUTDOWN SUPPORT ==================== */

// Global flag for clean shutdown on SIGINT (Ctrl+C) or SIGTERM
volatile sig_atomic_t running = 1;

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\n\n");
        printf("  Shutdown signal received - cleaning up...  \n");
        running = 0;
    }
}

void setupSignalHandlers() {
    signal(SIGINT, signal_handler);   // Ctrl+C
    #ifndef _WIN32
    signal(SIGTERM, signal_handler);  // Terminate command (Unix)
    #endif
    printf(" Signal handlers registered (Ctrl+C for graceful shutdown)\n");
}

/* ==================== DATA STRUCTURES ==================== */

/* 
 * DESIGN DECISION: Using Circular Queue for FIFO fairness
 * Why not Priority Queue? To prevent starvation of low-priority tickets.
 * Auto-escalation handles urgency while maintaining queue order.
 */

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

/* ==================== CIRCULAR QUEUE OPERATIONS ==================== */

struct Ticket queue[MAX];
int front = -1;
int rear = -1;

int isEmpty() {
    return front == -1;
}

int isFull() {
    return (rear + 1) % MAX == front;
}

int enqueue(struct Ticket t) {
    if (isFull()) {
        // Log overflow for monitoring
        FILE *overflow = fopen("overflow_log.txt", "a");
        if (overflow) {
            char timeBuf[50];
            time_t now = time(NULL);
            strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            fprintf(overflow, "[%s] QUEUE FULL - Ticket #%d rejected\n", timeBuf, t.ticketID);
            fclose(overflow);
        }
        return 0;
    }
    if (front == -1) front = 0;
    rear = (rear + 1) % MAX;
    queue[rear] = t;
    return 1;
}

int dequeue(struct Ticket *t) {
    if (isEmpty()) return 0;

    *t = queue[front];

    if (front == rear)
        front = rear = -1;
    else
        front = (front + 1) % MAX;

    return 1;
}

/* ==================== UTILITY FUNCTIONS ==================== */

void removeNewline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
        str[len - 1] = '\0';
}

void getSystemTime(char *buffer) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", tm_info);
}

void logError(const char *message) {
    FILE *err = fopen("error_log.txt", "a");
    if (err) {
        char timeBuf[50];
        getSystemTime(timeBuf);
        fprintf(err, "[%s] ERROR: %s\n", timeBuf, message);
        fclose(err);
    }
}

/* 
 * Auto-priority detection based on keywords in issue description.
 * NOTE: These keywords are NOT shown to users to prevent gaming the system.
 */
const char* getAutoPriority(const char* desc) {
    char d[300];
    strncpy(d, desc, 299);
    d[299] = '\0';
    for (int i = 0; d[i]; i++) d[i] = tolower(d[i]);

    // Critical: Security, financial, data loss
    if (strstr(d, "hack") || strstr(d, "security") || strstr(d, "money") || 
        strstr(d, "payment") || strstr(d, "fraud") || strstr(d, "stolen"))
        return "Critical";
    
    // High: System failures, urgent issues
    if (strstr(d, "urgent") || strstr(d, "fail") || strstr(d, "error") || 
        strstr(d, "crash") || strstr(d, "broke") || strstr(d, "not working"))
        return "High";
    
    // Medium: Performance issues, bugs
    if (strstr(d, "bug") || strstr(d, "slow") || strstr(d, "delay") || 
        strstr(d, "glitch") || strstr(d, "issue"))
        return "Medium";
    
    return "Low";
}

/* ==================== INPUT VALIDATION FUNCTIONS ==================== */

/*
 * PRODUCTION-GRADE VALIDATION:
 * Validates all input data to prevent crashes from malformed CSV or corrupt data.
 * Returns 1 if valid, 0 if invalid.
 */

int isValidTicketID(int id) {
    return (id >= MIN_TICKET_ID && id <= MAX_TICKET_ID);
}

int isValidEmail(const char *email) {
    if (!email || strlen(email) < MIN_EMAIL_LEN || strlen(email) > MAX_EMAIL_LEN) {
        return 0;
    }
    
    // Email must contain @ and . in correct order
    const char *at = strchr(email, '@');
    const char *dot = strrchr(email, '.');
    
    if (!at || !dot || dot <= at) {
        return 0;
    }
    
    // There should be characters after the last dot (domain extension)
    if (strlen(dot) < 2) {
        return 0;
    }
    
    return 1;
}

int isValidString(const char *str, int minLen, int maxLen) {
    if (!str) return 0;
    
    int len = strlen(str);
    if (len < minLen || len > maxLen) {
        return 0;
    }
    
    // Check for at least one non-whitespace character
    int hasContent = 0;
    for (int i = 0; str[i]; i++) {
        if (!isspace((unsigned char)str[i])) {
            hasContent = 1;
            break;
        }
    }
    
    return hasContent;
}

int isValidPriority(const char *priority) {
    if (!priority) return 0;
    
    return (strcmp(priority, "Low") == 0 ||
            strcmp(priority, "Medium") == 0 ||
            strcmp(priority, "High") == 0 ||
            strcmp(priority, "Critical") == 0);
}

/* ==================== DUPLICATE DETECTION ==================== */

/*
 * SMART DUPLICATE DETECTION:
 * - Prevents spam from impatient users resubmitting same issue
 * - Allows legitimate recurring issues (check resolved tickets)
 * - Compares: same email + similar issue text (first 30 chars)
 */

int isDuplicateInQueue(const char *email, const char *issue) {
    if (isEmpty()) return 0;
    
    char issuePrefix[31];
    strncpy(issuePrefix, issue, 30);
    issuePrefix[30] = '\0';
    
    // Convert to lowercase for comparison
    for (int i = 0; issuePrefix[i]; i++) {
        issuePrefix[i] = tolower(issuePrefix[i]);
    }
    
    int i = front;
    while (1) {
        // Check if same email
        if (strcasecmp(queue[i].email, email) == 0) {
            // Check if similar issue (first 30 chars match)
            char queueIssuePrefix[31];
            strncpy(queueIssuePrefix, queue[i].issueDescription, 30);
            queueIssuePrefix[30] = '\0';
            
            for (int j = 0; queueIssuePrefix[j]; j++) {
                queueIssuePrefix[j] = tolower(queueIssuePrefix[j]);
            }
            
            if (strcmp(issuePrefix, queueIssuePrefix) == 0) {
                return queue[i].ticketID; // Found duplicate - return existing ticket ID
            }
        }
        
        if (i == rear) break;
        i = (i + 1) % MAX;
    }
    
    return 0; // Not a duplicate
}

int isDuplicateInResolved(const char *email, const char *issue, int maxDaysBack) {
    FILE *f = fopen("resolved_tickets.csv", "r");
    if (!f) return 0;
    
    char issuePrefix[31];
    strncpy(issuePrefix, issue, 30);
    issuePrefix[30] = '\0';
    for (int i = 0; issuePrefix[i]; i++) {
        issuePrefix[i] = tolower(issuePrefix[i]);
    }
    
    char line[1024];
    fgets(line, sizeof(line), f); // Skip header
    
    time_t now = time(NULL);
    time_t cutoffTime = now - (maxDaysBack * 24 * 3600);
    
    while (fgets(line, sizeof(line), f)) {
        char lineCopy[1024];
        strncpy(lineCopy, line, 1023);
        lineCopy[1023] = '\0';
        
        // Parse: Ticket ID, Name, Email, Product, Date, Issue, Priority, Entry Time, Resolved Time
        char *tok = strtok(lineCopy, ",");
        if (!tok) continue;
        
        tok = strtok(NULL, ","); // name
        if (!tok) continue;
        
        char *csvEmail = strtok(NULL, ",");
        if (!csvEmail) continue;
        
        // Strip quotes
        if (csvEmail[0] == '"') csvEmail++;
        if (csvEmail[strlen(csvEmail)-1] == '"') csvEmail[strlen(csvEmail)-1] = '\0';
        
        if (strcasecmp(csvEmail, email) != 0) continue;
        
        tok = strtok(NULL, ","); // product
        tok = strtok(NULL, ","); // date
        
        char *csvIssue = strtok(NULL, ",");
        if (!csvIssue) continue;
        
        // Strip quotes
        if (csvIssue[0] == '"') csvIssue++;
        
        char csvIssuePrefix[31];
        strncpy(csvIssuePrefix, csvIssue, 30);
        csvIssuePrefix[30] = '\0';
        for (int i = 0; csvIssuePrefix[i]; i++) {
            csvIssuePrefix[i] = tolower(csvIssuePrefix[i]);
        }
        
        if (strcmp(issuePrefix, csvIssuePrefix) == 0) {
            // Found similar issue - check if within time window
            tok = strtok(NULL, ","); // priority
            tok = strtok(NULL, ","); // entry time
            
            if (tok) {
                time_t resolvedTime = (time_t)atol(tok);
                if (resolvedTime > cutoffTime) {
                    fclose(f);
                    return 1; // Recent duplicate found
                }
            }
        }
    }
    
    fclose(f);
    return 0; // No recent duplicate
}

/* ==================== CUSTOMER HISTORY ==================== */

int getCustomerHistory(const char *email, char history[][512], int maxHistory) {
    FILE *f = fopen("resolved_tickets.csv", "r");
    if (!f) return 0;
    
    int count = 0;
    char line[1024];
    fgets(line, sizeof(line), f); // Skip header
    
    while (fgets(line, sizeof(line), f) && count < maxHistory) {
        char lineCopy[1024];
        strncpy(lineCopy, line, 1023);
        lineCopy[1023] = '\0';
        
        // Parse to get email (3rd field)
        char *tok = strtok(lineCopy, ",");
        if (!tok) continue;
        tok = strtok(NULL, ","); // name
        if (!tok) continue;
        
        char *csvEmail = strtok(NULL, ",");
        if (!csvEmail) continue;
        
        // Strip quotes
        if (csvEmail[0] == '"') csvEmail++;
        if (csvEmail[strlen(csvEmail)-1] == '"') csvEmail[strlen(csvEmail)-1] = '\0';
        
        if (strcasecmp(csvEmail, email) == 0) {
            removeNewline(line);
            strncpy(history[count], line, 511);
            history[count][511] = '\0';
            count++;
        }
    }
    
    fclose(f);
    return count;
}

/* ==================== QUEUE STATISTICS ==================== */

void getQueueStats(int *total, double *avgWaitHours, int *oldestHours, int priorities[4]) {
    *total = 0;
    *avgWaitHours = 0.0;
    *oldestHours = 0;
    priorities[0] = priorities[1] = priorities[2] = priorities[3] = 0; // Critical, High, Medium, Low
    
    if (isEmpty()) return;
    
    time_t now = time(NULL);
    double totalWait = 0.0;
    
    int i = front;
    while (1) {
        (*total)++;
        
        // Calculate wait time
        double hours = difftime(now, queue[i].queueEntryTime) / 3600.0;
        totalWait += hours;
        
        if (hours > *oldestHours) {
            *oldestHours = (int)hours;
        }
        
        // Count priorities
        if (strcmp(queue[i].priority, "Critical") == 0) priorities[0]++;
        else if (strcmp(queue[i].priority, "High") == 0) priorities[1]++;
        else if (strcmp(queue[i].priority, "Medium") == 0) priorities[2]++;
        else priorities[3]++;
        
        if (i == rear) break;
        i = (i + 1) % MAX;
    }
    
    if (*total > 0) {
        *avgWaitHours = totalWait / (*total);
    }
}

/* ==================== AUTO-ESCALATION (24H CYCLES) ==================== */

/*
 * DESIGN DECISION: 24-hour escalation cycles
 * 
 * Why 24h instead of 48h/72h?
 * - Faster response to aging tickets
 * - Prevents tickets from languishing
 * - Creates urgency for support team
 * 
 * Escalation timeline:
 * Low: 0-24h Low, 24-48h Medium, 48h+ High
 * Medium: 0-24h Medium, 24h+ High
 * High: 0-24h High, 24h+ Critical
 * Critical: Always Critical
 */

void escalateOldTickets() {
    if (isEmpty()) return;
    
    time_t now = time(NULL);
    int i = front;
    int escalated = 0;
    
    while (1) {
        double hours = difftime(now, queue[i].queueEntryTime) / 3600.0;
        char oldPriority[20];
        strcpy(oldPriority, queue[i].priority);
        
        // FIXED: Complete 24-hour escalation with 72h Critical safety net
        // Rule: Every 24 hours, priority increases one step
        // Low → (24h) → Medium → (24h) → High → (24h) → Critical
        // Safety: ANY ticket ≥72h is forced to Critical
        
        if (strcmp(queue[i].priority, "Critical") != 0) {
            // SAFETY NET: Force any ticket ≥72 hours to Critical
            if (hours >= 72) {
                strcpy(queue[i].priority, "Critical");
                escalated++;
            }
            // Low priority escalation
            else if (strcmp(queue[i].priority, "Low") == 0) {
                if (hours >= 48) {
                    strcpy(queue[i].priority, "High");
                    escalated++;
                } else if (hours >= 24) {
                    strcpy(queue[i].priority, "Medium");
                    escalated++;
                }
            }
            // Medium priority escalation
            else if (strcmp(queue[i].priority, "Medium") == 0) {
                if (hours >= 24) {
                    strcpy(queue[i].priority, "High");
                    escalated++;
                }
            }
            // High priority escalation - FIXED: High → Critical after 24h
            else if (strcmp(queue[i].priority, "High") == 0) {
                if (hours >= 24) {
                    strcpy(queue[i].priority, "Critical");
                    escalated++;
                }
            }
        }
        
        if (i == rear) break;
        i = (i + 1) % MAX;
    }
    
    if (escalated > 0) {
        FILE *log = fopen("escalation_log.txt", "a");
        if (log) {
            char timeBuf[50];
            getSystemTime(timeBuf);
            fprintf(log, "[%s] Auto-escalated %d tickets\n", timeBuf, escalated);
            fclose(log);
        }
    }
}

/* ==================== CSV FILE OPERATIONS ==================== */

/*
 * SIMPLIFIED CSV STRUCTURE (removed unnecessary columns):
 * Old: 11 fields (including status, responseTime, resolveTime)
 * New: 8 fields (essential only)
 * Benefits: Cleaner data, fixes #### in Excel, easier to maintain
 */

void loadFromFile() {
    FILE *f = fopen("customer_support_tickets_updated.csv", "r");
    if (!f) {
        f = fopen("customer_support_tickets_updated.csv", "w");
        if (!f) {
            logError("Cannot create customer_support_tickets_updated.csv");
            return;
        }
        fprintf(f, "Ticket ID,Customer Name,Customer Email,Product,Purchase Date,Issue Description,Priority,Queue Entry Time\n");
        fclose(f);
        return;
    }

    char line[1024];
    fgets(line, sizeof(line), f); // Skip header

    front = rear = -1;
    int lineNumber = 1;  // Track line numbers for error reporting
    int validTickets = 0;
    int invalidTickets = 0;

    while (fgets(line, sizeof(line), f)) {
        lineNumber++;
        struct Ticket t;
        removeNewline(line);

        // Simple CSV parser that handles quoted fields
        char *fields[8];
        int fieldIndex = 0;
        char *ptr = line;
        char fieldBuffer[512];
        int bufferIndex = 0;
        int inQuotes = 0;

        while (*ptr && fieldIndex < 8) {
            if (*ptr == '"') {
                inQuotes = !inQuotes;
                ptr++;
                continue;
            }
            
            if (*ptr == ',' && !inQuotes) {
                fieldBuffer[bufferIndex] = '\0';
                fields[fieldIndex] = strdup(fieldBuffer);
                
                // ENHANCEMENT: NULL check for strdup
                if (!fields[fieldIndex]) {
                    char errMsg[256];
                    snprintf(errMsg, sizeof(errMsg), 
                             "Memory allocation failed at line %d - skipping", lineNumber);
                    logError(errMsg);
                    
                    // Free previously allocated fields
                    for (int j = 0; j < fieldIndex; j++) {
                        if (fields[j]) free(fields[j]);
                    }
                    goto next_line;  // Skip this line
                }
                
                fieldIndex++;
                bufferIndex = 0;
                ptr++;
                continue;
            }
            
            if (bufferIndex < 511) fieldBuffer[bufferIndex++] = *ptr;
            ptr++;
        }
        
        // Last field
        fieldBuffer[bufferIndex] = '\0';
        fields[fieldIndex] = strdup(fieldBuffer);
        
        // ENHANCEMENT: NULL check for last field
        if (!fields[fieldIndex]) {
            char errMsg[256];
            snprintf(errMsg, sizeof(errMsg), 
                     "Memory allocation failed at line %d - skipping", lineNumber);
            logError(errMsg);
            
            for (int j = 0; j < fieldIndex; j++) {
                if (fields[j]) free(fields[j]);
            }
            goto next_line;
        }
        
        fieldIndex++;

        // ENHANCEMENT: Better error message for malformed lines
        if (fieldIndex < 8) {
            char errMsg[256];
            snprintf(errMsg, sizeof(errMsg), 
                     "Line %d: Malformed CSV - %d fields (expected 8) - skipping", 
                     lineNumber, fieldIndex);
            logError(errMsg);
            
            // Free any allocated fields and skip malformed line
            for (int i = 0; i < fieldIndex; i++) {
                if (fields[i]) free(fields[i]);
            }
            invalidTickets++;
            continue;
        }

        // Parse fields
        t.ticketID = atoi(fields[0]);
        strncpy(t.customerName, fields[1], 99);
        t.customerName[99] = '\0';
        strncpy(t.email, fields[2], 99);
        t.email[99] = '\0';
        strncpy(t.product, fields[3], 99);
        t.product[99] = '\0';
        strncpy(t.purchaseDate, fields[4], 49);
        t.purchaseDate[49] = '\0';
        strncpy(t.issueDescription, fields[5], 199);
        t.issueDescription[199] = '\0';
        strncpy(t.priority, fields[6], 19);
        t.priority[19] = '\0';
        
        if (strlen(fields[7]) > 0) {
            t.queueEntryTime = (time_t)atol(fields[7]);
        } else {
            t.queueEntryTime = time(NULL);
        }

        // ENHANCEMENT: Validate parsed ticket data
        int validationFailed = 0;
        char validationMsg[256];
        
        if (!isValidTicketID(t.ticketID)) {
            snprintf(validationMsg, sizeof(validationMsg), 
                     "Line %d: Invalid ticket ID %d - skipping", lineNumber, t.ticketID);
            logError(validationMsg);
            validationFailed = 1;
        }
        
        if (!validationFailed && !isValidEmail(t.email)) {
            snprintf(validationMsg, sizeof(validationMsg), 
                     "Line %d: Invalid email '%s' for ticket #%d - skipping", 
                     lineNumber, t.email, t.ticketID);
            logError(validationMsg);
            validationFailed = 1;
        }
        
        if (!validationFailed && !isValidString(t.customerName, 2, MAX_CUSTOMER_NAME_LEN)) {
            snprintf(validationMsg, sizeof(validationMsg), 
                     "Line %d: Invalid customer name for ticket #%d - skipping", 
                     lineNumber, t.ticketID);
            logError(validationMsg);
            validationFailed = 1;
        }
        
        if (!validationFailed && !isValidPriority(t.priority)) {
            // Auto-correct invalid priority instead of failing
            snprintf(validationMsg, sizeof(validationMsg), 
                     "Line %d: Invalid priority '%s' for ticket #%d - defaulting to Low", 
                     lineNumber, t.priority, t.ticketID);
            logError(validationMsg);
            strcpy(t.priority, "Low");
        }
        
        if (validationFailed) {
            invalidTickets++;
        } else {
            enqueue(t);
            validTickets++;
        }

        // Free allocated strings
        for (int i = 0; i < fieldIndex; i++) {
            if (fields[i]) free(fields[i]);
        }
        
        next_line:
        continue;  // Label for goto in error handling
    }
    
    fclose(f);
    
    // Log loading summary
    if (invalidTickets > 0) {
        char summaryMsg[256];
        snprintf(summaryMsg, sizeof(summaryMsg), 
                 "CSV Load Summary: %d valid tickets loaded, %d invalid tickets skipped", 
                 validTickets, invalidTickets);
        logError(summaryMsg);
        printf("⚠️  Warning: %d invalid tickets skipped (check error_log.txt)\n", invalidTickets);
    }
}

/* ==================== ADMIN DASHBOARD GENERATION ==================== */

void generateAdminHTML() {
    // Write to temporary file first to prevent race conditions
    FILE *file = fopen("templates/admin_view.html.tmp", "w"); 
    if (!file) {
        logError("Cannot create admin_view.html.tmp");
        return;
    }

    // Get queue statistics
    int total = 0, oldestHours = 0;
    double avgWait = 0.0;
    int priorities[4] = {0, 0, 0, 0};
    getQueueStats(&total, &avgWait, &oldestHours, priorities);

    fprintf(file, "<!DOCTYPE html><html><head><title>Admin Dashboard</title>");
    fprintf(file, "<meta charset='UTF-8'>");
    fprintf(file, "<style>");
    fprintf(file, "body { font-family: 'Segoe UI', sans-serif; background: #f4f6f9; padding: 20px; margin: 0; }");
    
    fprintf(file, ".resolve-btn-top { position: sticky; top: 0; z-index: 1000; background: #27ae60; color: white; padding: 15px; text-align: center; margin: -20px -20px 20px -20px; box-shadow: 0 2px 10px rgba(0,0,0,0.2); }");
    fprintf(file, ".resolve-btn-top a { color: white; text-decoration: none; font-size: 16px; font-weight: bold; }");
    fprintf(file, ".resolve-btn-top a:hover { text-decoration: underline; }");
    
    // Stats card styling
    fprintf(file, ".stats-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 25px; }");
    fprintf(file, ".stat-card { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
    fprintf(file, ".stat-card h3 { margin: 0 0 5px 0; font-size: 14px; color: #7f8c8d; text-transform: uppercase; }");
    fprintf(file, ".stat-card .value { font-size: 32px; font-weight: bold; color: #2c3e50; }");
    fprintf(file, ".stat-card .subtext { font-size: 12px; color: #95a5a6; margin-top: 5px; }");
    fprintf(file, ".stat-card.critical { border-left: 4px solid #e74c3c; }");
    fprintf(file, ".stat-card.warning { border-left: 4px solid #f39c12; }");
    fprintf(file, ".stat-card.info { border-left: 4px solid #3498db; }");
    fprintf(file, ".stat-card.success { border-left: 4px solid #27ae60; }");
    
    fprintf(file, "table { width: 100%%; border-collapse: collapse; background: white; box-shadow: 0 4px 8px rgba(0,0,0,0.1); border-radius: 8px; overflow: hidden; }");
    fprintf(file, "th, td { padding: 15px; text-align: left; border-bottom: 1px solid #ddd; vertical-align: middle; }");
    fprintf(file, "th { background-color: #2c3e50; color: white; text-transform: uppercase; font-size: 13px; letter-spacing: 0.5px; }");
    fprintf(file, "tr:hover { background-color: #f8f9fa; }");
    
    // Age-based row highlighting
    fprintf(file, ".age-critical { background-color: #fadbd8 !important; }");
    fprintf(file, ".age-warning { background-color: #fdebd0 !important; }");
    fprintf(file, ".age-caution { background-color: #fff9e6 !important; }");
    
    fprintf(file, ".Critical { color: #c0392b; font-weight: bold; background: #fadbd8; padding: 4px 8px; border-radius: 4px; font-size: 12px; }");
    fprintf(file, ".High { color: #e67e22; font-weight: bold; background: #fdebd0; padding: 4px 8px; border-radius: 4px; font-size: 12px; }");
    fprintf(file, ".Medium { color: #2980b9; background: #d6eaf8; padding: 4px 8px; border-radius: 4px; font-size: 12px; }");
    fprintf(file, ".Low { color: #27ae60; background: #d5f5e3; padding: 4px 8px; border-radius: 4px; font-size: 12px; }");
    fprintf(file, ".logout-btn { float: right; background: #e74c3c; color: white; padding: 10px 20px; text-decoration: none; border-radius: 30px; font-weight: bold; font-size: 14px; box-shadow: 0 2px 5px rgba(231, 76, 60, 0.3); }");
    fprintf(file, ".logout-btn:hover { background: #c0392b; transform: translateY(-2px); }");
    fprintf(file, ".subtext { display: block; font-size: 12px; color: #7f8c8d; margin-top: 4px; }");
    fprintf(file, ".age-badge { font-size: 11px; padding: 3px 6px; border-radius: 3px; font-weight: 600; }");
    fprintf(file, ".age-critical-badge { background: #e74c3c; color: white; }");
    fprintf(file, ".age-warning-badge { background: #f39c12; color: white; }");
    fprintf(file, ".age-caution-badge { background: #f1c40f; color: #333; }");
    fprintf(file, ".history-tooltip { font-size: 11px; color: #3498db; margin-left: 8px; cursor: help; }");
    fprintf(file, ".priority-select { padding: 5px 8px; border: 1px solid #ddd; border-radius: 4px; background: white; font-size: 12px; cursor: pointer; font-weight: 600; }");
    fprintf(file, ".priority-select:hover { border-color: #3498db; }");
    fprintf(file, ".priority-Critical { background: #fadbd8; color: #c0392b; border-color: #c0392b; }");
    fprintf(file, ".priority-High { background: #fdebd0; color: #e67e22; border-color: #e67e22; }");
    fprintf(file, ".priority-Medium { background: #d6eaf8; color: #2980b9; border-color: #2980b9; }");
    fprintf(file, ".priority-Low { background: #d5f5e3; color: #27ae60; border-color: #27ae60; }");
    fprintf(file, "</style>");
    fprintf(file, "</head><body>");
    
    if (!isEmpty()) {
        fprintf(file, "<div class='resolve-btn-top'>");
        fprintf(file, "<a href='/resolve/%d'>⚡ Resolve Next Ticket (FIFO) - #%d ✅</a>", queue[front].ticketID, queue[front].ticketID);
        fprintf(file, "</div>");
    }
    
    fprintf(file, "<div style='overflow: hidden; margin-bottom: 20px;'>");
    fprintf(file, "<a href='/' class='logout-btn'>Logout</a>");
    fprintf(file, "<h2 style='color: #2c3e50; margin: 0;'>🚀 Live Support Dashboard</h2>");
    fprintf(file, "<p style='color: #7f8c8d; margin: 5px 0 0 0;'>Real-time ticket monitoring system (FIFO Circular Queue)</p>");
    fprintf(file, "</div>");

    // Statistics Dashboard
    fprintf(file, "<div class='stats-container'>");
    
    // Total Tickets
    fprintf(file, "<div class='stat-card info'>");
    fprintf(file, "<h3>📊 Total in Queue</h3>");
    fprintf(file, "<div class='value'>%d</div>", total);
    fprintf(file, "<div class='subtext'>Capacity: %d/%d (%.1f%%)</div>", total, MAX, (total * 100.0) / MAX);
    fprintf(file, "</div>");
    
    // Average Wait Time
    char waitClass[20] = "success";
    if (avgWait > 48) strcpy(waitClass, "critical");
    else if (avgWait > 24) strcpy(waitClass, "warning");
    fprintf(file, "<div class='stat-card %s'>", waitClass);
    fprintf(file, "<h3>⏱️ Avg Wait Time</h3>");
    fprintf(file, "<div class='value'>%.1fh</div>", avgWait);
    fprintf(file, "<div class='subtext'>Average across all tickets</div>");
    fprintf(file, "</div>");
    
    // Oldest Ticket
    char oldestClass[20] = "success";
    if (oldestHours > 72) strcpy(oldestClass, "critical");
    else if (oldestHours > 48) strcpy(oldestClass, "warning");
    fprintf(file, "<div class='stat-card %s'>", oldestClass);
    fprintf(file, "<h3>⚠️ Oldest Ticket</h3>");
    fprintf(file, "<div class='value'>%dh</div>", oldestHours);
    fprintf(file, "<div class='subtext'>Waiting time of longest ticket</div>");
    fprintf(file, "</div>");
    
    // Priority Breakdown
    fprintf(file, "<div class='stat-card info'>");
    fprintf(file, "<h3>🎯 Priority Distribution</h3>");
    fprintf(file, "<div style='font-size: 14px; margin-top: 10px;'>");
    fprintf(file, "<span class='Critical' style='margin-right: 8px;'>Critical: %d</span>", priorities[0]);
    fprintf(file, "<span class='High' style='margin-right: 8px;'>High: %d</span>", priorities[1]);
    fprintf(file, "<br><span class='Medium' style='margin-right: 8px; margin-top: 5px; display: inline-block;'>Medium: %d</span>", priorities[2]);
    fprintf(file, "<span class='Low'>Low: %d</span>", priorities[3]);
    fprintf(file, "</div></div>");
    
    fprintf(file, "</div>"); // End stats-container

    fprintf(file, "<table>");
    fprintf(file, "<tr><th width='5%%'>ID</th><th width='20%%'>Customer Details</th><th width='20%%'>Product Info</th><th width='25%%'>Issue Description</th><th width='12%%'>Priority</th><th width='10%%'>Wait Time</th><th width='8%%'>History</th></tr>");

    if (!isEmpty()) {
        time_t now = time(NULL);
        int i = front;
        while (1) {
            double hours = difftime(now, queue[i].queueEntryTime) / 3600.0;
            
            // Determine row class based on age
            char rowClass[50] = "";
            if (hours > 72) strcpy(rowClass, "class='age-critical'");
            else if (hours > 48) strcpy(rowClass, "class='age-warning'");
            else if (hours > 24) strcpy(rowClass, "class='age-caution'");
            
            fprintf(file, "<tr %s>", rowClass);
            fprintf(file, "<td><strong>#%d</strong></td>", queue[i].ticketID);
            
            fprintf(file, "<td><span style='font-weight:600; color:#2c3e50;'>%s</span><span class='subtext'>✉️ %s</span></td>", 
                    queue[i].customerName, queue[i].email);

            fprintf(file, "<td><span style='font-weight:600; color:#2c3e50;'>%s</span><span class='subtext'>📅 %s</span></td>", 
                    queue[i].product, queue[i].purchaseDate);

            fprintf(file, "<td>%s</td>", queue[i].issueDescription);
            
            // Priority dropdown for editing with color coding
            fprintf(file, "<td>");
            fprintf(file, "<select class='priority-select priority-%s' onchange='updatePriority(%d, this.value)'>", 
                    queue[i].priority, queue[i].ticketID);
            fprintf(file, "<option value='Low' %s>Low</option>", strcmp(queue[i].priority, "Low") == 0 ? "selected" : "");
            fprintf(file, "<option value='Medium' %s>Medium</option>", strcmp(queue[i].priority, "Medium") == 0 ? "selected" : "");
            fprintf(file, "<option value='High' %s>High</option>", strcmp(queue[i].priority, "High") == 0 ? "selected" : "");
            fprintf(file, "<option value='Critical' %s>Critical</option>", strcmp(queue[i].priority, "Critical") == 0 ? "selected" : "");
            fprintf(file, "</select>");
            fprintf(file, "</td>");
            
            // Wait time with badges
            char ageBadgeClass[50] = "";
            if (hours > 72) strcpy(ageBadgeClass, "age-critical-badge");
            else if (hours > 48) strcpy(ageBadgeClass, "age-warning-badge");
            else if (hours > 24) strcpy(ageBadgeClass, "age-caution-badge");
            
            if (strlen(ageBadgeClass) > 0) {
                fprintf(file, "<td><span class='age-badge %s'>%.1fh</span></td>", ageBadgeClass, hours);
            } else {
                fprintf(file, "<td>%.1fh</td>", hours);
            }
            
            // Customer history count
            char historyLines[10][512];
            int historyCount = getCustomerHistory(queue[i].email, historyLines, 10);
            if (historyCount > 0) {
                fprintf(file, "<td><span class='history-tooltip' title='%d previous tickets'>📋 %d</span></td>", 
                        historyCount, historyCount);
            } else {
                fprintf(file, "<td style='color: #bdc3c7;'>-</td>");
            }
            
            fprintf(file, "</tr>");

            if (i == rear) break;
            i = (i + 1) % MAX;
        }
    } else {
        fprintf(file, "<tr><td colspan='7' style='text-align:center; padding: 40px; color: #95a5a6;'><h3>No Pending Tickets! 🎉</h3><p>Good job team, all caught up.</p></td></tr>");
    }

    fprintf(file, "</table>");

    fprintf(file, "<div style='text-align:center; margin-top:20px; color:#bdc3c7; font-size:12px;'>");
    fprintf(file, "System Auto-Refreshes every 15s | Auto-escalation: Low→Medium (24h), Medium→High (24h), High→Critical (24h)");
    fprintf(file, "</div>");
    // JavaScript for priority update
    fprintf(file, "<script>");
    fprintf(file, "function updatePriority(ticketId, newPriority) {");
    fprintf(file, "  fetch('/update_priority/' + ticketId + '/' + newPriority, { method: 'POST' })");
    fprintf(file, "    .then(res => res.json())");
    fprintf(file, "    .then(data => {");
    fprintf(file, "      if (data.success) {");
    fprintf(file, "        alert('Priority updated to ' + newPriority);");
    fprintf(file, "        location.reload();");
    fprintf(file, "      } else {");
    fprintf(file, "        alert('Error: ' + data.error);");
    fprintf(file, "      }");
    fprintf(file, "    });");
    fprintf(file, "}");
    
    fprintf(file, "var isRefreshing = false;");
    fprintf(file, "var hasClickedResolve = false;");
    
    fprintf(file, "document.addEventListener('DOMContentLoaded', function() {");
    fprintf(file, "  var resolveLinks = document.querySelectorAll('a[href*=\"/resolve/\"]');");
    fprintf(file, "  resolveLinks.forEach(function(link) {");
    fprintf(file, "    link.addEventListener('click', function(e) {");
    fprintf(file, "      if (hasClickedResolve) {");
    fprintf(file, "        e.preventDefault();");
    fprintf(file, "        return false;");
    fprintf(file, "      }");
    fprintf(file, "      hasClickedResolve = true;");
    fprintf(file, "    });");
    fprintf(file, "  });");
    fprintf(file, "});");
    
    fprintf(file, "setTimeout(function() {");
    fprintf(file, "  if (!isRefreshing && !hasClickedResolve) {");
    fprintf(file, "    isRefreshing = true;");
    fprintf(file, "    location.reload();");
    fprintf(file, "  }");
    fprintf(file, "}, 5000);");
    
    fprintf(file, "</script>");
    fprintf(file, "</body></html>");
    fclose(file);
    
    // Atomic rename - prevents race conditions with Flask reading file
    remove("templates/admin_view.html");
    rename("templates/admin_view.html.tmp", "templates/admin_view.html");
}

/* ==================== TICKET RESOLUTION ==================== */

void archiveAndRemove(int id, const char *admin_username) {
    FILE *src = fopen("customer_support_tickets_updated.csv", "r");
    if (!src) {
        logError("Cannot open customer_support_tickets_updated.csv for archiving");
        return;
    }
    
    FILE *tmp = fopen("temp.csv", "w");
    if (!tmp) {
        logError("Cannot create temp.csv");
        fclose(src);
        return;
    }
    
    FILE *arc = fopen("resolved_tickets.csv", "a");

    // Check if resolved_tickets.csv exists, if not create with header
    if (!arc) {
        arc = fopen("resolved_tickets.csv", "w");
        fprintf(arc, "Ticket ID,Customer Name,Customer Email,Product,Purchase Date,Issue Description,Priority,Queue Entry Time,Resolved At,Resolved By\n");
        fclose(arc);
        arc = fopen("resolved_tickets.csv", "a");
    }

    char line[1024];
    fgets(line, sizeof(line), src);
    fprintf(tmp, "%s", line); // Copy header

    int found = 0;
    while (fgets(line, sizeof(line), src)) {
        char copy[1024];
        strncpy(copy, line, 1023);  //  Safe
        copy[1023] = '\0';           //  Null terminate
        
        // Parse ticket ID - handle quoted fields
        char *tok = strtok(copy, ",");
        if (tok) {
            // Remove quotes if present
            if (tok[0] == '"') tok++;
            int curr = atoi(tok);

            if (curr == id) {
                // Found the ticket to resolve
                char timeBuf[50];
                getSystemTime(timeBuf);
                removeNewline(line);
                
                // Append resolved timestamp AND admin username
                fprintf(arc, "%s,%s,%s\n", line, timeBuf, admin_username);
                found = 1;
            } else {
                fprintf(tmp, "%s", line);
            }
        } else {
            fprintf(tmp, "%s", line);
        }
    }

    fclose(src); fclose(tmp); fclose(arc);
    
    if (found) {
        remove("customer_support_tickets_updated.csv");
        rename("temp.csv", "customer_support_tickets_updated.csv");
    } else {
        remove("temp.csv");
    }
}

void resolveNextTicket(const char *admin_username) {
    struct Ticket t;
    if (!dequeue(&t)) return;
    archiveAndRemove(t.ticketID, admin_username);
    loadFromFile();
    generateAdminHTML();
}

/* ==================== PENDING TICKET PROCESSING ==================== */

void processPendingTickets() {
    FILE *pf = fopen("pending_tickets.csv", "r");
    if (!pf) return;

    FILE *db = fopen("customer_support_tickets_updated.csv", "a");
    FILE *duplicates = fopen("duplicate_tickets.log", "a");
    
    char line[1024];
    time_t entryTime = time(NULL);

    while (fgets(line, sizeof(line), pf)) {
        struct Ticket t;
        removeNewline(line);

        // Simple CSV parser that handles quoted fields
        char *fields[6];
        int fieldIndex = 0;
        char *ptr = line;
        char fieldBuffer[512];
        int bufferIndex = 0;
        int inQuotes = 0;

        while (*ptr && fieldIndex < 6) {
            if (*ptr == '"') {
                inQuotes = !inQuotes;
                ptr++;
                continue;
            }
            
            if (*ptr == ',' && !inQuotes) {
                fieldBuffer[bufferIndex] = '\0';
                fields[fieldIndex] = strdup(fieldBuffer);
                fieldIndex++;
                bufferIndex = 0;
                ptr++;
                continue;
            }
            
            if (bufferIndex < 511) fieldBuffer[bufferIndex++] = *ptr;
            ptr++;
        }
        
        // Last field
        fieldBuffer[bufferIndex] = '\0';
        fields[fieldIndex] = strdup(fieldBuffer);
        fieldIndex++;

        if (fieldIndex < 6) {
            for (int i = 0; i < fieldIndex; i++) {
                free(fields[i]);
            }
            continue;
        }

        t.ticketID = atoi(fields[0]);
        strncpy(t.customerName, fields[1], 99);
        t.customerName[99] = '\0';
        strncpy(t.email, fields[2], 99);
        t.email[99] = '\0';
        strncpy(t.product, fields[3], 99);
        t.product[99] = '\0';
        strncpy(t.purchaseDate, fields[4], 49);
        t.purchaseDate[49] = '\0';
        strncpy(t.issueDescription, fields[5], 199);
        t.issueDescription[199] = '\0';

        // DUPLICATE DETECTION
        int existingTicketID = isDuplicateInQueue(t.email, t.issueDescription);
        
        if (existingTicketID > 0) {
            // Log duplicate and skip
            char timeBuf[50];
            getSystemTime(timeBuf);
            fprintf(duplicates, "[%s] Duplicate rejected: Ticket #%d (similar to #%d) - %s - %s\n",
                    timeBuf, t.ticketID, existingTicketID, t.email, t.issueDescription);
            
            for (int i = 0; i < fieldIndex; i++) {
                free(fields[i]);
            }
            continue; // Skip this duplicate ticket
        }

        // If not duplicate, process normally
        strncpy(t.priority, getAutoPriority(t.issueDescription), 19);
        t.priority[19] = '\0';
        t.queueEntryTime = entryTime;

        enqueue(t);

        // Write to CSV with simplified structure
        fprintf(db, "%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%s,%ld\n",
            t.ticketID, t.customerName, t.email,
            t.product, t.purchaseDate,
            t.issueDescription, t.priority, (long)entryTime);

        // Free allocated strings
        for (int i = 0; i < fieldIndex; i++) {
            free(fields[i]);
        }
    }

    fclose(pf);
    fclose(db);
    fclose(duplicates);

    // Clear pending tickets (they're now in active queue)
    pf = fopen("pending_tickets.csv", "w");
    fclose(pf);

    loadFromFile();
}

/* ==================== ADMIN COMMANDS ==================== */

void checkAdminCommands() {
    FILE *cmd = fopen("admin_commands.txt", "r");
    if (!cmd) return;

    char line[256];
    if (fgets(line, sizeof(line), cmd)) {
        int id;
        char admin_username[100] = "admin";  // fallback default
        
        // Parse: "RESOLVE <id> <admin_username>"
        if (sscanf(line, "RESOLVE %d %99s", &id, admin_username) >= 1) {
            resolveNextTicket(admin_username);
        }
    }

    fclose(cmd);
    // Clear command file
    cmd = fopen("admin_commands.txt", "w");
    fclose(cmd);
}

/* ==================== CLEANUP AND STATE PERSISTENCE ==================== */

void saveQueueToFile() {
    /*
     * Saves current queue state to CSV file.
     * Called during graceful shutdown to preserve data.
     */
    FILE *f = fopen(PENDING_TICKETS_FILE, "w");
    if (!f) {
        logError("Cannot save queue state during shutdown");
        return;
    }
    
    fprintf(f, "Ticket ID,Customer Name,Customer Email,Product,Purchase Date,Issue Description,Priority,Queue Entry Time\n");
    
    if (!isEmpty()) {
        int i = front;
        while (1) {
            fprintf(f, "%d,\"%s\",\"%s\",\"%s\",%s,\"%s\",%s,%ld\n",
                    queue[i].ticketID,
                    queue[i].customerName,
                    queue[i].email,
                    queue[i].product,
                    queue[i].purchaseDate,
                    queue[i].issueDescription,
                    queue[i].priority,
                    (long)queue[i].queueEntryTime);
            
            if (i == rear) break;
            i = (i + 1) % MAX;
        }
    }
    
    fclose(f);
}

void cleanup() {
    /*
     * Graceful shutdown cleanup routine.
     * Saves queue state and generates final HTML dashboard.
     */
    printf("\n Performing cleanup tasks...\n");
    
    // Save current queue state
    printf("   [1/3] Saving queue state to CSV... ");
    fflush(stdout);
    saveQueueToFile();
    printf("ok\n");
    
    // Generate final HTML snapshot
    printf("   [2/3] Generating final admin dashboard... ");
    fflush(stdout);
    generateAdminHTML();
    printf("ok\n");
    
    // Display final statistics
    printf("   [3/3] Final Statistics:\n");
    int total = 0, oldestHours = 0;
    double avgWait = 0.0;
    int priorities[4] = {0, 0, 0, 0};
    getQueueStats(&total, &avgWait, &oldestHours, priorities);
    
    printf("         • Tickets in queue: %d\n", total);
    printf("         • Average wait time: %.1f hours\n", avgWait);
    printf("         • Priority breakdown: Critical=%d, High=%d, Medium=%d, Low=%d\n",
           priorities[0], priorities[1], priorities[2], priorities[3]);
    printf("   ok\n");
    
    printf("\n");
    printf("  Cleanup complete. All data saved. Goodbye!              \n");
    printf("\n");
}

/* ==================== MAIN LOOP ==================== */

#ifndef TESTING
int main() {
    printf("\n");
    printf("\n");
    printf("  Customer Support Ticketing System (DSA Project)           \n");
    printf("  Data Structure: Circular Queue (FIFO)                     \n");
    printf("  Enhanced with: Error Handling, Validation, Unit Tests     \n");
    printf("\n");
    
    printf("Features:\n");
    printf("   - FIFO Processing (Circular Queue)\n");
    printf("   - Auto-Escalation (%d hour cycles)\n", ESCALATION_CYCLE_HOURS);
    printf("   - Duplicate Detection\n");
    printf("   - Customer History Tracking\n");
    printf("   - Real-time Statistics\n");
    printf("   - Input Validation & Error Handling\n");
    printf("   - Graceful Shutdown Support\n\n");
    
    printf("Configuration:\n");
    printf("   - Queue Capacity: %d tickets\n", MAX_QUEUE_SIZE);
    printf("   - Escalation Cycle: %d hours\n", ESCALATION_CYCLE_HOURS);
    printf("   - Safety Net: %d hours → Critical\n\n", SAFETY_NET_HOURS);
    
    printf("System starting...\n");
    
    // Setup signal handlers for graceful shutdown
    setupSignalHandlers();
    
    // Load existing tickets from CSV
    loadFromFile();
    
    // Generate initial admin dashboard
    generateAdminHTML();
    
    printf(" System ready. Press Ctrl+C for graceful shutdown.\n\n");

    int cycles = 0;
    while (running) {  // Changed from while(1) to while(running)
        processPendingTickets();
        escalateOldTickets();
        checkAdminCommands();
        
        // Regenerate HTML every N cycles (configurable)
        // This reduces file I/O and race conditions while still being responsive
        if (cycles % HTML_GENERATION_CYCLES == 0) {
            generateAdminHTML();
        }
        
        cycles++;
        
        // Display statistics periodically
        if (cycles % STATS_DISPLAY_CYCLES == 0) {
            int total = 0, oldestHours = 0;
            double avgWait = 0.0;
            int priorities[4] = {0, 0, 0, 0};
            getQueueStats(&total, &avgWait, &oldestHours, priorities);
            
            printf("[Status] Tickets: %d | Avg Wait: %.1fh | Oldest: %dh | Critical: %d High: %d Med: %d Low: %d\n",
                   total, avgWait, oldestHours, priorities[0], priorities[1], priorities[2], priorities[3]);
        }
        
        // Sleep using configured interval
        #ifdef _WIN32
            Sleep(SLEEP_MILLISECONDS);
        #else
            usleep(SLEEP_MILLISECONDS * 1000);  // Convert milliseconds to microseconds
        #endif
    }
    
    // Graceful shutdown cleanup
    cleanup();
    
    return 0;
}
#endif /* TESTING */