from flask import Flask, render_template, request, redirect, url_for, flash, jsonify, session
import csv
import os
import time
import re
import hashlib
import secrets
import html as html_lib
from datetime import datetime, timedelta
import json

app = Flask(__name__, template_folder='templates', static_folder='static')

# ==================== SECURITY: Random Secret Key ====================
# Generates random secret key for each server instance
# NOTE: Sessions will reset on server restart
# For production persistence, set SECRET_KEY environment variable
app.secret_key = secrets.token_hex(32)

# ==================== SECURITY FIX: Strong Admin Credentials ====================
# ==================== MULTI-ADMIN SYSTEM ====================
# Admin accounts stored in admins.csv (auto-created on first run)
# Default accounts (5 admins with different roles)
# See CREDENTIALS.md for default usernames and passwords
# ⚠️ IMPORTANT: Change default passwords before production deployment!
# ==================== SECURITY: Password Hashing ====================

def hash_password(password):
    """Hash password using SHA-256 for secure storage"""
    return hashlib.sha256(password.encode()).hexdigest()

# ==================== DATABASE INITIALIZATION ====================

def init_db():
    """Initialize database files with correct headers"""
    if not os.path.exists('users.csv'):
        with open('users.csv', 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['username', 'password_hash'])
    
    # Initialize resolved_tickets.csv with Resolved By column
    if not os.path.exists('resolved_tickets.csv'):
        with open('resolved_tickets.csv', 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['Ticket ID', 'Customer Name', 'Customer Email', 'Product',
                           'Purchase Date', 'Issue Description', 'Priority',
                           'Queue Entry Time', 'Resolved At', 'Resolved By'])

    # ==================== MULTI-ADMIN SYSTEM ====================
    # Create admins.csv with 5 default accounts
    if not os.path.exists('admins.csv'):
        with open('admins.csv', 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['username', 'password_hash', 'full_name', 'role', 'created_at'])
            
            # 5 default admin accounts with different roles
            admins = [
                ('admin',       'Admin@DSA2025!',    'System Administrator', 'Super Admin'),
                ('manager1',    'Manager@123',       'John Manager',         'Manager'),
                ('support1',    'Support@123',       'Sarah Support',        'Support Agent'),
                ('analyst1',    'Analyst@123',       'Mike Analyst',         'Data Analyst'),
                ('supervisor1', 'Supervisor@123',    'Lisa Supervisor',      'Supervisor'),
            ]
            
            now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            for username, password, full_name, role in admins:
                pwd_hash = hashlib.sha256(password.encode()).hexdigest()
                writer.writerow([username, pwd_hash, full_name, role, now])
        
        print("✅ admins.csv created with 5 default accounts")
    
    # ==================== ACTIVITY LOG ====================
    # Create activity log CSV
    if not os.path.exists('admin_activity_log.csv'):
        with open('admin_activity_log.csv', 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['Timestamp', 'Admin Username', 'Admin Name', 'Action', 
                           'Ticket ID', 'Details', 'IP Address'])
        print("✅ admin_activity_log.csv created")

init_db()

# ==================== PRODUCT CONFIGURATION FROM FILE ====================

def load_products_config():
    """Load product configuration from JSON file"""
    # Try both uppercase and lowercase filenames for compatibility
    possible_paths = [
        os.path.join(os.path.dirname(__file__), 'PRODUCTS_CONFIG.json'),
        os.path.join(os.path.dirname(__file__), 'products_config.json')
    ]
    
    for config_path in possible_paths:
        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)
                print(f"✅ Loaded products from {os.path.basename(config_path)}")
                return config['products']
        except FileNotFoundError:
            continue
        except json.JSONDecodeError as e:
            print(f"⚠️  ERROR: Invalid JSON in {os.path.basename(config_path)}: {e}")
            return {}
    
    print("⚠️  WARNING: No products config file found. Using hardcoded products.")
    # Fallback to existing hardcoded list
    return {}

# Load products at startup
PRODUCTS_CONFIG = load_products_config()

# Extract list of valid product keys for validation
if PRODUCTS_CONFIG:
    VALID_PRODUCTS = list(PRODUCTS_CONFIG.keys())
    print(f"✅ Loaded {len(VALID_PRODUCTS)} products from products_config.json")
else:
    # Keep existing hardcoded list as fallback
    VALID_PRODUCTS = [
        'laptop', 'desktop', 'phone', 'tablet', 'monitor', 
        'keyboard', 'mouse', 'headphone', 'headphones', 'gaming pc',
        'smartwatch', 'camera', 'printer', 'speaker', 'speakers',
        'router', 'webcam', 'microphone', 'charger', 'cable',
        'hard drive', 'ssd', 'ram', 'graphics card', 'motherboard'
    ]
    print("⚠️  Using hardcoded product list")

# ==================== ADVANCED USERNAME VALIDATION ====================

