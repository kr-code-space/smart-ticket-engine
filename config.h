#ifndef CONFIG_H
#define CONFIG_H

/* ==================== SMART TICKET ENGINE CONFIGURATION ==================== */
/*
 * Central configuration file for all system constants.
 * Modify these values to customize system behavior.
 * 
 * For production deployment, consider moving to external config file.
 */

/* ==================== QUEUE SETTINGS ==================== */

// Maximum number of tickets in queue
// Recommendation: 10000 for academic projects, scale up for production
#define MAX_QUEUE_SIZE 10000

// Queue capacity warning threshold (percentage)
#define QUEUE_WARNING_THRESHOLD 80  // Alert when 80% full

/* ==================== ESCALATION SETTINGS ==================== */

// Hours between automatic priority escalations
// Low -> Medium -> High -> Critical
#define ESCALATION_CYCLE_HOURS 24

// Safety net: Force any ticket to Critical after this many hours
// Prevents tickets from being stuck indefinitely
#define SAFETY_NET_HOURS 72

/* ==================== PERFORMANCE TUNING ==================== */

// Regenerate admin HTML every N cycles
// Higher = less file I/O, lower = more responsive dashboard
#define HTML_GENERATION_CYCLES 4

// Main loop sleep time in milliseconds
// 500ms = responsive without excessive CPU usage
#define SLEEP_MILLISECONDS 500

// Display console statistics every N cycles
// 30 cycles * 500ms = every 15 seconds
#define STATS_DISPLAY_CYCLES 30

/* ==================== FILE PATHS ==================== */

// Primary data files
#define PENDING_TICKETS_FILE "customer_support_tickets_updated.csv"
#define RESOLVED_TICKETS_FILE "resolved_tickets.csv"
#define ADMIN_COMMANDS_FILE "admin_commands.txt"

// Log files
#define ERROR_LOG_FILE "error_log.txt"
#define OVERFLOW_LOG_FILE "overflow_log.txt"
#define ESCALATION_LOG_FILE "escalation_log.txt"
#define DUPLICATE_LOG_FILE "duplicate_tickets.log"

// Template files
#define ADMIN_TEMPLATE "templates/admin_view.html"
#define ADMIN_TEMPLATE_TMP "templates/admin_view.html.tmp"

/* ==================== VALIDATION LIMITS ==================== */

// Ticket field length limits
#define MAX_CUSTOMER_NAME_LEN 99
#define MAX_EMAIL_LEN 99
#define MAX_PRODUCT_LEN 99
#define MAX_PURCHASE_DATE_LEN 49
#define MAX_ISSUE_DESC_LEN 199
#define MAX_PRIORITY_LEN 19

// Ticket ID validation range
#define MIN_TICKET_ID 1
#define MAX_TICKET_ID 999999

// Email validation
#define MIN_EMAIL_LEN 3

/* ==================== ERROR CODES ==================== */

// Standardized return codes for consistency
// Note: Prefixed with TICKET_ to avoid conflicts with Windows error codes
#define SUCCESS 1
#define TICKET_ERROR_FILE_OPEN -1
#define TICKET_ERROR_MEMORY -2
#define TICKET_ERROR_INVALID_DATA -3
#define TICKET_ERROR_QUEUE_FULL -4
#define TICKET_ERROR_QUEUE_EMPTY -5

/* ==================== DUPLICATE DETECTION ==================== */

// Number of characters to compare for duplicate detection
#define DUPLICATE_CHECK_PREFIX_LEN 30

// Days to look back in resolved tickets for duplicates
#define DUPLICATE_LOOKBACK_DAYS 7

/* ==================== CUSTOMER HISTORY ==================== */

// Maximum number of previous tickets to retrieve
#define MAX_CUSTOMER_HISTORY 10

#endif /* CONFIG_H */
