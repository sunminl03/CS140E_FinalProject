#!/usr/bin/env python3
"""
HDLC loopback test - laptop side

Workflow:
  1. First, load the loopback test program on the Pi:
     make
     
     IMPORTANT: The loopback test runs forever, so 'make' will also run forever
     (my-install keeps reading from the port). You have two options:
     
     Option A: Run in separate terminals
     - Terminal 1: Run 'make' (let it keep running)
     - Terminal 2: Run this Python script
     
     Option B: Kill my-install after bootloader finishes
     - Run 'make' in background: make &
     - Wait for "BOOT:bootloader: Done." and "HDLC loopback test starting..."
     - Kill my-install: pkill my-install (or Ctrl+C if in foreground)
     - Then run this Python script
     
  2. Run this script:
     python3 tests/tests-hdlc/hdlc_laptop_test.py
     
  The script will auto-detect the serial port, or you can specify it:
     python3 tests/tests-hdlc/hdlc_laptop_test.py /dev/cu.usbserial-110
  
  If you get "no data received", make sure:
  - my-install is NOT running (check with: ps aux | grep my-install)
  - The Pi program is running (you should see "HDLC loopback test starting..." in make output)
  - No other program is using the serial port
"""

"""
How the test works:
1. Laptop sends a frame(decoded payload) to the Pi
2. Waits for a response
    - wait 0.5 seconds to ensure Pi has time to process
    - check if there is any data in the buffer
    - polls for up to 2 seconds every 50 ms 
    - collect incoming data
3. Call read_one_hdlc_frame() with a 5s timeout
4. Pass any already-collected data
5. Decode recieved frame, compare payload with original
"""
import serial
import serial.tools.list_ports
import sys
import time

HDLC_FLAG = 0x7E
HDLC_ESCAPE = 0x7D
HDLC_XOR = 0x20

# CRC-16-CCITT/X.25 table from your C code
CRC16_TABLE = [
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
    0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
    0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
    0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
    0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
    0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
    0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
    0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
    0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
    0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
    0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
    0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
    0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
    0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
    0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
    0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
    0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
    0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
    0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
    0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
    0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78
]

def hdlc_crc16(data: bytes) -> int:
    if len(data) == 0:
        return 0xFFFF   # matches your current C implementation
    crc = 0xFFFF
    for b in data:
        idx = (crc ^ b) & 0xFF
        crc = (crc >> 8) ^ CRC16_TABLE[idx]
    return crc ^ 0xFFFF

def escape_byte(out: bytearray, b: int):
    if b == HDLC_FLAG or b == HDLC_ESCAPE or b < 0x20:
        out.append(HDLC_ESCAPE)
        out.append(b ^ HDLC_XOR)
    else:
        out.append(b)

def hdlc_encode(payload: bytes) -> bytes:
    out = bytearray()
    out.append(HDLC_FLAG)

    for b in payload:
        escape_byte(out, b)

    fcs = hdlc_crc16(payload)
    escape_byte(out, fcs & 0xFF)
    escape_byte(out, (fcs >> 8) & 0xFF)

    out.append(HDLC_FLAG)
    return bytes(out)

def hdlc_decode_flagged(frame: bytes) -> bytes:
    # Expects full frame including starting/ending flags.
    if len(frame) < 4:
        raise ValueError("frame too short")
    if frame[0] != HDLC_FLAG or frame[-1] != HDLC_FLAG:
        raise ValueError("missing flags")

    body = frame[1:-1]
    decoded = bytearray()
    i = 0
    while i < len(body):
        if body[i] == HDLC_ESCAPE:
            if i + 1 >= len(body):
                raise ValueError("trailing escape")
            decoded.append(body[i + 1] ^ HDLC_XOR)
            i += 2
        else:
            decoded.append(body[i])
            i += 1

    if len(decoded) < 2:
        raise ValueError("missing fcs")

    payload = decoded[:-2]
    recv_fcs = decoded[-2] | (decoded[-1] << 8)
    calc_fcs = hdlc_crc16(payload)
    if recv_fcs != calc_fcs:
        raise ValueError(f"bad fcs: recv=0x{recv_fcs:04x} calc=0x{calc_fcs:04x}")

    return bytes(payload)