"""
DESIGN DECISION: Pattern-based validation (no email verification)
Why? Demo-friendly while still professional and secure

Validation rules:
- Length: 3-20 characters
- Must contain at least one letter
- Only alphanumeric + underscore
- No 3+ repeated characters (aaa, 111)
- No keyboard walks (qwerty, asdf)
- No common test words (automatically detected)
- Must contain vowels (prevents random mashing)
"""

def is_valid_username(username):
    """Advanced pattern-based username validation"""
    
    # Length check
    if len(username) < 3 or len(username) > 20:
        return False, "Username must be 3-20 characters"
    
    # Must contain at least one letter
    if not re.search(r'[a-zA-Z]', username):
        return False, "Username must contain at least one letter"
    
    # Only alphanumeric + underscore
    if not re.match(r'^[a-zA-Z0-9_]+$', username):
        return False, "Only alphabets and underscore allowed"
    
    # Prevent 3+ identical characters in a row
    for i in range(len(username) - 2):
        if username[i] == username[i+1] == username[i+2]:
            return False, "Cannot have 3 identical characters in a row"
    
    # Prevent keyboard walks (automatic detection)
    keyboard_walks = [
        'qwerty', 'asdfgh', 'zxcvbn', '123456', 'abcdef',
        'qwertz', 'asdfg', 'yxcvb', 'mnbvc'
    ]
    lower_username = username.lower()
    for walk in keyboard_walks:
        if walk in lower_username:
            return False, "Username too predictable (keyboard pattern detected)"
    
    # Prevent common test names (automatic)
    common_test_words = [
        'test', 'admin', 'user', 'demo', 'sample', 
        'guest', 'temp', 'null', 'void', 'root'
    ]
    for word in common_test_words:
        if word in lower_username:
            return False, f"Username not allowed (contains reserved word: {word})"
    
    # Check vowel ratio (prevents random mashing like "qwrtypsd")
    if len(username) > 5:
        vowels = sum(1 for c in username.lower() if c in 'aeiou')
        if vowels < 2:
            return False, "Username must contain at least 2 vowels"
    
    return True, "Valid username"

def is_valid_password(password):
    """Password validation with security requirements"""
    
    # Minimum length
    if len(password) < 6:
        return False, "Password must be at least 6 characters"

    # Must contain at least one letter
    if not re.search(r"[A-Za-z]", password):
        return False, "Password must contain at least one letter"

    # Must contain at least one number
    if not re.search(r"\d", password):
        return False, "Password must contain at least one number"

    # No spaces allowed
    if " " in password:
        return False, "Password cannot contain spaces"

    # Check against common weak passwords
    weak_passwords = {
        "123456", "password", "qwerty",
        "admin123", "abc123", "000000",
        "letmein", "welcome", "123456789"
    }

    if password.lower() in weak_passwords:
        return False, "Password is too common and weak"

    return True, "Valid password"

# ==================== INPUT VALIDATION (CRITICAL FIX) ====================

# VALID_PRODUCTS now loaded from JSON file above (see line ~48)
# This allows easy product updates without code changes

def validate_product(product_name):
    """
    Validate product against known catalog with fuzzy matching for typos.
    Returns: (is_valid, corrected_product_name, error_message)
    """
    if not product_name or len(product_name.strip()) == 0:
        return False, None, "Product name is required"
    
    product_lower = product_name.lower().strip()
    
    # Minimum length check
    if len(product_lower) < 3:
        return False, None, "Product name too short (minimum 3 characters)"
    
    # Maximum length check
    if len(product_lower) > 50:
        return False, None, "Product name too long (maximum 50 characters)"
    
    # Exact match
    if product_lower in VALID_PRODUCTS:
        return True, product_lower, ""
    
    # Fuzzy match for common typos (Levenshtein distance)
    def levenshtein_distance(s1, s2):
        if len(s1) < len(s2):
            return levenshtein_distance(s2, s1)
        if len(s2) == 0:
            return len(s1)
        previous_row = range(len(s2) + 1)
        for i, c1 in enumerate(s1):
            current_row = [i + 1]
            for j, c2 in enumerate(s2):
                insertions = previous_row[j + 1] + 1
                deletions = current_row[j] + 1
                substitutions = previous_row[j] + (c1 != c2)
                current_row.append(min(insertions, deletions, substitutions))
            previous_row = current_row
        return previous_row[-1]
    
    # Check for close matches (1-2 character difference)
    for valid_product in VALID_PRODUCTS:
        distance = levenshtein_distance(product_lower, valid_product)
        if distance <= 2:  # Allow up to 2 typos
            return True, valid_product, f" (auto-corrected from '{product_name}')"
    
    # No match found - provide helpful error
    similar = [p for p in VALID_PRODUCTS if product_lower[:3] in p or p[:3] in product_lower]
    if similar:
        # Use display names from config if available
        if PRODUCTS_CONFIG:
            suggestions = ', '.join([PRODUCTS_CONFIG[p].get('display_name', p) for p in similar[:5]])
        else:
            suggestions = ', '.join(similar[:5])
        return False, None, f"Unknown product '{product_name}'. Did you mean: {suggestions}?"
    
    # Get example products from config
    if PRODUCTS_CONFIG:
        examples = ', '.join([PRODUCTS_CONFIG[p].get('display_name', p) for p in list(VALID_PRODUCTS)[:5]])
    else:
        examples = 'laptop, phone, tablet, monitor, keyboard'
    
    return False, None, f"Unknown product '{product_name}'. Valid products include: {examples}, etc."

