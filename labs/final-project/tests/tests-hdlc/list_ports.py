#!/usr/bin/env python3
"""
Helper script to list available serial ports
"""

try:
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()
    
    if ports:
        print("Available serial ports:")
        for port in ports:
            print(f"  {port.device} - {port.description}")
    else:
        print("No serial ports found.")
        print("\nMake sure your Pi is connected via USB.")
        print("On macOS, ports are typically named:")
        print("  /dev/tty.usbserial-*")
        print("  /dev/tty.usbmodem*")
        print("  /dev/cu.*")
        
except ImportError:
    print("pyserial not installed. Install with: pip3 install --user pyserial")
except Exception as e:
    print(f"Error: {e}")