def read_one_hdlc_frame(ser: serial.Serial, timeout=3.0, debug=False, initial_data=b'') -> bytes:
    deadline = time.time() + timeout
    frame = None
    
    # Process any initial data we already have
    data_buffer = bytearray(initial_data)
    buffer_pos = 0

    # wait for starting flag
    # Skip any garbage/printk output until we find a flag
    if debug:
        print("  Waiting for starting flag...")
    bytes_seen = 0
    while time.time() < deadline:
        # First check our buffer, then read from serial
        if buffer_pos < len(data_buffer):
            b = bytes([data_buffer[buffer_pos]])
            buffer_pos += 1
        else:
            try:
                b = ser.read(1)
            except serial.serialutil.SerialException as e:
                if "device reports readiness" in str(e) or "no data" in str(e):
                    # Device might be disconnected or port conflict - wait a bit and retry
                    time.sleep(0.1)
                    continue
                raise
            if not b:
                time.sleep(0.01)  # Small sleep to avoid busy-waiting
                continue
            data_buffer.append(b[0])  # Add to buffer for potential future use
        
        bytes_seen += 1
        if debug and bytes_seen <= 10:  # Show first few bytes for debugging
            print(f"    Read byte: 0x{b[0]:02x} ({chr(b[0]) if 32 <= b[0] < 127 else '?'})")
        
        if b[0] == HDLC_FLAG:
            frame = bytearray([HDLC_FLAG])
            if debug:
                print(f"  Found starting flag after {bytes_seen} bytes (at buffer position {buffer_pos-1})")
            # Check if the frame might already be complete in our buffer
            # Look ahead for an ending flag
            if buffer_pos < len(data_buffer):
                # Check remaining bytes in buffer for ending flag
                for i in range(buffer_pos, len(data_buffer)):
                    if data_buffer[i] == HDLC_FLAG:
                        # Found complete frame in buffer!
                        frame.extend(data_buffer[buffer_pos:i+1])
                        buffer_pos = i + 1
                        if debug:
                            print(f"  Found complete frame in buffer! Frame length: {len(frame)}")
                        return bytes(frame)
            break
    
    if frame is None:
        # Check if there's any data in the buffer
        remaining = ser.read(ser.in_waiting) if ser.in_waiting > 0 else b''
        if remaining:
            raise TimeoutError(f"timed out waiting for starting flag (received {len(remaining)} bytes: {hex_bytes(remaining[:50])}...)")
        else:
            raise TimeoutError("timed out waiting for starting flag (no data received - Pi may not be running or not responding)")

    # read until ending flag
    # Continue reading from where we left off (after the starting flag)
    if debug:
        print("  Reading frame body...")
    while time.time() < deadline:
        # Continue from buffer if we have data there, otherwise read from serial
        if buffer_pos < len(data_buffer):
            b = bytes([data_buffer[buffer_pos]])
            buffer_pos += 1
        else:
            try:
                b = ser.read(1)
            except serial.serialutil.SerialException as e:
                if "device reports readiness" in str(e) or "no data" in str(e):
                    # Device might be disconnected or port conflict - wait a bit and retry
                    time.sleep(0.1)
                    continue
                raise
            if not b:
                time.sleep(0.01)
                continue
            data_buffer.append(b[0])  # Add to buffer
        
        frame.append(b[0])
        if b[0] == HDLC_FLAG and len(frame) > 1:
            if debug:
                print(f"  Found ending flag, frame length: {len(frame)}")
            return bytes(frame)

    raise TimeoutError(f"timed out waiting for ending flag (received so far: {hex_bytes(bytes(frame))})")

def hex_bytes(bs: bytes) -> str:
    return " ".join(f"{b:02x}" for b in bs)

