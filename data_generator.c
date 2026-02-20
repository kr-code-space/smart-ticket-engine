#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

// CONFIGURATION: Direct access to main database
#define DB_FILE "customer_support_tickets_updated.csv"
#define CONFIG_FILE "GENERATOR_CONFIG.json"

// ==================== DATA STRUCTURES ====================

#define MAX_NAMES 200
#define MAX_PRODUCTS 30
#define MAX_KEYWORDS 30
#define MAX_SENTENCES 50
#define STR_LEN 100

char first_names[MAX_NAMES][STR_LEN];
int first_name_count = 0;
char last_names[MAX_NAMES][STR_LEN];
int last_name_count = 0;
char domains[20][STR_LEN];
int domain_count = 0;
char suffixes[MAX_SENTENCES][STR_LEN];
int suffix_count = 0;
char details[MAX_SENTENCES][STR_LEN];
int detail_count = 0;

struct ProductType {
    char name[STR_LEN];
    char keywords[MAX_KEYWORDS][STR_LEN];
    int keyword_count;
};
struct ProductType products[MAX_PRODUCTS];
int product_count = 0;

const char *priorities[] = {"Low", "Medium", "High", "Critical"};

// ==================== UTILS ====================

int randomInt(int min, int max) {
    return min + rand() % (max - min + 1);
}

char* read_file(const char* filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0';
    }
    fclose(f);
    return buffer;
}

// ==================== JSON PARSER ====================
// (Simplified parser as provided before)

int parse_json_array(const char *json, const char *key, char target[][STR_LEN]) {
    char searchKey[100];
    sprintf(searchKey, "\"%s\"", key);
    char *start = strstr(json, searchKey);
    if (!start) return 0;
    start = strchr(start, '[');
    if (!start) return 0;
    start++;
    char *end = strchr(start, ']');
    if (!end) return 0;
    
    int count = 0;
    char *curr = start;
    while (curr < end) {
        char *valStart = strchr(curr, '"');
        if (!valStart || valStart > end) break;
        valStart++;
        char *valEnd = strchr(valStart, '"');
        if (!valEnd) break;
        int len = valEnd - valStart;
        if (len >= STR_LEN) len = STR_LEN - 1;
        strncpy(target[count], valStart, len);
        target[count][len] = '\0';
        count++;
        curr = valEnd + 1;
        if (count >= MAX_NAMES) break;
    }
    return count;
}

void load_products(const char *json) {
    char *prodSection = strstr(json, "\"products\"");
    if (!prodSection) return;
    char *start = strchr(prodSection, '{');
    start++;
    char *curr = start;
    while (curr && *curr) {
        char *keyStart = strchr(curr, '"');
        if (!keyStart) break;
        keyStart++;
        char *keyEnd = strchr(keyStart, '"');
        if (!keyEnd) break;
        int pLen = keyEnd - keyStart;
        if (pLen >= STR_LEN) pLen = STR_LEN - 1;
        strncpy(products[product_count].name, keyStart, pLen);
        products[product_count].name[pLen] = '\0';
        
        char *kwKey = strstr(keyEnd, "\"keywords\"");
        if (!kwKey) break;
        char *kwStart = strchr(kwKey, '[');
        char *kwEnd = strchr(kwStart, ']');
        int kCount = 0;
        char *kCurr = kwStart;
        while (kCurr < kwEnd) {
            char *wStart = strchr(kCurr, '"');
            if (!wStart || wStart > kwEnd) break;
            wStart++;
            char *wEnd = strchr(wStart, '"');
            int wLen = wEnd - wStart;
            if (wLen >= STR_LEN) wLen = STR_LEN - 1;
            strncpy(products[product_count].keywords[kCount], wStart, wLen);
            products[product_count].keywords[kCount][wLen] = '\0';
            kCount++;
            kCurr = wEnd + 1;
        }
        products[product_count].keyword_count = kCount;
        product_count++;
        char *objEnd = strchr(kwEnd, '}'); 
        if (!objEnd) break;
        curr = objEnd + 1;
        if (product_count >= MAX_PRODUCTS) break;
    }
}

