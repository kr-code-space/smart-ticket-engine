// ==================== PRODUCT CONFIGURATION ====================
// This will be loaded from the server via a new endpoint

let productData = {};

// Load product configuration from server
async function loadProductConfig() {
    try {
        const response = await fetch('/api/products');
        if (response.ok) {
            productData = await response.json();
            console.log('✅ Product configuration loaded:', Object.keys(productData).length, 'products');
        }
    } catch (error) {
        console.warn('⚠️  Could not load product config, using fallback');
        // Fallback to basic config
        productData = {
            'laptop': {
                hint: 'Tip: Mention what\'s wrong (e.g., screen, battery, keyboard)',
                keywords: ['screen', 'display', 'keyboard', 'battery', 'broken', 'not working']
            },
            'phone': {
                hint: 'Tip: What\'s the problem? (e.g., screen, battery, camera)',
                keywords: ['screen', 'battery', 'camera', 'broken', 'not working']
            }
        };
    }
}

// ==================== FORM VALIDATION ====================

function showProductHint(product) {
    const hintDiv = document.getElementById('product-hint');
    
    if (hintDiv && productData[product]) {
        hintDiv.textContent = productData[product].hint || '';
        hintDiv.style.display = 'block';
    } else if (hintDiv) {
        hintDiv.style.display = 'none';
    }
    
    validateDescriptionRealtime();
}

function validateDescriptionRealtime() {
    const descriptionField = document.getElementById('description');
    const productField = document.getElementById('product-select');
    const feedbackDiv = document.getElementById('description-feedback');
    
    if (!descriptionField || !productField || !feedbackDiv) {
        return; // Elements not found
    }
    
    const description = descriptionField.value.trim().toLowerCase();
    const product = productField.value;
    
    if (!product || description.length < 10) {
        feedbackDiv.style.display = 'none';
        return;
    }
    
    const hasProductName = description.includes(product.toLowerCase());
    const data = productData[product];
    let hasRelevantKeyword = false;
    
    if (data && data.keywords) {
        hasRelevantKeyword = data.keywords.some(keyword => 
            description.includes(keyword.toLowerCase())
        );
    }
    
    feedbackDiv.style.display = 'block';
    
    if (hasProductName || hasRelevantKeyword) {
        feedbackDiv.textContent = '✅ Good description!';
        feedbackDiv.className = 'description-feedback feedback-valid';
    } else {
        feedbackDiv.textContent = `⚠️ Please mention the ${product} or describe the specific issue`;
        feedbackDiv.className = 'description-feedback feedback-warning';
    }
}

function validateEmail(email) {
    const username = email.split('@')[0];
    
    // Check for suspicious patterns (no vowels in username)
    if (username.length >= 5) {
        const vowelCount = (username.match(/[aeiou]/gi) || []).length;
        if (vowelCount < 2) {
            return 'Email username appears unusual. Please use a real email address.';
        }
    }
    
    // Check for test/fake patterns
    const suspiciousPatterns = ['test', 'fake', 'dummy', 'spam'];
    for (let pattern of suspiciousPatterns) {
        if (email.toLowerCase().includes(pattern)) {
            return 'Please use your real email address, not a test/fake email.';
        }
    }
    
    return null; // Valid
}

function validateForm() {
    const form = document.querySelector('form');
    if (!form) return true; // No form found, allow submission
    
    const descriptionField = document.getElementById('description');
    const productField = document.getElementById('product-select');
    const emailField = document.querySelector('input[name="email"]');
    
    if (!descriptionField || !productField || !emailField) {
        return true; // Required fields not found, let server validate
    }
    
    const description = descriptionField.value.trim().toLowerCase();
    const product = productField.value;
    const email = emailField.value.toLowerCase();
    
    // Email validation
    const emailError = validateEmail(email);
    if (emailError) {
        alert('❌ ' + emailError);
        emailField.focus();
        return false;
    }
    
    // Product validation
    if (!product) {
        alert('❌ Please select a product');
        productField.focus();
        return false;
    }
    
    // Word count validation
    const wordCount = description.split(/\s+/).filter(w => w.length > 0).length;
    if (wordCount < 3) {
        alert('❌ Please provide more details (at least 3 words)');
        descriptionField.focus();
        return false;
    }
    
    // Vowel check (prevents gibberish)
    const vowelCount = (description.match(/[aeiou]/g) || []).length;
    if (vowelCount < 3) {
        alert('❌ Please write a proper description');
        descriptionField.focus();
        return false;
    }
    
    // Product relevance check
    const hasProductName = description.includes(product.toLowerCase());
    const data = productData[product];
    let hasRelevantKeyword = false;
    
    if (data && data.keywords) {
        hasRelevantKeyword = data.keywords.some(keyword => 
            description.includes(keyword.toLowerCase())
        );
    }
    
    if (!hasProductName && !hasRelevantKeyword) {
        const exampleKeywords = data && data.keywords ? data.keywords.slice(0, 3).join(', ') : 'the issue';
        alert(`❌ Please describe the issue with your ${product}.\n\nMention the product name or what's wrong (e.g., ${exampleKeywords})`);
        descriptionField.focus();
        return false;
    }
    
    return true; // All validations passed
}

// ==================== DATE CONSTRAINTS ====================

function setupDateConstraints() {
    const dateInput = document.querySelector('input[name="dop"]');
    if (!dateInput) return;
    
    // Set today as max
    const today = new Date().toISOString().split('T')[0];
    dateInput.setAttribute('max', today);
    
    // Set minimum date (10 years ago)
    const tenYearsAgo = new Date();
    tenYearsAgo.setFullYear(tenYearsAgo.getFullYear() - 10);
    const minDate = tenYearsAgo.toISOString().split('T')[0];
    dateInput.setAttribute('min', minDate);
}

// ==================== INITIALIZATION ====================

document.addEventListener("DOMContentLoaded", async function() {
    console.log('🚀 Initializing ticket form...');
    
    // Load product configuration
    await loadProductConfig();
    
    // Setup date constraints
    setupDateConstraints();
    
    // Attach form submit handler
    const form = document.querySelector("form");
    if (form) {
        form.addEventListener("submit", function(event) {
            if (!validateForm()) {
                event.preventDefault(); // Stop form submission
            }
        });
        console.log('✅ Form validation attached');
    }
    
    // Setup real-time description validation
    const descriptionField = document.getElementById('description');
    if (descriptionField) {
        descriptionField.addEventListener('input', validateDescriptionRealtime);
    }
    
    console.log('✅ Form initialized successfully');
});