def find_serial_port():
    """Auto-detect USB serial port, preferring cu.usbserial devices on macOS"""
    ports = serial.tools.list_ports.comports()
    
    # Filter for USB serial devices
    usb_ports = [p for p in ports if 'usb' in p.device.lower() or 'usb' in p.description.lower()]
    
    if not usb_ports:
        print("No USB serial ports found.")
        print("\nAvailable ports:")
        for port in ports:
            print(f"  {port.device} - {port.description}")
        return None
    
    # Prefer cu.usbserial on macOS
    for port in usb_ports:
        if 'cu.usbserial' in port.device:
            return port.device
    
    # Otherwise return first USB port
    return usb_ports[0].device

def main():
    if len(sys.argv) >= 2:
        port = sys.argv[1]
    else:
        # Auto-detect port
        port = find_serial_port()
        if not port:
            print(f"\nusage: python3 {sys.argv[0]} [PORT]")
            print("  PORT: Serial port (e.g., /dev/cu.usbserial-110)")
            print("  If not provided, will attempt to auto-detect")
            sys.exit(1)
        print(f"Auto-detected port: {port}")

    # payload = bytes([
    #     0x48, 0x44, 0x4c, 0x43,   # "HDLC"
    #     0x00,
    #     0x11,
    #     0x7e,
    #     0x22,
    #     0x7d,
    #     0x33,
    #     0x03
    # ])
    payload = bytes([
    0xFF, 0x03,       # PPP address/control
    0xC0, 0x21,       # protocol = LCP
    0x11, 0x22, 0x33  # fake info field
    ])
    # sending encoded frame to the Pi
    tx_frame = hdlc_encode(payload)

    print(f"Opening {port}...")
    max_retries = 3
    ser = None
    for attempt in range(max_retries):
        try:
            # Configure for 8n1 mode (8 data bits, no parity, 1 stop bit) at 115200 baud
            # This matches the configuration used by my-install
            ser = serial.Serial(
                port=port,
                baudrate=115200,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,
                xonxoff=False,
                rtscts=False,
                dsrdtr=False
            )
            break
        except serial.serialutil.SerialException as e:
            if attempt < max_retries - 1:
                print(f"  Attempt {attempt + 1} failed: {e}")
                print("  Retrying in 1 second...")
                time.sleep(1)
            else:
                print(f"ERROR: Could not open serial port '{port}' after {max_retries} attempts")
                print(f"  {e}")
                if "Operation not permitted" in str(e) or "Permission denied" in str(e):
                    print("\n  This usually means:")
                    print("    - The port is already in use by another program (e.g., my-install)")
                    print("    - Make sure to close any other programs using the port")
                    print("    - On macOS, try: sudo chmod 666 " + port)
                elif "device reports readiness" in str(e) or "no data" in str(e):
                    print("\n  This usually means:")
                    print("    - The port is in use by another program")
                    print("    - The device might not be ready")
                    print("    - Try running 'make' first to load the program, then run this test")
                print("\nAvailable serial ports:")
                ports = serial.tools.list_ports.comports()
                if ports:
                    for p in ports:
                        print(f"  {p.device} - {p.description}")
                else:
                    print("  (none found)")
                print(f"\nUsage: python3 {sys.argv[0]} [PORT]")
                sys.exit(1)
    
    print("Port opened successfully")
    
    # IMPORTANT: Check if port is actually available
    # If my-install is still running, we might have issues
    print("  Checking port status...")
    time.sleep(0.5)  # Give device time to initialize
    
    # Check for any initial data (might indicate Pi is sending)
    initial_check = ser.read(ser.in_waiting) if ser.in_waiting > 0 else b''
    if initial_check:
        print(f"  WARNING: {len(initial_check)} bytes already in buffer: {hex_bytes(initial_check[:30])}...")
        print("  This might mean my-install is still running or Pi is sending data")
    
    # Clear buffers to start fresh
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    time.sleep(0.3)  # Give time for buffers to clear
    print("Buffers cleared")
    
    # Test: Send a single byte to see if Pi is alive
    # Actually, don't do this - it might interfere with hdlc_recv
    # Instead, just proceed and see what happens
    print("Pi should be ready (make sure 'make' has finished completely)")

    print("\nSending frame...")
    print("payload: ", hex_bytes(payload))
    print("tx frame: ", hex_bytes(tx_frame))

    print(f"  Sending {len(tx_frame)} bytes...")
    bytes_sent = ser.write(tx_frame)
    if bytes_sent != len(tx_frame):
        print(f"  WARNING: Only sent {bytes_sent} of {len(tx_frame)} bytes!")
    ser.flush()
    print(f"  Frame written ({bytes_sent} bytes) and flushed")
    
    # Give Pi time to receive, process, and send response
    # The Pi needs to: receive frame, decode, encode response, flush TX
    # At 115200 baud, 20 bytes takes ~2ms to transmit, but we need processing time
    print("  Waiting for Pi to process (0.5s)...")
    time.sleep(0.5)  # Increased delay to ensure Pi has time to process
    
    # Check if there's any data already in the buffer
    bytes_waiting = ser.in_waiting
    if bytes_waiting > 0:
        print(f"  ✓ {bytes_waiting} bytes already in buffer - response may have arrived!")
        # Don't clear it - it might be the response!
    else:
        print(f"  No data in buffer yet, will wait for response...")
    
    print("Frame sent, waiting for response...")
    
    # Wait and actively poll for data
    print("  Actively polling for response (up to 2 seconds)...")
    start_time = time.time()
    initial_data = b''
    poll_count = 0
    while time.time() - start_time < 2.0:
        poll_count += 1
        if ser.in_waiting > 0:
            chunk = ser.read(ser.in_waiting)
            initial_data += chunk
            print(f"  [Poll #{poll_count}] Received {len(chunk)} bytes (total: {len(initial_data)})")
            if len(initial_data) >= 20:  # Got a reasonable amount, might be the frame
                print(f"    Data so far: {hex_bytes(initial_data[:50])}...")
        time.sleep(0.05)  # Poll more frequently
    
    if initial_data:
        print(f"  ✓ Received {len(initial_data)} bytes total after {poll_count} polls")
        print(f"    First 50 bytes: {hex_bytes(initial_data[:50])}")
        if len(initial_data) > 50:
            print(f"    ... (showing first 50 of {len(initial_data)} bytes)")
        # Check if we have a flag byte
        if HDLC_FLAG in initial_data:
            flag_pos = initial_data.index(HDLC_FLAG)
            print(f"    ✓ Found HDLC_FLAG (0x7E) at position {flag_pos}")
        else:
            print(f"    ✗ No HDLC_FLAG (0x7E) found in received data")
    else:
        print(f"  ✗ No data received after 2 seconds ({poll_count} polls) - Pi may not be responding")
        print("  Possible issues:")
        print("    - my-install might still be running (check with 'ps aux | grep my-install')")
        print("    - Pi program might not be running")
        print("    - Port might be in use by another program")

    try:
        # If we already have data, pass it to the frame reader
        rx_frame = read_one_hdlc_frame(ser, timeout=5.0, debug=True, initial_data=initial_data)
        print("rx frame: ", hex_bytes(rx_frame))
    except TimeoutError as e:
        print(f"ERROR: {e}")
        # Check if there's any data in the buffer
        remaining = ser.read(ser.in_waiting)
        if remaining:
            print(f"Remaining data in buffer: {hex_bytes(remaining)}")
        ser.close()
        sys.exit(1)

    try:
        rx_payload = hdlc_decode_flagged(rx_frame)
        print("rx payload:", hex_bytes(rx_payload))

        if rx_payload == payload:
            print("\n✓ PASS: payload round-tripped correctly")
            ser.close()
            return 0
        else:
            print("\n✗ FAIL: payload mismatch")
            print(f"  Expected: {hex_bytes(payload)}")
            print(f"  Received: {hex_bytes(rx_payload)}")
            ser.close()
            sys.exit(2)
    except ValueError as e:
        print(f"\n✗ FAIL: Frame decode error: {e}")
        ser.close()
        sys.exit(2)

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(130)
    except Exception as e:
        print(f"\n\nUnexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)