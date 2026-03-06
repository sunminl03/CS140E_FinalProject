# HDLC Framing Tests

This directory contains comprehensive tests for the HDLC-like framing implementation (RFC 1662).

## Test Files

The tests are organized by functionality and difficulty:

- **0-basic-encode-decode.c**: Basic round-trip encoding/decoding tests
  - Simple ASCII strings
  - Binary data with special bytes
  - Empty payload handling

- **1-crc16-test.c**: CRC-16 calculation verification
  - Known test vectors
  - Consistency checks
  - Edge cases

- **2-escape-test.c**: Character escaping tests
  - Flag bytes (0x7E)
  - Escape bytes (0x7D)
  - Control characters (< 0x20)
  - All special characters together

- **3-fcs-verification.c**: Frame Check Sequence verification
  - Valid frame decoding
  - Corrupted payload detection
  - Corrupted FCS detection
  - Incomplete frame handling

- **4-edge-cases.c**: Edge case testing
  - Single byte payload
  - Large payloads
  - Maximum escaping scenarios
  - No escaping scenarios
  - Buffer size limits

- **5-error-handling.c**: Error condition testing
  - NULL pointer handling
  - Invalid frame structures
  - Incomplete escape sequences
  - Frame validation

## Running Tests

### Option 1: Add to Parent Makefile (Recommended)

Edit `../Makefile` and add the test programs:

```makefile
# Add test programs
PROGS += tests/0-basic-encode-decode.c
PROGS += tests/1-crc16-test.c
PROGS += tests/2-escape-test.c
PROGS += tests/3-fcs-verification.c
PROGS += tests/4-edge-cases.c
PROGS += tests/5-error-handling.c

# Add common source
COMMON_SRC += hdlc_framing.c
```

Then from the parent directory:
```bash
make run
```

### Option 2: Build Individual Tests

From the parent directory, build and run a single test:

```bash
make PROGS="tests/0-basic-encode-decode.c" COMMON_SRC="hdlc_framing.c" run
```

### Option 3: Using Fake-Pi (For Unit Testing)

If you have fake-pi set up, you can run these tests without hardware:

```bash
cd ../3-cross-checking/1-fake-pi
# Copy test files and hdlc_framing.c/h to fake-pi directory
# Then run tests
```

## Test Output

Each test prints:
- Test case descriptions
- Success/error messages
- Detailed information about what's being tested

Look for "SUCCESS" messages to verify tests pass, and "ERROR" messages for failures.

## Expected Results

All tests should pass with the correct HDLC framing implementation. The tests verify:

1. **Correctness**: Encoding and decoding preserve data
2. **FCS**: Frame Check Sequence detects corruption
3. **Escaping**: Special characters are properly escaped
4. **Error Handling**: Invalid inputs are rejected
5. **Edge Cases**: Boundary conditions are handled

## Debugging

If a test fails:
1. Check the error message for details
2. Verify the HDLC framing implementation matches RFC 1662
3. Check CRC-16 calculation (should use CRC-16-CCITT/X.25)
4. Verify escaping rules (FLAG, ESCAPE, and control chars < 0x20)

## Notes

- These tests don't require UART hardware - they test the encoding/decoding logic directly
- The tests use `printk()` for output (standard cs140e output function)
- All tests use the `notmain()` entry point (standard cs140e convention)
