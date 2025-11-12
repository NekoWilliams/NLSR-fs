#!/bin/bash
# Test script that mimics the actual parser logic

set -e

echo "========================================"
echo "Sidecar Log Parser Logic Test"
echo "========================================"
echo ""

# Test function that mimics parseSidecarLog logic
test_parse_line() {
    local line="$1"
    local test_name="$2"
    
    echo "=== Test: $test_name ==="
    echo "Input: ${line:0:100}..."
    
    # Mimic the parser logic
    local entry=""
    
    # Extract service_call information
    if [[ $line == *"\"service_call\""* ]]; then
        # Find service_call block
        local sc_start=$(echo "$line" | grep -o "\"service_call\"" | head -1)
        if [ -n "$sc_start" ]; then
            # Extract in_time
            if [[ $line == *"\"in_time\""* ]]; then
                local in_time=$(echo "$line" | grep -oP '"in_time"\s*:\s*"\K[^"]*' | head -1)
                if [ -n "$in_time" ]; then
                    entry="${entry}service_call_in_time=$in_time "
                    echo "  ✓ Extracted service_call_in_time: $in_time"
                fi
            fi
            
            # Extract out_time
            if [[ $line == *"\"out_time\""* ]]; then
                local out_time=$(echo "$line" | grep -oP '"out_time"\s*:\s*"\K[^"]*' | head -1)
                if [ -n "$out_time" ]; then
                    entry="${entry}service_call_out_time=$out_time "
                    echo "  ✓ Extracted service_call_out_time: $out_time"
                fi
            fi
        fi
    fi
    
    # Extract sidecar information
    if [[ $line == *"\"sidecar\""* ]]; then
        # Extract in_time
        if [[ $line == *"\"in_time\""* ]]; then
            # Find sidecar block's in_time (second occurrence)
            local sidecar_in_time=$(echo "$line" | grep -oP '"sidecar"[^}]*"in_time"\s*:\s*"\K[^"]*' | head -1)
            if [ -n "$sidecar_in_time" ]; then
                entry="${entry}sidecar_in_time=$sidecar_in_time "
                echo "  ✓ Extracted sidecar_in_time: $sidecar_in_time"
            fi
        fi
        
        # Extract out_time
        if [[ $line == *"\"out_time\""* ]]; then
            local sidecar_out_time=$(echo "$line" | grep -oP '"sidecar"[^}]*"out_time"\s*:\s*"\K[^"]*' | head -1)
            if [ -n "$sidecar_out_time" ]; then
                entry="${entry}sidecar_out_time=$sidecar_out_time "
                echo "  ✓ Extracted sidecar_out_time: $sidecar_out_time"
            fi
        fi
    fi
    
    # Extract sfc_time
    if [[ $line == *"\"sfc_time\""* ]]; then
        local sfc_time=$(echo "$line" | grep -oP '"sfc_time"\s*:\s*"\K[^"]*' | head -1)
        if [ -n "$sfc_time" ]; then
            entry="${entry}sfc_time=$sfc_time "
            echo "  ✓ Extracted sfc_time: $sfc_time"
        fi
    fi
    
    if [ -n "$entry" ]; then
        echo "  ✓ Test PASSED: Extracted values: $entry"
        return 0
    else
        echo "  ✗ Test FAILED: No values extracted"
        return 1
    fi
}

# Test data
TEST_LINE1='{"sfc_time": "2025-11-12 02:58:50.676086", "service_call": {"call_name": "/relay", "in_time": "2025-11-12 02:58:50.676086", "out_time": "2025-11-12 02:58:50.677831", "port_num": 6363, "in_datasize": 13, "out_datasize": 13}, "sidecar": {"in_time": "2025-11-12 02:58:50.692365", "out_time": "2025-11-12 02:58:50.696276", "name": "ndn-sidecar", "protocol": "ndn"}, "host_name": "192.168.49.2"}'

TEST_LINE2='{"sfc_time": "2025-11-12 02:58:51.123456", "service_call": {"in_time": "2025-11-12 02:58:51.123456", "out_time": "2025-11-12 02:58:51.125000"}, "sidecar": {"in_time": "2025-11-12 02:58:51.130000", "out_time": "2025-11-12 02:58:51.135000"}}'

TEST_LINE3='{"sidecar": {"in_time": "2025-11-12 02:58:50.692365", "out_time": "2025-11-12 02:58:50.696276"}}'