def validate_purchase_date(date_string):
    """
    Validate purchase date is in valid format and reasonable range.
    Returns: (is_valid, formatted_date, error_message)
    """
    if not date_string or len(date_string.strip()) == 0:
        return False, None, "Purchase date is required"
    
    date_string = date_string.strip()
    
    # Try multiple date formats
    formats_to_try = [
        "%Y-%m-%d",    # 2024-12-31
        "%d-%m-%Y",    # 31-12-2024
        "%m-%d-%Y",    # 12-31-2024
        "%Y/%m/%d",    # 2024/12/31
        "%d/%m/%Y",    # 31/12/2024
        "%m/%d/%Y",    # 12/31/2024
    ]
    
    parsed_date = None
    for date_format in formats_to_try:
        try:
            parsed_date = datetime.strptime(date_string, date_format)
            break
        except ValueError:
            continue
    
    if parsed_date is None:
        return False, None, "Invalid date format. Please use YYYY-MM-DD (e.g., 2024-12-31)"
    
    # Validate year is reasonable
    current_year = datetime.now().year
    if parsed_date.year < 1990:
        return False, None, f"Purchase date too old (year {parsed_date.year}). Products must be purchased after 1990."
    
    if parsed_date.year > current_year:
        return False, None, f"Purchase date cannot be in the future (year {parsed_date.year})"
    
    # Check if date is not more than 10 years old
    ten_years_ago = datetime.now() - timedelta(days=365*10)
    if parsed_date < ten_years_ago:
        return False, None, f"Purchase date too old ({parsed_date.strftime('%Y-%m-%d')}). We only support products purchased within the last 10 years."
    
    # Check if date is not in the future
    if parsed_date > datetime.now():
        return False, None, f"Purchase date cannot be in the future ({parsed_date.strftime('%Y-%m-%d')})"
    
    # Return standardized format (YYYY-MM-DD)
    return True, parsed_date.strftime("%Y-%m-%d"), ""

def validate_issue_description(description, product=None):
    """
    Validate issue description is meaningful and not spam.
    If product is provided, check semantic relevance.
    Returns: (is_valid, error_message)
    """
    if not description or len(description.strip()) == 0:
        return False, "Issue description is required"
    
    description = description.strip()
    
    # Minimum length
    if len(description) < 10:
        return False, "Description too short (minimum 10 characters). Please provide details about your issue."
    
    # Maximum length
    if len(description) > 500:
        return False, "Description too long (maximum 500 characters). Please be concise."
    
    # Must contain at least 3 words (spaces)
    word_count = len(description.split())
    if word_count < 3:
        return False, "Description too brief. Please use at least 3 words to describe your issue."
    
    # Must contain vowels (prevents random mashing like "asdfgh")
    vowel_count = sum(1 for c in description.lower() if c in 'aeiou')
    if vowel_count < 3:
        return False, "Description appears invalid. Please write a proper description of your issue."
    
    # Check if description contains reasonable English patterns
    # A real sentence should have vowels distributed throughout, not clustered
    words = description.split()
    suspicious_word_count = 0
    for word in words:
        word_lower = word.lower().strip('.,!?')
        if len(word_lower) >= 4:  # Only check words 4+ chars
            # Check for excessive consonants clusters (like "jhghbgjmhtb")
            consonant_clusters = 0
            for i in range(len(word_lower) - 2):
                if (word_lower[i] not in 'aeiou' and 
                    word_lower[i+1] not in 'aeiou' and 
                    word_lower[i+2] not in 'aeiou'):
                    consonant_clusters += 1
            
            # If word has many consonant clusters, it's suspicious
            if consonant_clusters > len(word_lower) / 3:
                suspicious_word_count += 1
    
    # If more than half the words are suspicious, reject
    if len(words) >= 2 and suspicious_word_count > len(words) / 2:
        return False, "Description contains gibberish. Please write clearly about your actual issue."
    
    # Check for excessive special characters (spam indicator)
    special_chars = sum(1 for c in description if not c.isalnum() and c not in ' .,!?\'-')
    if special_chars > len(description) * 0.3:  # More than 30% special chars
        return False, "Description contains too many special characters. Please write clearly."
    
    # Check for excessive repeated characters (spam indicator)
    max_repeats = 0
    current_repeats = 1
    for i in range(1, len(description)):
        if description[i] == description[i-1]:
            current_repeats += 1
            max_repeats = max(max_repeats, current_repeats)
        else:
            current_repeats = 1
    
    if max_repeats > 5:
        return False, "Description contains excessive repeated characters. Please write naturally."
    
    # SEMANTIC VALIDATION: Check if description relates to product
    if product:
        product_lower = product.lower()
        description_lower = description.lower()
        
        # Get keywords from JSON config
        if PRODUCTS_CONFIG and product in PRODUCTS_CONFIG:
            keywords = PRODUCTS_CONFIG[product].get('keywords', [])
        else:
            # Fallback to basic keywords if config not available
            keywords = ['broken', 'not working', 'issue', 'problem']
        
        # Check if ANY keyword appears in description
        has_relevant_keyword = any(keyword in description_lower for keyword in keywords)
        
        # Also check if product name itself appears in description
        product_mentioned = product_lower in description_lower
        
        if not has_relevant_keyword and not product_mentioned:
            # Description doesn't seem related to the product
            # Get display name from config
            if PRODUCTS_CONFIG and product in PRODUCTS_CONFIG:
                display_name = PRODUCTS_CONFIG[product].get('display_name', product)
            else:
                display_name = product
            
            keyword_examples = ', '.join(keywords[:5]) if len(keywords) >= 5 else ', '.join(keywords)
            return False, f"Description doesn't seem related to {display_name}. Please describe the actual issue with your {display_name} (e.g., mention: {keyword_examples}, etc.)"    
    return True, ""

