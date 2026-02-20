@echo off
REM Clean Old Logs Script (Windows)
REM Run this after installing the fixed version to start fresh

echo 🧹 Cleaning up old log files...

REM Backup old logs (in case you need them)
if exist admin_activity_log.csv (
    ren admin_activity_log.csv admin_activity_log_OLD.csv
    echo ✅ Backed up old admin_activity_log.csv
)

if exist admin_activity_log.txt (
    ren admin_activity_log.txt admin_activity_log_OLD.txt
    echo ✅ Backed up old admin_activity_log.txt
)

if exist escalation_log.txt (
    ren escalation_log.txt escalation_log_OLD.txt
    echo ✅ Backed up old escalation_log.txt
)

REM Create fresh CSV log with header
echo Timestamp,Admin Username,Admin Name,Action,Ticket ID,Details,IP Address > admin_activity_log.csv
echo ✅ Created fresh admin_activity_log.csv

echo.
echo 🎉 Done! Old logs backed up, fresh logs created.
echo Now restart your server: python server.py
pause
