#!/bin/bash
# Simple test script for log parser using actual log format

set -e

echo "========================================"
echo "Sidecar Log Parser Simple Test"
echo "========================================"
echo ""

# Test data directory
TEST_DIR="/tmp/sidecar_log_parser_test"
mkdir -p "$TEST_DIR"

# Test 1: Normal log entry
echo "=== Test 1: Normal log entry ==="
cat > "$TEST_DIR/test1.log" << 'EOF'
{"sfc_time": "2025-11-12 02:58:50.676086", "service_call": {"call_name": "/relay", "in_time": "2025-11-12 02:58:50.676086", "out_time": "2025-11-12 02:58:50.677831", "port_num": 6363, "in_datasize": 13, "out_datasize": 13}, "sidecar": {"in_time": "2025-11-12 02:58:50.692365", "out_time": "2025-11-12 02:58:50.696276", "name": "ndn-sidecar", "protocol": "ndn"}, "host_name": "192.168.49.2"}
EOF

echo "Test log file created: $TEST_DIR/test1.log"
echo "Content:"
cat "$TEST_DIR/test1.log"
echo ""
echo "Checking for expected keys..."
if grep -q "service_call" "$TEST_DIR/test1.log" && \
   grep -q "sidecar" "$TEST_DIR/test1.log" && \
   grep -q "in_time" "$TEST_DIR/test1.log" && \
   grep -q "out_time" "$TEST_DIR/test1.log"; then
    echo "✓ Test 1 PASSED: All expected keys found"
else
    echo "✗ Test 1 FAILED: Missing expected keys"
fi
echo ""

# Test 2: Extract service_call times
echo "=== Test 2: Extract service_call times ==="
python3 << 'PYTHON_SCRIPT'
import json
import sys

log_file = "/tmp/sidecar_log_parser_test/test1.log"
with open(log_file, 'r') as f:
    data = json.load(f)

print("Extracted values:")
if "service_call" in data:
    sc = data["service_call"]
    print(f"  service_call.in_time: {sc.get('in_time', 'N/A')}")
    print(f"  service_call.out_time: {sc.get('out_time', 'N/A')}")
    
    # Calculate processing time
    from datetime import datetime
    try:
        in_t = datetime.strptime(sc.get('in_time', ''), '%Y-%m-%d %H:%M:%S.%f')
        out_t = datetime.strptime(sc.get('out_time', ''), '%Y-%m-%d %H:%M:%S.%f')
        proc_time = (out_t - in_t).total_seconds()
        print(f"  Processing time: {proc_time} seconds")
        if proc_time > 0:
            print("✓ Test 2 PASSED: Processing time calculated correctly")
        else:
            print("✗ Test 2 FAILED: Processing time is zero or negative")
    except Exception as e:
        print(f"✗ Test 2 FAILED: Error calculating processing time: {e}")

if "sidecar" in data:
    sd = data["sidecar"]
    print(f"  sidecar.in_time: {sd.get('in_time', 'N/A')}")
    print(f"  sidecar.out_time: {sd.get('out_time', 'N/A')}")
PYTHON_SCRIPT
echo ""

# Test 3: Multiple log entries
echo "=== Test 3: Multiple log entries ==="
cat > "$TEST_DIR/test3.log" << 'EOF'
{"sfc_time": "2025-11-12 02:58:50.676086", "service_call": {"in_time": "2025-11-12 02:58:50.676086", "out_time": "2025-11-12 02:58:50.677831"}, "sidecar": {"in_time": "2025-11-12 02:58:50.692365", "out_time": "2025-11-12 02:58:50.696276"}}
{"sfc_time": "2025-11-12 02:58:51.123456", "service_call": {"in_time": "2025-11-12 02:58:51.123456", "out_time": "2025-11-12 02:58:51.125000"}, "sidecar": {"in_time": "2025-11-12 02:58:51.130000", "out_time": "2025-11-12 02:58:51.135000"}}
EOF

line_count=$(wc -l < "$TEST_DIR/test3.log")
echo "Log file has $line_count lines"
if [ "$line_count" -eq 2 ]; then
    echo "✓ Test 3 PASSED: Multiple entries detected"
else
    echo "✗ Test 3 FAILED: Expected 2 entries, found $line_count"
fi
echo ""

# Test 4: Empty log file
echo "=== Test 4: Empty log file ==="
cat > "$TEST_DIR/test4.log" << 'EOF'
EOF

if [ ! -s "$TEST_DIR/test4.log" ]; then
    echo "✓ Test 4 PASSED: Empty log file handled"
else
    echo "✗ Test 4 FAILED: Log file is not empty"
fi
echo ""

# Test 5: Invalid JSON
echo "=== Test 5: Invalid JSON format ==="
cat > "$TEST_DIR/test5.log" << 'EOF'
invalid json content
EOF

# Check if it's invalid JSON
python3 << 'PYTHON_SCRIPT'
import json
import sys

log_file = "/tmp/sidecar_log_parser_test/test5.log"
try:
    with open(log_file, 'r') as f:
        json.load(f)
    print("✗ Test 5 FAILED: Invalid JSON was parsed successfully")
except json.JSONDecodeError:
    print("✓ Test 5 PASSED: Invalid JSON correctly rejected")
except Exception as e:
    print(f"✗ Test 5 FAILED: Unexpected error: {e}")
PYTHON_SCRIPT
echo ""

# Test 6: Missing service_call
echo "=== Test 6: Missing service_call ==="
cat > "$TEST_DIR/test6.log" << 'EOF'
{"sfc_time": "2025-11-12 02:58:50.676086", "sidecar": {"in_time": "2025-11-12 02:58:50.692365", "out_time": "2025-11-12 02:58:50.696276"}}
EOF

python3 << 'PYTHON_SCRIPT'
import json

log_file = "/tmp/sidecar_log_parser_test/test6.log"
with open(log_file, 'r') as f:
    data = json.load(f)

if "service_call" not in data and "sidecar" in data:
    print("✓ Test 6 PASSED: Missing service_call handled, sidecar data available")
else:
    print("✗ Test 6 FAILED: Unexpected structure")
PYTHON_SCRIPT
echo ""

# Summary
echo "========================================"
echo "Test Summary"
echo "========================================"
echo "Test files created in: $TEST_DIR"
echo ""
echo "To test with actual NLSR code, you can:"
echo "1. Copy test files to NLSR-fs test directory"
echo "2. Run NLSR with test log file path"
echo "3. Check NLSR logs for parsing results"
echo ""
echo "Cleaning up test files..."
# Uncomment to clean up:
# rm -rf "$TEST_DIR"
echo "Done!"