def validate_name(name):
    """Validate customer name"""
    if not name or len(name.strip()) == 0:
        return False, "Name is required"
    
    name = name.strip()
    
    if len(name) < 2:
        return False, "Name too short (minimum 2 characters)"
    
    if len(name) > 100:
        return False, "Name too long (maximum 100 characters)"
    
    # Must contain at least one letter
    if not re.search(r'[a-zA-Z]', name):
        return False, "Name must contain at least one letter"
    
    return True, ""

def validate_email(email):
    """Validate email format and check for suspicious patterns"""
    if not email or len(email.strip()) == 0:
        return False, "Email is required"
    
    email = email.strip().lower()
    
    # Basic email regex
    email_pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
    if not re.match(email_pattern, email):
        return False, "Invalid email format (e.g., user@example.com)"
    
    if len(email) > 100:
        return False, "Email too long (maximum 100 characters)"
    
    # Extract username and domain
    try:
        username, domain = email.split('@')
    except ValueError:
        return False, "Invalid email format"
    
    # Check username quality (prevent gibberish like "hnvhb")
    # Must have at least 2 vowels (prevents random mashing)
    vowel_count = sum(1 for c in username if c in 'aeiou')
    if len(username) >= 5 and vowel_count < 2:
        return False, "Email username appears invalid (please use a real email address)"
    
    # Check for suspicious patterns (all consonants, repeated chars)
    if len(username) >= 4:
        # Check for excessive repeated characters
        max_repeats = 0
        current_repeats = 1
        for i in range(1, len(username)):
            if username[i] == username[i-1]:
                current_repeats += 1
                max_repeats = max(max_repeats, current_repeats)
            else:
                current_repeats = 1
        
        if max_repeats >= 3:
            return False, "Email appears invalid (repeated characters)"
    
    # Check domain has at least one vowel (prevents "gcgchg.com")
    domain_name = domain.split('.')[0]
    domain_vowels = sum(1 for c in domain_name if c in 'aeiou')
    if len(domain_name) >= 4 and domain_vowels == 0:
        return False, "Email domain appears invalid (please use a real email service)"
    
    # Warn about common test/fake email patterns
    suspicious_patterns = ['test', 'fake', 'dummy', 'spam', 'temp', 'throwaway']
    for pattern in suspicious_patterns:
        if pattern in username or pattern in domain:
            return False, f"Email appears to be a test/fake address. Please use your real email."
    
    return True, ""

# ==================== USER MANAGEMENT (WITH SECURITY) ====================

def check_user(username, password):
    """Check if user credentials are valid using hashed password comparison"""
    if not os.path.exists('users.csv'): 
        return False
    
    password_hash = hash_password(password)
    
    with open('users.csv', 'r') as f:
        reader = csv.reader(f)
        next(reader, None)  # Skip header
        for row in reader:
            if row and len(row) >= 2:
                if row[0] == username and row[1] == password_hash:
                    return True
    return False

def register_user(username, password):
    """Register new user with hashed password"""
    # Check if username already exists
    if os.path.exists('users.csv'):
        with open('users.csv', 'r') as f:
            reader = csv.reader(f)
            next(reader, None)
            for row in reader:
                if row and row[0] == username:
                    return False
    
    # Hash password before storing
    password_hash = hash_password(password)
    
    with open('users.csv', 'a', newline='') as f:
        csv.writer(f).writerow([username, password_hash])
    return True

# ==================== TICKET ID GENERATION ====================

