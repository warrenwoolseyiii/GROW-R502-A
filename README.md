# GROW R502-A Fingerprint Sensor Driver Library

This repository provides C, C++, and Python drivers and example applications for the GROW R502-A fingerprint sensor module.

## Features

*   **Multi-language support:**
    *   **C:** A foundational driver (`lib/C/`) providing core communication and command functions.
    *   **C++:** A wrapper class (`lib/Cpp/`) around the C driver for easier integration into C++ projects.
    *   **Python:** A pure Python library (`lib/Python/`) for use in Python applications, implementing the sensor's communication protocol.
*   **Core Sensor Functions:**
    *   Handshake
    *   Password Verification
    *   Reading System Parameters
    *   Fingerprint Image Capture
    *   Template Generation from Image
    *   Template Matching (Search)
    *   Storing Templates to Flash
    *   Deleting Templates from Flash
    *   Emptying Fingerprint Library
*   **Fingerprint Enrollment Process:**
    *   Capture first fingerprint image.
    *   Generate character file (template part 1).
    *   Capture second fingerprint image (same finger).
    *   Generate character file (template part 2).
    *   Combine character files to create a robust template.
    *   Store the final template into the sensor's flash memory.
*   **Example Applications:**
    *   Command-line examples for C, C++, and Python demonstrating library usage for various operations, including the full enrollment sequence.

## Directory Structure

```
GROW-R502-A/
├── lib/
│   ├── C/
│   │   ├── r502a_driver.h         # C driver header
│   │   └── r502a_driver.c         # C driver implementation
│   ├── Cpp/
│   │   ├── FingerprintSensor.hpp  # C++ wrapper header
│   │   └── FingerprintSensor.cpp  # C++ wrapper implementation
│   └── Python/
│       ├── r502a.py               # Python library class
│       └── r502a_driver_constants.py # Python constants
├── examples/
│   ├── C/
│   │   ├── main.c                 # C example application
│   │   └── Makefile               # Makefile for C example
│   ├── Cpp/
│   │   ├── main.cpp               # C++ example application
│   │   └── Makefile               # Makefile for C++ example
│   └── Python/
│       └── main.py                # Python example application
└── README.md
```

## Getting Started

### Prerequisites

*   A GROW R502-A fingerprint sensor module.
*   A way to connect the sensor to your computer (e.g., USB-to-Serial adapter like CP2102 or FT232RL). The sensor uses UART communication (default 57600 baud, 8N1).
*   **For C/C++ examples:** A C/C++ compiler (like GCC/G++) and Make.
*   **For Python example:** Python 3 and the `pyserial` library (`pip install pyserial`).

### 1. C Driver & Example

The C driver is located in `lib/C/`. The example application `examples/C/main.c` demonstrates its usage.

**Building the C Example:**
Navigate to the `examples/C/` directory and run `make`:
```bash
cd examples/C
make
```
This will create an executable `fingerprint_sensor_example` (or similar, check Makefile output, e.g. `build/r502a_c_example`) in a `build` subdirectory.

**Running the C Example:**
Replace `/dev/ttyUSB0` with your sensor's serial port.
```bash
./build/r502a_c_example /dev/ttyUSB0 <command> [args...]
```
**Available Commands (C Example):**
*   `handshake`: Test communication.
*   `readparams`: Read sensor system parameters.
*   `verifypwd <password_hex>`: Verify the sensor's password (default is 0x00000000).
*   `getimage`: Capture a fingerprint image.
*   `img2tz <buffer_id>`: Generate a template from the last image into CharBuffer (1 or 2).
*   `createtpl`: Combine templates from CharBuffer1 and CharBuffer2.
*   `storetpl <buffer_id> <page_id>`: Store the template from CharBuffer (1 or 2) to a specific page ID in flash.
*   `setled <ctrl> <speed> <color> <count>`: Configure Aura LED (e.g., `setled 1 200 2 0` for blue breathing).
*   (Other commands like `search`, `deletetpl`, `empty` are also available in the C driver and can be added to the example).

**Enrollment Sequence (C Example):**
1.  `./build/r502a_c_example /dev/ttyUSB0 getimage` (Place finger)
2.  `./build/r502a_c_example /dev/ttyUSB0 img2tz 1`
3.  `./build/r502a_c_example /dev/ttyUSB0 getimage` (Place same finger again)
4.  `./build/r502a_c_example /dev/ttyUSB0 img2tz 2`
5.  `./build/r502a_c_example /dev/ttyUSB0 createtpl`
6.  `./build/r502a_c_example /dev/ttyUSB0 storetpl 1 <page_id>` (e.g., `storetpl 1 0`)