void init_data() {
    char *json = read_file(CONFIG_FILE);
    if (!json) {
        printf(" Error: Could not read %s. Using defaults.\n", CONFIG_FILE);
        exit(1);
    }
    first_name_count = parse_json_array(json, "first_names", first_names);
    last_name_count = parse_json_array(json, "last_names", last_names);
    domain_count = parse_json_array(json, "domains", domains);
    suffix_count = parse_json_array(json, "suffixes", suffixes);
    detail_count = parse_json_array(json, "details", details);
    load_products(json);
    free(json);
}

// ==================== LOGIC ====================

// Find the highest ticket ID currently in the DB
int get_next_id() {
    FILE *f = fopen(DB_FILE, "r");
    if (!f) return 1000; // Start here if file doesn't exist

    int max_id = 1000;
    char line[1024];
    
    // Skip header
    fgets(line, sizeof(line), f);

    while (fgets(line, sizeof(line), f)) {
        // Simple parse to get the first number (ID)
        int id = atoi(line);
        if (id > max_id) max_id = id;
    }
    fclose(f);
    return max_id + 1;
}

void get_product_and_issue(char *prod_buf, char *issue_buf) {
    if (product_count == 0) {
        strcpy(prod_buf, "Unknown");
        strcpy(issue_buf, "Unknown issue");
        return;
    }
    int p_idx = randomInt(0, product_count - 1);
    strcpy(prod_buf, products[p_idx].name);
    
    const char *keyword = "issue";
    if (products[p_idx].keyword_count > 0) {
        keyword = products[p_idx].keywords[randomInt(0, products[p_idx].keyword_count - 1)];
    }
    const char *suf = (suffix_count > 0) ? suffixes[randomInt(0, suffix_count - 1)] : "broken";
    const char *det = (detail_count > 0) ? details[randomInt(0, detail_count - 1)] : "help";
    
    sprintf(issue_buf, "%s %s ; %s", keyword, suf, det);
}

int main() {
    srand(time(NULL));
    init_data();

    int n;
    printf("\n SMART TICKET GENERATOR (Live Append Mode)\n");
    printf("-------------------------------------------\n");
    printf("Target Database: %s\n", DB_FILE);
    
    // Auto-detect next ID
    int next_id = get_next_id();
    printf("Starting Ticket ID: #%d (Auto-detected)\n", next_id);
    
    printf("\nHow many tickets to generate? ");
    scanf("%d", &n);

    // Append mode "a" is critical here!
    FILE *fp = fopen(DB_FILE, "a");
    if (!fp) {
        printf(" Error opening database file!\n");
        return 1;
    }

    printf("\nGenerating %d tickets...\n", n);
    
    long current_time = time(NULL);

    for (int i = 0; i < n; i++) {
        int id = next_id + i;
        
        // Generate fields
        const char *fname = first_names[randomInt(0, first_name_count - 1)];
        const char *lname = last_names[randomInt(0, last_name_count - 1)];
        char full_name[100];
        sprintf(full_name, "%s %s", fname, lname);

        char email[100];
        char f_lower[50], l_lower[50];
        strcpy(f_lower, fname); strcpy(l_lower, lname);
        for(int k=0; f_lower[k]; k++) f_lower[k] = tolower(f_lower[k]);
        for(int k=0; l_lower[k]; k++) l_lower[k] = tolower(l_lower[k]);
        sprintf(email, "%s.%s%d@%s", f_lower, l_lower, randomInt(1, 999), domains[randomInt(0, domain_count - 1)]);

        char product[50], issue[256];
        get_product_and_issue(product, issue);

        char date[20];
        sprintf(date, "%04d-%02d-%02d", randomInt(2023, 2025), randomInt(1, 12), randomInt(1, 28));
        const char *prio = priorities[randomInt(0, 3)];
        
        // Spread timestamps slightly into the past (e.g., last 10 mins) so they don't look instant
        long ticket_time = current_time - randomInt(0, 600); 

        // Write directly to CSV
        fprintf(fp, "%d,\"%s\",\"%s\",\"%s\",%s,\"%s\",%s,%ld\n",
            id, full_name, email, product, date, issue, prio, ticket_time);
            
        if (i % 50 == 0) { printf("."); fflush(stdout); }
    }

    fclose(fp);
    printf("\n\n Success! Appended %d tickets to %s\n", n, DB_FILE);
    printf("   New ID Range: #%d - #%d\n", next_id, next_id + n - 1);
    printf("   Refresh your Admin Dashboard to see them!\n");

    return 0;
}