def get_next_ticket_id():
    """Generate next available ticket ID by scanning all CSV files"""
    max_id = 1000 

    # Check active tickets
    if os.path.exists('customer_support_tickets_updated.csv'):
        with open('customer_support_tickets_updated.csv', 'r') as f:
            reader = csv.reader(f)
            next(reader, None) 
            for row in reader:
                if row:
                    try:
                        curr_id = int(row[0])
                        if curr_id > max_id: 
                            max_id = curr_id
                    except: 
                        pass
    
    # Check pending tickets
    if os.path.exists('pending_tickets.csv'):
        with open('pending_tickets.csv', 'r') as f:
            reader = csv.reader(f)
            for row in reader:
                if row:
                    try:
                        curr_id = int(row[0])
                        if curr_id > max_id: 
                            max_id = curr_id
                    except: 
                        pass
    
    # Check resolved tickets
    if os.path.exists('resolved_tickets.csv'):
        with open('resolved_tickets.csv', 'r') as f:
            reader = csv.reader(f)
            next(reader, None)
            for row in reader:
                if row:
                    try:
                        curr_id = int(row[0])
                        if curr_id > max_id: 
                            max_id = curr_id
                    except: 
                        pass
                    
    return max_id + 1

# ==================== DUPLICATE DETECTION ====================

def check_duplicate_ticket(email, description):
    """
    Smart duplicate detection:
    - Returns (True, ticket_id) if duplicate exists in open tickets
    - Returns (False, None) if no duplicate
    
    Checks: Same email + similar issue (first 30 chars match)
    """
    
    if not os.path.exists('customer_support_tickets_updated.csv'):
        return False, None
    
    desc_prefix = description[:30].lower().strip()
    
    with open('customer_support_tickets_updated.csv', 'r') as f:
        reader = csv.reader(f)
        next(reader, None)  # Skip header
        
        for row in reader:
            if len(row) < 6:
                continue
            
            csv_email = row[2].strip().lower()
            csv_desc = row[5].strip().lower()[:30]
            
            if csv_email == email.lower() and csv_desc == desc_prefix:
                return True, row[0]  # Duplicate found
    
    return False, None

# ==================== MULTI-ADMIN HELPERS ====================

def check_admin(username, password):
    """
    Verify admin credentials against admins.csv
    Returns: (is_valid, full_name, role) on success, (False, None, None) on failure
    """
    if not os.path.exists('admins.csv'):
        return False, None, None
    
    pwd_hash = hashlib.sha256(password.encode()).hexdigest()
    
    with open('admins.csv', 'r') as f:
        reader = csv.reader(f)
        next(reader, None)  # skip header
        for row in reader:
            if len(row) >= 4 and row[0] == username and row[1] == pwd_hash:
                return True, row[2], row[3]  # full_name, role
    
    return False, None, None

