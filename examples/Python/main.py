#!/usr/bin/env python3

import argparse
import sys
import time

# Adjust the path to import from the parent directory's lib/Python
# This allows running the script directly from the examples/Python directory.
# For a proper package, this would be handled differently.
sys.path.append('../../')
from lib.Python.r502a import FingerprintSensor
# Explicitly use the same base path for the constants module
from lib.Python import r502a_driver_constants
from lib.Python.r502a_driver_constants import * # Keep this for direct access to constants


def main():
    parser = argparse.ArgumentParser(description="R502-A Fingerprint Sensor Example (Python)")
    parser.add_argument("port", help="Serial port where the R502-A sensor is connected (e.g., /dev/ttyUSB0 or COM3)")
    
    subparsers = parser.add_subparsers(dest="command", help="Command to execute", required=True)

    subparsers.add_parser("handshake", help="Perform handshake with the sensor")
    subparsers.add_parser("readparams", help="Read system parameters from the sensor")
    
    parser_verifypwd = subparsers.add_parser("verifypwd", help="Verify sensor password")
    parser_verifypwd.add_argument("password", type=lambda x: int(x, 16), help="Password in hexadecimal (e.g., 0x00000000)")

    subparsers.add_parser("getimage", help="Collect fingerprint image and store in ImageBuffer")
    
    parser_img2tz = subparsers.add_parser("img2tz", help="Generate template from ImageBuffer to CharBuffer1 or CharBuffer2")
    parser_img2tz.add_argument("buffer_id", type=int, choices=[1, 2], help="CharBuffer ID (1 or 2)")

    subparsers.add_parser("createtpl", help="Combine CharBuffer1 and CharBuffer2 to create a template")

    parser_storetpl = subparsers.add_parser("storetpl", help="Store template from CharBuffer to Flash")
    parser_storetpl.add_argument("buffer_id", type=int, choices=[1, 2], help="CharBuffer ID (1 or 2) containing the template")
    parser_storetpl.add_argument("page_id", type=int, help="Page ID (address) in Flash to store the template")

    parser_search = subparsers.add_parser("search", help="Search fingerprint library")
    parser_search.add_argument("buffer_id", type=int, choices=[1, 2], help="CharBuffer ID (1 or 2) containing template to search for")
    parser_search.add_argument("start_page", type=int, help="Starting page ID for search")
    parser_search.add_argument("num_pages", type=int, help="Number of pages to search")

    parser_deletetpl = subparsers.add_parser("deletetpl", help="Delete template(s) from Flash")
    parser_deletetpl.add_argument("start_page", type=int, help="Starting page ID to delete from")
    parser_deletetpl.add_argument("num_to_delete", type=int, help="Number of templates to delete")

    subparsers.add_parser("empty", help="Erase all stored fingerprints from the device")

    parser_setled = subparsers.add_parser("setled", help="Configure Aura LED")
    parser_setled.add_argument("ctrl_code", type=int, help="Control code (e.g., 1:breathing, 2:flashing, 3:on, 4:off)")
    parser_setled.add_argument("speed", type=int, help="Speed of effect (0-255)")
    parser_setled.add_argument("color_index", type=int, help="Color index (e.g., 1:red, 2:blue, 7:white)")
    parser_setled.add_argument("count", type=int, help="Number of cycles (0 for infinite)")
    # Add enroll and verify commands
    parser_enroll = subparsers.add_parser("enroll", help="Interactive enrollment to specified page ID")
    parser_enroll.add_argument("page_id", type=int, help="Page ID to enroll the fingerprint to")

    subparsers.add_parser("verify", help="Verify fingerprint against stored templates")

    args = parser.parse_args()

    sensor = FingerprintSensor(args.port)
    print(f"Attempting to connect to sensor on {args.port}...")
    if not sensor.connect():
        print("Failed to connect to the sensor.")
        sys.exit(1)

    print(f"Connected. Executing command: {args.command}")
    ret_code = R502A_CONF_ERR_RECV # Default to an error, will be overwritten by successful command

    try:
        if args.command == "handshake":
            ret_code = sensor.handshake()
            print(f"Handshake response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
        
        elif args.command == "readparams":
            ret_code, params = sensor.read_system_parameters()
            print(f"Read System Parameters response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
            if ret_code == R502A_CONF_OK and params:
                print(params)

        elif args.command == "verifypwd":
            ret_code = sensor.verify_password(args.password)
            print(f"Verify Password response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")

        elif args.command == "getimage":
            print("Attempting to generate image...")
            ret_code = sensor.generate_image() # Corrected method name
            print(f"Generate Image response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
            if ret_code == R502A_CONF_NO_FINGER:
                print("No finger detected. Please place your finger on the sensor.")
            elif ret_code == R502A_CONF_OK:
                print("Fingerprint image collected successfully.")

        elif args.command == "img2tz": # Renamed from genchar
            print(f"Attempting to convert image to template in CharBuffer{args.buffer_id}...")
            ret_code = sensor.image_to_template(args.buffer_id) # Corrected method name
            print(f"Image to Template response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")

        elif args.command == "createtpl":
            print("Attempting to create template (combining CharBuffer1 and CharBuffer2)...")
            ret_code = sensor.create_template()
            print(f"Create Template response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
            if ret_code == R502A_CONF_FAIL_COMBINE:
                print("Failed to combine templates. Ensure two distinct, good quality images were captured.")

        elif args.command == "storetpl":
            print(f"Attempting to store template from CharBuffer{args.buffer_id} to PageID {args.page_id}...")
            ret_code = sensor.store_template(args.buffer_id, args.page_id)
            print(f"Store Template response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
        
        elif args.command == "search":
            print(f"Searching library for template in CharBuffer{args.buffer_id} (Pages {args.start_page}-{args.start_page + args.num_pages -1})...")
            ret_code, result = sensor.search_fingerprint(args.buffer_id, args.start_page, args.num_pages)
            print(f"Search Fingerprint response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
            if ret_code == R502A_CONF_OK and result:
                print(f"  Match found: {result}")
            elif ret_code == R502A_CONF_FAIL_FIND_MATCH:
                print("  No matching fingerprint found.")
        
        elif args.command == "deletetpl":
            print(f"Deleting {args.num_to_delete} template(s) starting from PageID {args.start_page}...")
            ret_code = sensor.delete_template(args.start_page, args.num_to_delete)
            print(f"Delete Template response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")

        elif args.command == "empty":
            print("Attempting to empty the fingerprint library...")
            ret_code = sensor.empty_fingerprint_library() # Direct call for now
            print(f"Empty Library response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")

        elif args.command == "setled":
            print(f"Setting LED: Ctrl={args.ctrl_code}, Speed={args.speed}, Color={args.color_index}, Count={args.count}")
            ret_code = sensor.set_aura_led_config(args.ctrl_code, args.speed, args.color_index, args.count)
            print(f"Set LED response: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")

        elif args.command == "enroll":
            page_id = args.page_id
            print(f"Starting interactive enrollment for Page ID {page_id}.")
            # --- First Scan ---
            print("Step 1: Place your finger on the sensor for the FIRST scan, then press Enter.")
            sensor.set_aura_led_config(3, 0, 2, 1)  # LED ON, blue
            input()
            print("Capturing first image...")
            ret_code = sensor.generate_image()
            if ret_code != R502A_CONF_OK:
                print(f"Enrollment failed: GetImage (1) - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
            else:
                print("First image captured successfully. Generating template for CharBuffer1...")
                ret_code = sensor.image_to_template(1)
                if ret_code != R502A_CONF_OK:
                    print(f"Enrollment failed: Img2Tz (1) - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
                else:
                    print("Template for CharBuffer1 generated. Remove finger.")
                    sensor.set_aura_led_config(4, 0, 0, 0)  # LED OFF
                    time.sleep(2)
                    # --- Second Scan ---
                    print("Step 2: Place the SAME finger on the sensor for the SECOND scan, then press Enter.")
                    sensor.set_aura_led_config(3, 0, 2, 1)  # LED ON, blue
                    input()
                    print("Capturing second image...")
                    ret_code = sensor.generate_image()
                    if ret_code != R502A_CONF_OK:
                        print(f"Enrollment failed: GetImage (2) - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
                    else:
                        print("Second image captured successfully. Generating template for CharBuffer2...")
                        ret_code = sensor.image_to_template(2)
                        if ret_code != R502A_CONF_OK:
                            print(f"Enrollment failed: Img2Tz (2) - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
                        else:
                            print("Template for CharBuffer2 generated. Creating combined template...")
                            sensor.set_aura_led_config(4, 0, 0, 0)  # LED OFF
                            ret_code = sensor.create_template()
                            if ret_code != R502A_CONF_OK:
                                print(f"Enrollment failed: CreateTemplate - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
                                if ret_code == R502A_CONF_FINGER_NOMATCH:
                                    print("Note: The two fingerprints did not match. Please try again with the same finger.")
                            else:
                                print(f"Combined template created successfully. Storing to Page ID {page_id}...")
                                ret_code = sensor.store_template(1, page_id)
                                if ret_code != R502A_CONF_OK:
                                    print(f"Enrollment failed: StoreTemplate - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
                                else:
                                    print(f"Fingerprint successfully enrolled and stored at Page ID {page_id}!")
                                    ret_code = R502A_CONF_OK
                                    sensor.set_aura_led_config(2, 150, 4, 3)  # Flash green LED for success
            # Check the return code, if it's an error, flash red LED
            if ret_code != R502A_CONF_OK:
                sensor.set_aura_led_config(2, 150, 1, 3)  # Flash red LED for error
                print(f"Enrollment process failed with code: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
            else:
                print("Enrollment completed successfully.")

        elif args.command == "verify":
            print("Starting fingerprint verification...")
            ret_code = sensor.generate_image()
            if ret_code != R502A_CONF_OK:
                print(f"Verification failed: GetImage - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
            else:
                print("Image captured successfully. Converting to template...")
                ret_code = sensor.image_to_template(1)
                if ret_code != R502A_CONF_OK:
                    print(f"Verification failed: Img2Tz - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
                else:
                    print("Template generated successfully. Searching in library...")
                    for page_id in range(0, 200, 10):  # Search in chunks of 10 pages
                        ret_code, result = sensor.search_fingerprint(1, page_id, page_id + 10)
                        if ret_code == R502A_CONF_OK and result:
                            print(f"Fingerprint verified successfully! Found at Page ID {result.page_id}.")
                            print(f"Match Score: {result.match_score}")
                            sensor.set_aura_led_config(2, 150, 4, 3)  # Flash green LED for success
                            break
                        else:
                            print(f"Verification failed: Search - 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
                            sensor.set_aura_led_config(2, 150, 1, 3)  # Flash red LED for error
            if ret_code != R502A_CONF_OK:
                print(f"Verification process failed with code: 0x{ret_code:02X} ({error_code_to_string(ret_code)})")
            else:
                print("Verification completed successfully.")

        else:
            # This case should not be reached if subparsers are 'required'
            print(f"Unknown command: {args.command}")
            ret_code = R502A_ERR_INVALID_ARGS



    except ValueError as e: # Catches errors from int() conversion or library value checks
        print(f"Input error: {e}")
        ret_code = R502A_ERR_INVALID_ARGS
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        import traceback
        traceback.print_exc()
        ret_code = R502A_CONF_ERR_RECV # Generic error
    finally:
        if sensor:
            sensor.disconnect()
            print("Disconnected.")

    sys.exit(0 if ret_code == R502A_CONF_OK else 1)

if __name__ == "__main__":
    main()