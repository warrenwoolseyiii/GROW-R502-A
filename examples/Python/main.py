import argparse
import sys
import time # For potential delays if needed

# Adjust the path to import from the parent directory's lib/Python
sys.path.append('../../') # Or use a more robust relative import if this becomes a package
from lib.Python.r502a import FingerprintSensor, R502A_CONF_OK, R502A_CONF_NO_FINGER, \
                               R502A_CONF_FAIL_ENROLL, R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT, \
                               R502A_CONF_PWD_FAIL, R502A_CONF_FAIL_GEN_CHAR_DISORDERLY, \
                               R502A_CONF_FAIL_GEN_IMAGE_NO_PRIMARY


def main():
    parser = argparse.ArgumentParser(description="R502-A Fingerprint Sensor CLI Example")
    parser.add_argument("port", help="Serial port (e.g., /dev/ttyUSB0 or COM3)")
    parser.add_argument("command", help="Command to execute",
                        choices=["handshake", "readparams", "verifypwd", "getimage", "genchar"])
    # Add more choices as commands are implemented

    # Command-specific arguments
    parser.add_argument("--password", help="Password for 'verifypwd' (hex, e.g., 0x00000000)", default="0x00000000")
    parser.add_argument("--buffer_id", help="Buffer ID (1-6) for 'genchar'", type=int, default=1)


    args = parser.parse_args()

    sensor = FingerprintSensor(args.port)

    print(f"Attempting to connect to sensor on {args.port}...")
    if not sensor.connect():
        print(f"Failed to connect to sensor on port {args.port}.")
        return 1
    
    print(f"Connected. Executing command: {args.command}")
    ret_code = R502A_CONF_OK # Default to OK for commands not returning a specific code directly
    result_data = None

    try:
        if args.command == "handshake":
            ret_code = sensor.handshake()
            print(f"Handshake result: 0x{ret_code:02X} ({'OK' if ret_code == R502A_CONF_OK else 'FAIL'})")
        
        elif args.command == "readparams":
            ret_code, params = sensor.read_system_parameters()
            print(f"Read System Parameters result: 0x{ret_code:02X} ({'OK' if ret_code == R502A_CONF_OK else 'FAIL'})")
            if ret_code == R502A_CONF_OK and params:
                print(params) # Uses the __str__ method of SystemParameters
            result_data = params

        elif args.command == "verifypwd":
            try:
                password_val = int(args.password, 16)
            except ValueError:
                print("Invalid password format. Please use hex (e.g., 0x12345678).")
                sensor.disconnect()
                return 1
            print(f"Verifying password: 0x{password_val:08X}")
            ret_code = sensor.verify_password(password_val)
            print(f"Verify Password result: 0x{ret_code:02X} ({'OK' if ret_code == R502A_CONF_OK else ('WRONG_PWD' if ret_code == R502A_CONF_PWD_FAIL else 'FAIL')})")

        elif args.command == "getimage":
            print("Attempting to get image (GetImageEx)...")
            ret_code = sensor.get_image_extended()
            print(f"Get Image Extended result: 0x{ret_code:02X} (", end="")
            if ret_code == R502A_CONF_OK: print("OK", end="")
            elif ret_code == R502A_CONF_NO_FINGER: print("NO_FINGER", end="")
            elif ret_code == R502A_CONF_FAIL_ENROLL: print("FAIL_COLLECT", end="") # Also 0x03
            elif ret_code == R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT: print("POOR_IMAGE_QUALITY", end="") # Also 0x07
            else: print("FAIL/OTHER", end="")
            print(")")

        elif args.command == "genchar":
            if not (1 <= args.buffer_id <= 6):
                print("Invalid buffer_id. Must be 1-6.")
                sensor.disconnect()
                return 1
            print(f"Generating character file in buffer {args.buffer_id}...")
            ret_code = sensor.generate_character_file(args.buffer_id)
            print(f"Generate Character File result: 0x{ret_code:02X} (", end="")
            if ret_code == R502A_CONF_OK: print("OK", end="")
            elif ret_code == R502A_CONF_FAIL_GEN_CHAR_DISORDERLY: print("FAIL_DISORDERLY_IMG", end="")
            elif ret_code == R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT: print("FAIL_SMALL_POINT_IMG", end="")
            elif ret_code == R502A_CONF_FAIL_GEN_IMAGE_NO_PRIMARY: print("FAIL_NO_PRIMARY_IMG", end="")
            else: print("FAIL/OTHER", end="")
            print(")")
            
        # Add other commands here
        # elif args.command == "regmodel":
        # ...
        else:
            print(f"Unknown command: {args.command}")
            sensor.disconnect()
            return 1

    except Exception as e:
        print(f"An error occurred: {e}")
        sensor.disconnect()
        return 1
    finally:
        sensor.disconnect()
        print("Disconnected.")

    return 0 if ret_code == R502A_CONF_OK else 1


if __name__ == "__main__":
    sys.exit(main())