# Run tests
passed=0
total=0

total=$((total + 1))
if test_parse_line "$TEST_LINE1" "NormalEntry"; then
    passed=$((passed + 1))
fi
echo ""

total=$((total + 1))
if test_parse_line "$TEST_LINE2" "SecondEntry"; then
    passed=$((passed + 1))
fi
echo ""

total=$((total + 1))
if test_parse_line "$TEST_LINE3" "MissingServiceCall"; then
    passed=$((passed + 1))
fi
echo ""

# Summary
echo "========================================"
echo "Test Summary: $passed/$total tests passed"
echo "========================================"

# Now test the actual extraction with Python for comparison
echo ""
echo "=== Python-based extraction test (for comparison) ==="
python3 << 'PYTHON_SCRIPT'
import json
import re

def extract_values_mimic_parser(line):
    """Mimic the C++ parser logic"""
    entry = {}
    
    # Extract service_call information
    if '"service_call"' in line:
        # Find service_call block
        sc_match = re.search(r'"service_call"\s*:\s*\{([^}]+)\}', line)
        if sc_match:
            sc_content = sc_match.group(1)
            
            # Extract in_time
            in_time_match = re.search(r'"in_time"\s*:\s*"([^"]+)"', sc_content)
            if in_time_match:
                entry["service_call_in_time"] = in_time_match.group(1)
            
            # Extract out_time
            out_time_match = re.search(r'"out_time"\s*:\s*"([^"]+)"', sc_content)
            if out_time_match:
                entry["service_call_out_time"] = out_time_match.group(1)
    
    # Extract sidecar information
    if '"sidecar"' in line:
        sidecar_match = re.search(r'"sidecar"\s*:\s*\{([^}]+)\}', line)
        if sidecar_match:
            sidecar_content = sidecar_match.group(1)
            
            # Extract in_time
            in_time_match = re.search(r'"in_time"\s*:\s*"([^"]+)"', sidecar_content)
            if in_time_match:
                entry["sidecar_in_time"] = in_time_match.group(1)
            
            # Extract out_time
            out_time_match = re.search(r'"out_time"\s*:\s*"([^"]+)"', sidecar_content)
            if out_time_match:
                entry["sidecar_out_time"] = out_time_match.group(1)
    
    # Extract sfc_time
    if '"sfc_time"' in line:
        sfc_time_match = re.search(r'"sfc_time"\s*:\s*"([^"]+)"', line)
        if sfc_time_match:
            entry["sfc_time"] = sfc_time_match.group(1)
    
    return entry

# Test with actual log line
test_line = '{"sfc_time": "2025-11-12 02:58:50.676086", "service_call": {"call_name": "/relay", "in_time": "2025-11-12 02:58:50.676086", "out_time": "2025-11-12 02:58:50.677831", "port_num": 6363, "in_datasize": 13, "out_datasize": 13}, "sidecar": {"in_time": "2025-11-12 02:58:50.692365", "out_time": "2025-11-12 02:58:50.696276", "name": "ndn-sidecar", "protocol": "ndn"}, "host_name": "192.168.49.2"}'

print("Testing parser logic...")
result = extract_values_mimic_parser(test_line)

print("Extracted values:")
for key, value in result.items():
    print(f"  {key}: {value}")

# Verify against JSON parsing
data = json.loads(test_line)
print("\nVerification against JSON parsing:")
if "service_call" in data:
    print(f"  service_call.in_time: {data['service_call'].get('in_time')}")
    print(f"  service_call.out_time: {data['service_call'].get('out_time')}")
if "sidecar" in data:
    print(f"  sidecar.in_time: {data['sidecar'].get('in_time')}")
    print(f"  sidecar.out_time: {data['sidecar'].get('out_time')}")

# Check if extracted values match
if (result.get("service_call_in_time") == data["service_call"].get("in_time") and
    result.get("service_call_out_time") == data["service_call"].get("out_time") and
    result.get("sidecar_in_time") == data["sidecar"].get("in_time") and
    result.get("sidecar_out_time") == data["sidecar"].get("out_time")):
    print("\n✓ Parser logic test PASSED: All values match JSON parsing")
else:
    print("\n✗ Parser logic test FAILED: Values don't match")
PYTHON_SCRIPT