def log_admin_activity(action, ticket_id='', details=''):
    """
    Log admin activity to admin_activity_log.csv only
    FIXED: Removed duplicate TXT logging to prevent redundant entries
    Reads admin info from current Flask session
    """
    try:
        admin_username = session.get('admin_username', 'unknown')
        admin_name = session.get('admin_name', 'unknown')
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        ip_address = request.remote_addr or 'N/A'
        
        # Write to CSV for structured data (single source of truth)
        with open('admin_activity_log.csv', 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([timestamp, admin_username, admin_name, action, 
                           ticket_id, details, ip_address])
    
    except Exception as e:
        print(f"⚠️  Activity log error: {e}")

# ==================== ROUTES ====================

@app.route('/')
def landing(): 
    return render_template('index.html')

@app.route('/login')
def login_page(): 
    return render_template('login.html')

@app.route('/signup')
def signup_page(): 
    return render_template('signup.html')

@app.route('/do_signup', methods=['POST'])
def do_signup():
    """Handle user registration with advanced validation and password hashing"""
    username = request.form['username']
    password = request.form['password']

    # Validate username
    is_valid, message = is_valid_username(username)
    if not is_valid:
        flash(f"Invalid username: {message}", "error")
        return redirect(url_for('signup_page'))

    # Validate password
    is_valid, message = is_valid_password(password)
    if not is_valid:
        flash(f"Invalid password: {message}", "error")
        return redirect(url_for('signup_page'))

    # Register user (password will be hashed)
    if register_user(username, password):
        flash("✅ Account created successfully! Please login.", "success")
        return redirect(url_for('login_page'))
    else:
        flash("❌ Username already exists!", "error")
        return redirect(url_for('signup_page'))

@app.route('/auth', methods=['POST'])
def auth():
    """Handle user authentication with secure password verification"""
    username = request.form['username']
    password = request.form['password']
    role = request.form.get('role', 'user')

    if role == 'admin':
        # MULTI-ADMIN: Check credentials from admins.csv
        is_valid, full_name, admin_role = check_admin(username, password)
        if is_valid:
            # Create admin session
            session['is_admin'] = True
            session['admin_username'] = username
            session['admin_name'] = full_name
            session['admin_role'] = admin_role
            
            # Log the login
            log_admin_activity('LOGIN', details=f'{full_name} logged in as {admin_role}')
            
            return redirect(url_for('admin_dashboard'))
        else:
            flash("Invalid Admin credentials!", "error")
            # FIXED: Redirect back to admin login, not customer login
            return redirect(url_for('login_page') + '?role=admin')
    else:
        # Validate input format before checking credentials
        is_valid, message = is_valid_username(username)
        if not is_valid:
            flash("Invalid username format", "error")
            return redirect(url_for('login_page'))

        is_valid, message = is_valid_password(password)
        if not is_valid:
            flash("Invalid password format", "error")
            return redirect(url_for('login_page'))

        # Check credentials (uses hashed password comparison)
        if check_user(username, password):
            return redirect(url_for('user_homepage'))
        else:
            flash("Invalid username or password!", "error")
            return redirect(url_for('login_page'))

@app.route('/home')
def user_homepage(): 
    return render_template('homepage.html')

@app.route('/index1')
def ticket_form(): 
    return render_template('index1.html')

@app.route('/submit_ticket', methods=['POST'])
def submit_ticket():
    """
    Handle ticket submission with comprehensive validation, duplicate detection, and XSS protection
    Flow: Validate inputs → Check for duplicates → Generate ID → Save to pending_tickets.csv
    """
    # SECURITY FIX: Sanitize all user inputs to prevent XSS attacks
    name = html_lib.escape(request.form.get('name', '').strip())
    email = html_lib.escape(request.form.get('email', '').strip())
    product = html_lib.escape(request.form.get('product', '').strip())
    purchase_date = html_lib.escape(request.form.get('dop', '').strip())
    description = html_lib.escape(request.form.get('description', '').strip())
    
    # ==================== VALIDATION (CRITICAL FIX) ====================
    
    # Validate name
    name_valid, name_error = validate_name(name)
    if not name_valid:
        return render_template('index1.html', error=f"❌ {name_error}")
    
    # Validate email
    email_valid, email_error = validate_email(email)
    if not email_valid:
        return render_template('index1.html', error=f"❌ {email_error}")
    
    # Validate product (with auto-correction)
    product_valid, corrected_product, product_error = validate_product(product)
    if not product_valid:
        return render_template('index1.html', error=f"❌ {product_error}")
    
    # Use corrected product name
    product = corrected_product
    product_correction_note = ""
    if "auto-corrected" in str(product_error):
        product_correction_note = product_error
    
    # Validate purchase date (with format standardization)
    date_valid, formatted_date, date_error = validate_purchase_date(purchase_date)
    if not date_valid:
        return render_template('index1.html', error=f"❌ {date_error}")
    
    # Use standardized date format
    purchase_date = formatted_date
    
    # Validate issue description (WITH semantic check against product)
    desc_valid, desc_error = validate_issue_description(description, product)
    if not desc_valid:
        return render_template('index1.html', error=f"❌ {desc_error}")
    
    # ==================== END VALIDATION ====================
    
    # CRITICAL: Replace commas with semicolons to prevent CSV parsing issues
    description = description.replace(',', ';')
    
    # DUPLICATE DETECTION
    is_duplicate, existing_ticket_id = check_duplicate_ticket(email, description)
    
    if is_duplicate:
        return render_template('homepage.html',
            message=f"⚠️ You already have an open ticket for this issue (#{existing_ticket_id}). "
                   f"Please wait for resolution or check ticket status.",
            new_id=existing_ticket_id)
    
    # Not a duplicate - create new ticket
    new_ticket_id = get_next_ticket_id()

    # Save to pending_tickets.csv (staging area)
    with open('pending_tickets.csv', 'a', newline='', encoding='utf-8') as f:
        writer = csv.writer(f, quoting=csv.QUOTE_MINIMAL)
        writer.writerow([new_ticket_id, name, email, product, purchase_date, description])
    
    # Calculate queue position for user feedback
    queue_size = 0
    if os.path.exists('customer_support_tickets_updated.csv'):
        with open('customer_support_tickets_updated.csv', 'r') as f:
            queue_size = sum(1 for line in f) - 1  # Subtract header
    
    # Add pending tickets count
    if os.path.exists('pending_tickets.csv'):
        with open('pending_tickets.csv', 'r') as f:
            queue_size += sum(1 for line in f)
    
    # Generate user feedback message
    position_msg = f"✅ Ticket #{new_ticket_id} created successfully!{product_correction_note}"
    if queue_size > 1:
        position_msg += f" You are approximately #{queue_size} in the queue."
    elif queue_size == 1:
        position_msg += " Your ticket will be processed next!"
    else:
        position_msg += " Your ticket will be processed shortly!"

    return render_template('homepage.html', 
                           message=position_msg, 
                           new_id=new_ticket_id)

@app.route('/status.html')
def status_page(): 
    return render_template('status.html')

@app.route('/check_status', methods=['POST'])
def check_status():
    """
    Check ticket status with email verification for security
    Prevents unauthorized users from viewing other people's tickets
    """
    # SECURITY FIX: Sanitize inputs
    ticket_id = html_lib.escape(request.form.get('ticket_id', '').strip())
    email_input = html_lib.escape(request.form.get('email', '').strip().lower())
    
    found_status = None
    found_customer = None
    found_issue = None
    found_dop = None          
    found_resolve_time = None
    found_priority = None
    found_product = None
    wait_time = None
    queue_position = None
    error_msg = None

    def search_csv(filename, db_type):
        """Search CSV file for ticket with email verification"""
        if not os.path.exists(filename): 
            return None
            
        with open(filename, 'r', encoding='utf-8') as f:
            reader = csv.reader(f)
            header = next(reader, None)  # Skip header
            
            for row in reader:
                if len(row) < 6: 
                    continue 
                
                # CSV reader automatically handles quotes, just strip whitespace
                csv_id = row[0].strip()
                csv_email = row[2].strip().lower()

                if csv_id == ticket_id:
                    if csv_email == email_input:
                        return row
                    else:
                        return "UNAUTHORIZED"
        return None

    # Search in active tickets first
    result = search_csv('customer_support_tickets_updated.csv', 'active')
    
    if result == "UNAUTHORIZED":
        error_msg = "🔒 Security Error: Ticket ID exists but Email does not match!"
    elif result:
        found_status = "Open"
        found_customer = result[1].strip()
        found_product = result[3].strip()
        found_dop = result[4].strip()
        found_issue = result[5].strip()
        found_priority = result[6].strip() if len(result) > 6 else "N/A"
        
        # Calculate queue position
        position = 1
        with open('customer_support_tickets_updated.csv', 'r') as f:
            reader = csv.reader(f)
            next(reader, None)
            for row in reader:
                if row and row[0].strip() == ticket_id:
                    break
                position += 1
        queue_position = position
        
        # Calculate wait time
        if len(result) > 7 and result[7]:
            try:
                entry_time = int(result[7].strip())
                current_time = int(time.time())
                wait_hours = (current_time - entry_time) / 3600.0
                if wait_hours < 1:
                    wait_time = f"{wait_hours * 60:.0f} minutes"
                else:
                    wait_time = f"{wait_hours:.1f} hours"
            except:
                wait_time = "Calculating..."

    # If not in active, search resolved
    if not found_status and not error_msg:
        result = search_csv('resolved_tickets.csv', 'resolved')
        if result == "UNAUTHORIZED":
            error_msg = "🔒 Security Error: Ticket ID exists but Email does not match!"
        elif result:
            found_status = "Resolved"
            found_customer = result[1].strip()
            found_product = result[3].strip()
            found_dop = result[4].strip()
            found_issue = result[5].strip()
            found_priority = result[6].strip() if len(result) > 6 else "N/A"
            found_resolve_time = result[8].strip() if len(result) > 8 else "N/A"

    if not found_status and not error_msg:
        error_msg = "❌ Ticket ID not found in our database."

    return render_template('status.html', 
                           ticket_id=ticket_id if not error_msg else None, 
                           status=found_status, 
                           customer=found_customer, 
                           product=found_product,
                           dop=found_dop, 
                           issue=found_issue, 
                           priority=found_priority,
                           wait_time=wait_time,
                           queue_position=queue_position,
                           resolve_time=found_resolve_time, 
                           error=error_msg)

@app.route('/admin')
def admin_dashboard():
    """Display admin dashboard - requires active admin session"""
    # Session guard
    if not session.get('is_admin'):
        flash("Please log in as admin first.", "error")
        return redirect(url_for('login_page'))
    
    # FIXED: Don't log every dashboard access - too verbose
    # Only log on initial login (already logged in auth())
    
    if os.path.exists('templates/admin_view.html'): 
        return render_template('admin_view.html',
                             admin_name=session.get('admin_name', 'Admin'),
                             admin_role=session.get('admin_role', ''))
    return "<h3>Dashboard loading... refresh shortly.</h3>"

@app.route('/admin_logout')
def admin_logout():
    """Logout admin and clear session"""
    log_admin_activity('LOGOUT', details='Admin logged out')
    
    # FIXED: Clear all session data properly
    session.pop('is_admin', None)
    session.pop('admin_username', None)
    session.pop('admin_name', None)
    session.pop('admin_role', None)
    
    # Clear any remaining session data
    session.clear()
    
    flash("Logged out successfully.", "success")
    return redirect(url_for('login_page'))

@app.route('/resolve/<int:ticket_id>')
def resolve_ticket(ticket_id):
    """Mark ticket as resolved - requires active admin session"""
    # Session guard
    if not session.get('is_admin'):
        flash("Unauthorized.", "error")
        return redirect(url_for('login_page'))
    
    # FIXED: Enhanced duplicate prevention with timestamp
    resolved_key = f'resolved_{ticket_id}'
    resolved_time_key = f'resolved_time_{ticket_id}'
    
    current_time = time.time()
    last_resolved_time = session.get(resolved_time_key, 0)
    
    # If resolved within last 60 seconds, skip (prevent duplicates)
    if session.get(resolved_key) and (current_time - last_resolved_time) < 60:
        # Already resolved recently, just redirect silently
        return redirect(url_for('admin_dashboard'))
    
    admin_username = session.get('admin_username', 'admin')
    
    # Write command with admin username for tracking
    with open('admin_commands.txt', 'w') as f: 
        f.write(f"RESOLVE {ticket_id} {admin_username}")
    
    # Mark as resolved in session with timestamp
    session[resolved_key] = True
    session[resolved_time_key] = current_time
    
    # Log the resolution
    log_admin_activity('RESOLVE_TICKET', ticket_id=ticket_id, 
                      details=f'Ticket #{ticket_id} resolved')
    
    time.sleep(0.5)
    
    # Force browser to reload (prevent cache)
    from flask import make_response
    response = make_response(redirect(url_for('admin_dashboard')))
    response.headers['Cache-Control'] = 'no-cache, no-store, must-revalidate, max-age=0'
    response.headers['Pragma'] = 'no-cache'
    response.headers['Expires'] = '0'
    return response

@app.route('/update_priority/<int:ticket_id>/<priority>', methods=['POST'])
def update_priority(ticket_id, priority):
    """Update ticket priority - requires active admin session"""
    # Session guard
    if not session.get('is_admin'):
        return jsonify({'success': False, 'error': 'Unauthorized'})
    
    valid_priorities = ['Low', 'Medium', 'High', 'Critical']
    
    if priority not in valid_priorities:
        return jsonify({'success': False, 'error': 'Invalid priority'})
    
    # Update in active queue CSV
    try:
        rows = []
        found = False
        old_priority = 'Unknown'
        
        if os.path.exists('customer_support_tickets_updated.csv'):
            with open('customer_support_tickets_updated.csv', 'r') as f:
                reader = csv.reader(f)
                header = next(reader)
                rows.append(header)
                
                for row in reader:
                    if len(row) > 0 and row[0].strip() == str(ticket_id):
                        old_priority = row[6] if len(row) > 6 else 'Unknown'
                        row[6] = priority  # Priority is column 6
                        found = True
                    rows.append(row)
            
            # Write back
            if found:
                with open('customer_support_tickets_updated.csv', 'w', newline='') as f:
                    writer = csv.writer(f)
                    writer.writerows(rows)
                
                # Log the priority change
                log_admin_activity('CHANGE_PRIORITY', ticket_id=ticket_id,
                                 details=f'Priority changed: {old_priority} → {priority}')
                
                return jsonify({'success': True, 'message': f'Priority updated to {priority}'})
        
        return jsonify({'success': False, 'error': 'Ticket not found'})
    
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)})

