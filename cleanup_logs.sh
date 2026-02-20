#!/bin/bash
# Clean Old Logs Script
# Run this after installing the fixed version to start fresh

echo "🧹 Cleaning up old log files..."

# Backup old logs (in case you need them)
if [ -f "admin_activity_log.csv" ]; then
    mv admin_activity_log.csv admin_activity_log_OLD_$(date +%Y%m%d_%H%M%S).csv
    echo "✅ Backed up old admin_activity_log.csv"
fi

if [ -f "admin_activity_log.txt" ]; then
    mv admin_activity_log.txt admin_activity_log_OLD_$(date +%Y%m%d_%H%M%S).txt
    echo "✅ Backed up old admin_activity_log.txt"
fi

if [ -f "escalation_log.txt" ]; then
    mv escalation_log.txt escalation_log_OLD_$(date +%Y%m%d_%H%M%S).txt
    echo "✅ Backed up old escalation_log.txt"
fi

# Create fresh CSV log with header
echo "Timestamp,Admin Username,Admin Name,Action,Ticket ID,Details,IP Address" > admin_activity_log.csv
echo "✅ Created fresh admin_activity_log.csv"

echo ""
echo "🎉 Done! Old logs backed up, fresh logs created."
echo "Now restart your server: python server.py"
