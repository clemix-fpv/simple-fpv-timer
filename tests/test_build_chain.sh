#!/bin/bash
# Test script to validate build chain works correctly
# Tests that changes to index.html appear in the build output on FIRST build

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASS="${GREEN}✓ PASS${NC}"
FAIL="${RED}✗ FAIL${NC}"
INFO="${BLUE}ℹ INFO${NC}"

echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Build Chain Validation Test${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
echo ""

# Find the project root directory first (where platformio.ini is located)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Change to project root to ensure git commands work
cd "$PROJECT_ROOT"

# Get the current branch
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
echo -e "${INFO} Current branch: $CURRENT_BRANCH"

# Check if the remote branch exists
echo -e "${INFO} Checking if remote branch exists..."
if ! git ls-remote --exit-code --heads origin "$CURRENT_BRANCH" > /dev/null 2>&1; then
    echo -e "${YELLOW}⚠ WARNING${NC} Remote branch '$CURRENT_BRANCH' does not exist on origin."
    echo ""
    echo -e "Do you want to push the current branch to create it on the remote?"
    echo -e "Command: ${YELLOW}git push -u origin $CURRENT_BRANCH${NC}"
    echo ""
    read -p "Push branch to remote? [y/N] " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${INFO} Pushing branch to remote..."
        if git push -u origin "$CURRENT_BRANCH"; then
            echo -e "${PASS} Branch pushed successfully"
        else
            echo -e "${FAIL} Failed to push branch"
            exit 1
        fi
    else
        echo -e "${INFO} Skipping push. Cannot continue test without remote branch."
        exit 1
    fi
else
    echo -e "${PASS} Remote branch '$CURRENT_BRANCH' exists"
fi

# Create test directory
TEST_DIR="$PROJECT_ROOT/tests/tmp/build-test-$$"
echo -e "${INFO} Creating test directory: $TEST_DIR"
mkdir -p "$TEST_DIR"

# Setup cleanup trap to ensure test directory is removed even on interruption
cleanup() {
    EXIT_CODE=$?
    if [ -d "$TEST_DIR" ]; then
        echo ""
        echo -e "${INFO} Cleaning up test directory: $TEST_DIR"
        cd "$PROJECT_ROOT"
        rm -rf "$TEST_DIR"
    fi
    exit $EXIT_CODE
}
trap cleanup EXIT INT TERM

echo ""
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"
echo -e "${BLUE} Step 1: Clone fresh repository${NC}"
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"

cd "$TEST_DIR"
git clone --branch "$CURRENT_BRANCH" --depth 1 https://github.com/ulph0/simple-fpv-timer.git test-repo
cd test-repo

echo -e "${PASS} Fresh clone complete"

echo ""
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"
echo -e "${BLUE} Step 2: Initial build from scratch${NC}"
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"

pio run

# Check that static_files.h was generated
if [ ! -f "src/src/static_files.h" ]; then
    echo -e "${FAIL} static_files.h not generated!"
    exit 1
fi

echo -e "${PASS} Initial build complete"
echo -e "${INFO} Checking initial static_files.h content..."

# Save hash of static_files.h
ORIGINAL_HASH=$(md5sum src/src/static_files.h | awk '{print $1}')
echo -e "${INFO} Original static_files.h hash: $ORIGINAL_HASH"

# Verify original content exists and looks reasonable
ORIGINAL_SIZE=$(stat -f%z src/src/static_files.h 2>/dev/null || stat -c%s src/src/static_files.h)
echo -e "${INFO} Original static_files.h size: $ORIGINAL_SIZE bytes"

if [ "$ORIGINAL_SIZE" -lt 5000 ]; then
    echo -e "${FAIL} static_files.h too small! Should contain embedded web GUI."
    exit 1
fi

# Check for expected structure (file entries with names)
if ! grep -q "\.name = \"/index.html" src/src/static_files.h; then
    echo -e "${FAIL} No index.html entry found in static_files.h!"
    exit 1
fi

if ! grep -q "\.name = \"/app.js" src/src/static_files.h; then
    echo -e "${FAIL} No app.js entry found in static_files.h!"
    exit 1
fi

echo -e "${PASS} Found original GUI content structure in static_files.h"

echo ""
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"
echo -e "${BLUE} Step 3: Modify index.html${NC}"
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"

echo "HELLO WORLD FROM BUILD CHAIN TEST" > src/data_src/index.html

echo -e "${PASS} Modified index.html"
cat src/data_src/index.html

echo ""
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"
echo -e "${BLUE} Step 4: Rebuild (THIS IS THE CRITICAL TEST)${NC}"
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"

pio run

# Check that static_files.h changed
NEW_HASH=$(md5sum src/src/static_files.h | awk '{print $1}')
echo -e "${INFO} New static_files.h hash: $NEW_HASH"

if [ "$ORIGINAL_HASH" = "$NEW_HASH" ]; then
    echo -e "${FAIL} static_files.h did NOT change after modifying index.html!"
    echo -e "${FAIL} This means the build system didn't regenerate the header!"
    exit 1
fi

echo -e "${PASS} static_files.h changed (hash different)"

# CRITICAL CHECK: Verify the new content actually contains "HELLO WORLD"
echo -e "${INFO} Verifying modified content is embedded correctly..."

# The new static_files.h should be much smaller since it only has "HELLO WORLD..."
NEW_SIZE=$(stat -f%z src/src/static_files.h 2>/dev/null || stat -c%s src/src/static_files.h)
echo -e "${INFO} New static_files.h size: $NEW_SIZE bytes (was $ORIGINAL_SIZE bytes)"

# Extract index.html content from static_files.h and decompress it
# The content is in hex format like: 0x1f, 0x8b, 0x08, ...
echo -e "${INFO} Extracting embedded index.html to verify content..."

# Create a Python script to extract and decompress
cat > /tmp/extract_check.py << 'PYTHON_SCRIPT'
import re
import gzip
import sys

# Read the static_files.h
with open('src/src/static_files.h', 'r') as f:
    content = f.read()

# Find the index.html file entry (look for the array before index.html path)
# Pattern: find "file_XX[] = {" followed by hex bytes, then look for /index.html in the struct
match = re.search(r'/\* index\.html\.gz \*/\s*const unsigned char file_(\d+)\[\] = \{([^}]+)\}', content, re.DOTALL)

if not match:
    print("ERROR: Could not find index.html.gz data")
    sys.exit(1)

hex_data = match.group(2)

# Extract hex values (0xNN format)
hex_values = re.findall(r'0x([0-9a-fA-F]{2})', hex_data)

if not hex_values:
    print("ERROR: No hex values found")
    sys.exit(1)

# Convert to bytes
byte_data = bytes([int(h, 16) for h in hex_values])

try:
    # Decompress gzip data
    decompressed = gzip.decompress(byte_data).decode('utf-8')
    print("CONTENT:", decompressed)
    
    # Check if it contains "HELLO WORLD"
    if "HELLO WORLD FROM BUILD CHAIN TEST" in decompressed:
        print("VERIFICATION: SUCCESS - Found expected content")
        sys.exit(0)
    else:
        print("VERIFICATION: FAILED - Expected content not found")
        print("Got:", decompressed[:100])
        sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    sys.exit(1)
PYTHON_SCRIPT

# Run the verification
if python3 /tmp/extract_check.py; then
    echo -e "${PASS} Verified: index.html contains 'HELLO WORLD FROM BUILD CHAIN TEST'"
else
    echo -e "${FAIL} Content verification failed! See output above."
    rm -f /tmp/extract_check.py
    exit 1
fi

rm -f /tmp/extract_check.py

echo ""
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"
echo -e "${BLUE} Step 5: Verify firmware was rebuilt${NC}"
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"

FIRMWARE_PATH=".pio/build/esp32dev/firmware.bin"

if [ ! -f "$FIRMWARE_PATH" ]; then
    echo -e "${FAIL} Firmware binary not found!"
    exit 1
fi

# Check firmware contains different content
# Note: We can't easily verify the exact content in the binary without
# extracting and decompressing, but we can verify it was rebuilt
FIRMWARE_SIZE=$(stat -f%z "$FIRMWARE_PATH" 2>/dev/null || stat -c%s "$FIRMWARE_PATH")
echo -e "${INFO} Firmware size: $FIRMWARE_SIZE bytes ($(($FIRMWARE_SIZE / 1024)) KB)"

# Firmware should be reasonable size (not too small, not too large)
MIN_SIZE=$((100 * 1024))  # 100KB minimum
MAX_SIZE=$((2 * 1024 * 1024))  # 2MB maximum

if [ "$FIRMWARE_SIZE" -lt "$MIN_SIZE" ]; then
    echo -e "${FAIL} Firmware too small (< 100KB)! Build may have failed."
    exit 1
fi

if [ "$FIRMWARE_SIZE" -gt "$MAX_SIZE" ]; then
    echo -e "${FAIL} Firmware too large (> 2MB)! Won't fit in flash."
    exit 1
fi

echo -e "${PASS} Firmware size within acceptable range"

echo ""
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"
echo -e "${BLUE} Step 6: Final verification${NC}"
echo -e "${BLUE}────────────────────────────────────────────────────${NC}"

# Check critical files exist
if [ -f "src/src/static_files.h" ]; then
    echo -e "${PASS} static_files.h exists"
else
    echo -e "${FAIL} static_files.h missing!"
    exit 1
fi

if [ -f "src/src/config_default.c" ]; then
    echo -e "${PASS} config_default.c exists"
else
    echo -e "${FAIL} config_default.c missing!"
    exit 1
fi

if [ -f ".pio/build/esp32dev/firmware.elf" ]; then
    echo -e "${PASS} firmware.elf exists"
else
    echo -e "${FAIL} firmware.elf missing!"
    exit 1
fi

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════${NC}"
echo -e "${GREEN}          ALL TESTS PASSED ✓${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════${NC}"
echo ""
echo -e "${PASS} Initial build: Original GUI embedded correctly"
echo -e "${PASS} After modifying index.html: Changes appear in FIRST rebuild"
echo -e "${PASS} Build chain working - no extra rebuild needed"
echo ""

# Cleanup is handled by the trap handler