@app.route('/activity_log')
def view_activity_log():
    """View admin activity log - requires active admin session"""
    # Session guard
    if not session.get('is_admin'):
        flash("Please log in as admin first.", "error")
        return redirect(url_for('login_page'))
    
    activities = []
    if os.path.exists('admin_activity_log.csv'):
        with open('admin_activity_log.csv', 'r') as f:
            reader = csv.reader(f)
            next(reader, None)  # skip header
            for row in reader:
                if len(row) >= 6:
                    activities.append({
                        'timestamp': row[0],
                        'admin_username': row[1],
                        'admin_name': row[2],
                        'action_type': row[3],
                        'ticket_id': row[4],
                        'details': row[5],
                        'ip_address': row[6] if len(row) > 6 else 'N/A'
                    })
    
    activities.reverse()  # Newest first
    
    return render_template('activity_log.html',
                         activities=activities,
                         admin_name=session.get('admin_name', 'Admin'))

@app.route('/api/products')
def get_products():
    """
    API endpoint to provide product configuration to JavaScript.
    This allows the frontend to have the same product data as backend.
    """
    from flask import jsonify
    
    if PRODUCTS_CONFIG:
        return jsonify(PRODUCTS_CONFIG)
    else:
        # Return basic structure if config not loaded
        return jsonify({
            'laptop': {'hint': 'Laptop issue', 'keywords': ['screen', 'battery']},
            'phone': {'hint': 'Phone issue', 'keywords': ['screen', 'camera']}
        })
    