### 2. C++ Wrapper & Example

The C++ wrapper in `lib/Cpp/` provides an object-oriented interface.

**Building the C++ Example:**
Navigate to the `examples/Cpp/` directory and run `make`:
```bash
cd examples/Cpp
make
```
This will create an executable `fingerprint_sensor_cpp_example` (or similar, e.g. `build/r502a_cpp_example`) in a `build` subdirectory.

**Running the C++ Example:**
Replace `/dev/ttyUSB0` with your sensor's serial port.
```bash
./build/r502a_cpp_example /dev/ttyUSB0 <command> [args...]
```
**Available Commands (C++ Example):**
Commands are similar to the C example, including `handshake`, `readparams`, `verifypwd`, `getimage`, `img2tz`, `createtpl`, `storetpl`, and `setled <ctrl> <speed> <color> <count>`.

**Enrollment Sequence (C++ Example):**
Follow the same sequence as the C example, using the C++ executable.

### 3. Python Library & Example

The Python library is in `lib/Python/`. The example script `examples/Python/main.py` uses this library.

**Prerequisites for Python:**
```bash
pip install pyserial
```

**Running the Python Example:**
Navigate to the `examples/Python/` directory. Replace `/dev/ttyUSB0` with your sensor's serial port.
```bash
python3 ./main.py /dev/ttyUSB0 <command> [args...]
```
**Available Commands (Python Example):**
*   `handshake`: Test communication.
*   `readparams`: Read sensor system parameters.
*   `verifypwd <password_hex>`: Verify the sensor's password.
*   `getimage`: Capture a fingerprint image.
*   `img2tz <buffer_id>`: Generate a template (CharBuffer 1 or 2).
*   `createtpl`: Combine templates from CharBuffer1 and CharBuffer2.
*   `storetpl <buffer_id> <page_id>`: Store template to flash.
*   `search <buffer_id> <start_page> <num_pages>`: Search for a fingerprint.
*   `deletetpl <start_page> <num_to_delete>`: Delete template(s).
*   `empty`: Empty the entire fingerprint library.
*   `setled <ctrl_code> <speed> <color_index> <count>`: Configure Aura LED.

**Enrollment Sequence (Python Example):**
1.  `python3 ./main.py /dev/ttyUSB0 getimage` (Place finger)
2.  `python3 ./main.py /dev/ttyUSB0 img2tz 1`
3.  `python3 ./main.py /dev/ttyUSB0 getimage` (Place same finger again)
4.  `python3 ./main.py /dev/ttyUSB0 img2tz 2`
5.  `python3 ./main.py /dev/ttyUSB0 createtpl`
6.  `python3 ./main.py /dev/ttyUSB0 storetpl 1 <page_id>` (e.g., `storetpl 1 0`)


## Library Usage Notes

### C Library (`lib/C/r502a_driver.h`)
*   Include `r502a_driver.h` in your C project.
*   Initialize an `r502a_handle_t` structure with your UART read/write function pointers and device address.
*   Call the `r502a_...` functions.
*   Most functions that interact with the sensor return a driver status code (e.g., `R502A_CONF_OK` for successful communication by the driver) and provide the sensor's actual confirmation code via an output parameter pointer. Check both.

### C++ Library (`lib/Cpp/FingerprintSensor.hpp`)
*   Include `FingerprintSensor.hpp`.
*   Instantiate the `Grow::FingerprintSensor` class, providing UART function pointers.
*   Call methods on the sensor object.
*   Similar to the C driver, methods that interact with the sensor return a driver status, and the sensor's confirmation code is passed via a pointer.

### Python Library (`lib/Python/r502a.py`)
*   Import the `FingerprintSensor` class from `lib.Python.r502a`.
*   Instantiate `FingerprintSensor(port, baudrate, ...)`.
*   Call `connect()` before other operations and `disconnect()` when done.
*   Methods directly return the sensor's confirmation code or a tuple `(confirmation_code, data)` for functions like `read_system_parameters` or `search_fingerprint`.
*   The library uses constants defined in `lib/Python/r502a_driver_constants.py`.

## Contributing
Contributions, bug reports, and feature requests are welcome. Please open an issue or submit a pull request.

## License
This project is licensed under the MIT License - see the LICENSE file for details (TODO: Add LICENSE file).