if __name__ == '__main__':
    print("\n" + "="*70)
    print("🔒 SECURITY INFORMATION - DSA Ticketing System")
    print("="*70)
    print("✅ All security fixes applied:")
    print("   • Password hashing enabled (SHA-256)")
    print("   • XSS protection active (HTML escaping)")
    print("   • Random secret key generated")
    print("   • Multi-admin authentication + session management")
    print("   • Complete activity logging system")
    print()
    print("📋 Admin Accounts (5 admins in admins.csv):")
    print("   admin       / Admin@DSA2025!    — Super Admin")
    print("   manager1    / Manager@123       — Manager")
    print("   support1    / Support@123       — Support Agent")
    print("   analyst1    / Analyst@123       — Data Analyst")
    print("   supervisor1 / Supervisor@123    — Supervisor")
    print()
    print("📊 Activity Logging:")
    print("   • All admin actions logged to admin_activity_log.csv")
    print("   • Human-readable log: admin_activity_log.txt")
    print("   • View logs at: http://localhost:5000/activity_log")
    print()
    print("⚠️  Change passwords in admins.csv before production!")
    print("="*70)
    print()
    print("🚀 Starting Flask server on http://localhost:5000")
    print("   Press Ctrl+C to stop")
    print()
    
    app.run(debug=True, port=5000, use_reloader=False)