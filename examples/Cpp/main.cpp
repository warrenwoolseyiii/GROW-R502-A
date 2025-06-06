#include <iostream>
#include <iomanip> // For std::hex, std::setw, std::setfill
#include <cstdlib>  // For strtoul, atoi
#include <cstring>  // For strcmp

// POSIX UART specific
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>  // For errno, EAGAIN, EWOULDBLOCK
#include <cstdio>  // For perror, fprintf, stderr
#include <ctime>   // For time()


#include "../../lib/Cpp/FingerprintSensor.hpp"
// We also need the C header for r502a_system_params_t and r502a_search_result_t
// if the C++ class uses them directly in its public interface, which it does.
#include "../../lib/C/r502a_driver.h"


// --- POSIX UART Implementation (C-style for direct use with function pointers) ---
// These functions will be passed to the FingerprintSensor C++ class.
// The C++ class expects function pointers of type r502a_uart_write_fn and r502a_uart_read_fn.

static int uart_fd_cpp = -1;

extern "C" int uart_posix_write_cpp(const uint8_t* data, uint16_t length) {
    if (uart_fd_cpp == -1) return -1;
    ssize_t written = write(uart_fd_cpp, data, length);
    if (written < 0) {
        perror("UART (cpp) write error");
        return -1;
    }
    if (written != length) {
        fprintf(stderr, "UART (cpp) write incomplete: wrote %zd of %d bytes\n", written, length);
        return -1;
    }
    return 0; // Success
}

extern "C" int uart_posix_read_cpp(uint8_t* buffer, uint16_t length, uint32_t timeout_ms) {
    if (uart_fd_cpp == -1) return -1;
    
    ssize_t bytes_read = 0;
    uint16_t total_read = 0;
    // Using a simple select-based timeout for potentially better responsiveness
    fd_set read_fds;
    struct timeval tv;

    FD_ZERO(&read_fds);
    FD_SET(uart_fd_cpp, &read_fds);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    // Try to read 'length' bytes
    while(total_read < length) {
        int retval = select(uart_fd_cpp + 1, &read_fds, NULL, NULL, &tv);
        if (retval == -1) {
            perror("select() error in uart_posix_read_cpp");
            return -1;
        } else if (retval) { // Data is available
            bytes_read = read(uart_fd_cpp, buffer + total_read, length - total_read);
            if (bytes_read > 0) {
                total_read += bytes_read;
            } else if (bytes_read == 0) { // Should not happen if select indicated data
                break; 
            } else { // bytes_read < 0
                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
                     continue; // Should not happen with select, but good practice
                 }
                perror("UART (cpp) read error after select");
                return -1;
            }
        } else { // Timeout
            // std::cout << "Read timeout in uart_posix_read_cpp" << std::endl;
            break;
        }
        // Re-arm select for next iteration if needed (e.g. if read didn't get all 'length' bytes)
        FD_ZERO(&read_fds);
        FD_SET(uart_fd_cpp, &read_fds);
        // Timeout for subsequent reads should be adjusted or be very short
        // For simplicity, if first select times out, we assume overall timeout.
        // If first read gets some data but not all, subsequent reads should be fast.
        // A more robust approach would re-calculate tv based on remaining time.
        // For now, if we got some data, we try to read the rest with a very short timeout.
        if (total_read > 0 && total_read < length) {
            tv.tv_sec = 0;
            tv.tv_usec = 50000; // 50ms for subsequent reads
        }
    }
    return total_read;
}

int uart_posix_open_cpp(const char* port_name, int baud_rate_val) {
    uart_fd_cpp = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_fd_cpp == -1) {
        perror("Error opening serial port (cpp)");
        return -1;
    }
    fcntl(uart_fd_cpp, F_SETFL, 0); // Clear O_NDELAY for blocking behavior by default

    struct termios options;
    tcgetattr(uart_fd_cpp, &options);

    speed_t baud;
    switch (baud_rate_val) {
        case 9600:   baud = B9600;   break;
        case 19200:  baud = B19200;  break;
        case 38400:  baud = B38400;  break;
        case 57600:  baud = B57600;  break;
        case 115200: baud = B115200; break;
        default:
            std::cerr << "Unsupported baud rate: " << baud_rate_val << std::endl;
            close(uart_fd_cpp);
            uart_fd_cpp = -1;
            return -1;
    }
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag |= CREAD | CLOCAL;

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;

    // For select-based timeout, VMIN and VTIME are less critical but set them sanely.
    options.c_cc[VMIN] = 0;  // Non-blocking read
    options.c_cc[VTIME] = 1; // 0.1s timeout (used if select is not used or as a fallback)

    tcflush(uart_fd_cpp, TCIFLUSH);
    if (tcsetattr(uart_fd_cpp, TCSANOW, &options) != 0) {
        perror("Error setting terminal options (cpp)");
        close(uart_fd_cpp);
        uart_fd_cpp = -1;
        return -1;
    }
    return 0;
}

void uart_posix_close_cpp() {
    if (uart_fd_cpp != -1) {
        close(uart_fd_cpp);
        uart_fd_cpp = -1;
    }
}

void print_system_params_cpp(const r502a_system_params_t& params) {
    std::cout << "System Parameters (C++ Example):\n"
              << "  Status Register:         0x" << std::hex << std::setw(4) << std::setfill('0') << params.status_register << "\n"
              << "  System Identifier Code:  0x" << std::hex << std::setw(4) << std::setfill('0') << params.system_identifier_code << "\n"
              << "  Finger Library Size:     " << std::dec << params.finger_library_size << "\n"
              << "  Security Level:          " << std::dec << params.security_level << "\n"
              << "  Device Address:          0x" << std::hex << std::setw(8) << std::setfill('0') << params.device_address << "\n"
              << "  Data Packet Size Code:   " << std::dec << params.data_packet_size_code
              << " (Actual: " << (32 * (1 << params.data_packet_size_code)) << " bytes)\n"
              << "  Baud Rate N:             " << std::dec << params.baud_rate_N
              << " (Actual: " << (params.baud_rate_N * 9600) << " bps)\n" << std::endl;
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <serial_port> <command> [args...]\n"
                  << "Commands:\n"
                  << "  handshake\n"
                  << "  readparams\n"
                  << "  verifypwd <password_hex>\n"
                  << "  getimage (calls FingerprintSensor::generateImage)\n"
                  << "  img2tz <buffer_id(1 or 2)> (calls FingerprintSensor::imageToTemplate)\n"
                  << "  createtpl (calls FingerprintSensor::createTemplate)\n"
                  << "  storetpl <buffer_id(1 or 2)> <page_id> (calls FingerprintSensor::storeTemplate)\n"
                  << "  setled <ctrl> <speed> <color> <count> (configures Aura LED; e.g., setled 1 200 2 0 for blue breathing)\n"
                  << "  empty (erase all stored fingerprints from the device)\n"
                  // Add more commands
                  << std::endl;
        return 1;
    }

    const char* port = argv[1];
    const char* command_str = argv[2];

    if (uart_posix_open_cpp(port, 57600) != 0) {
        return 1;
    }

    Grow::FingerprintSensor sensor(R502A_DEFAULT_ADDRESS, uart_posix_write_cpp, uart_posix_read_cpp);
    
    uint8_t ret = sensor.init(); // Initialize (checks UART functions)
    if (ret != R502A_CONF_OK) {
        std::cerr << "Failed to initialize FingerprintSensor object: 0x"
                  << std::hex << (int)ret << " (" << Grow::FingerprintSensor::errorCodeToString(ret) << ")" << std::endl;
        uart_posix_close_cpp();
        return 1;
    }

    std::cout << "FingerprintSensor object initialized. Executing command: " << command_str << std::endl;

    if (strcmp(command_str, "handshake") == 0) {
        ret = sensor.handshake();
        std::cout << "Handshake result: 0x" << std::hex << (int)ret
                  << " (" << Grow::FingerprintSensor::errorCodeToString(ret) << ")" << std::endl;
    } else if (strcmp(command_str, "readparams") == 0) {
        r502a_system_params_t params; // Using the C struct
        ret = sensor.readSystemParameters(params);
        std::cout << "Read System Parameters result: 0x" << std::hex << (int)ret
                  << " (" << Grow::FingerprintSensor::errorCodeToString(ret) << ")" << std::endl;
        if (ret == R502A_CONF_OK) {
            print_system_params_cpp(params);
        }
    } else if (strcmp(command_str, "verifypwd") == 0) {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " " << port << " verifypwd <password_hex_4_bytes>\n";
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint32_t password = (uint32_t)strtoul(argv[3], NULL, 16);
            std::cout << "Verifying password: 0x" << std::hex << std::setw(8) << std::setfill('0') << password << std::endl;
            ret = sensor.verifyPassword(password);
            std::cout << "Verify Password result: 0x" << std::hex << (int)ret
                      << " (" << Grow::FingerprintSensor::errorCodeToString(ret) << ")" << std::endl;
        }
    } else if (strcmp(command_str, "getimage") == 0) {
        std::cout << "Attempting to generate image (calls FingerprintSensor::generateImage)..." << std::endl;
        uint8_t sensor_confirmation_code;
        uint8_t driver_status = sensor.generateImage(&sensor_confirmation_code);
        if (driver_status == R502A_CONF_OK) {
            std::cout << "Generate Image - Sensor response: 0x" << std::hex << (int)sensor_confirmation_code
                      << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
            ret = sensor_confirmation_code;
        } else {
            std::cout << "Generate Image - Driver error: 0x" << std::hex << (int)driver_status
                      << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status) << ")" << std::endl;
            ret = driver_status;
        }
    } else if (strcmp(command_str, "img2tz") == 0) {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " " << port << " img2tz <buffer_id(1 or 2)>\n";
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint8_t buffer_id = (uint8_t)atoi(argv[3]);
            std::cout << "Converting image to template in buffer " << (int)buffer_id << " (calls FingerprintSensor::imageToTemplate)...\n";
            uint8_t sensor_confirmation_code;
            uint8_t driver_status = sensor.imageToTemplate(buffer_id, &sensor_confirmation_code);
            if (driver_status == R502A_CONF_OK) {
                std::cout << "Image to Template - Sensor response: 0x" << std::hex << (int)sensor_confirmation_code
                          << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                ret = sensor_confirmation_code;
            } else {
                std::cout << "Image to Template - Driver error: 0x" << std::hex << (int)driver_status
                          << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status) << ")" << std::endl;
                ret = driver_status;
            }
        }
    } else if (strcmp(command_str, "createtpl") == 0) {
        std::cout << "Attempting to create template (combines CharBuffer1 & 2, calls FingerprintSensor::createTemplate)..." << std::endl;
        uint8_t sensor_confirmation_code;
        uint8_t driver_status = sensor.createTemplate(&sensor_confirmation_code);
        if (driver_status == R502A_CONF_OK) {
            std::cout << "Create Template - Sensor response: 0x" << std::hex << (int)sensor_confirmation_code
                      << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
            ret = sensor_confirmation_code;
        } else {
            std::cout << "Create Template - Driver error: 0x" << std::hex << (int)driver_status
                      << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status) << ")" << std::endl;
            ret = driver_status;
        }
    } else if (strcmp(command_str, "storetpl") == 0) {
        if (argc < 5) {
            std::cerr << "Usage: " << argv[0] << " " << port << " storetpl <buffer_id(1 or 2)> <page_id>\n";
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint8_t buffer_id = (uint8_t)atoi(argv[3]);
            uint16_t page_id = (uint16_t)atoi(argv[4]);
            std::cout << "Storing template from buffer " << (int)buffer_id << " to page " << page_id
                      << " (calls FingerprintSensor::storeTemplate)...\n";
            uint8_t sensor_confirmation_code;
            uint8_t driver_status = sensor.storeTemplate(buffer_id, page_id, &sensor_confirmation_code);
            if (driver_status == R502A_CONF_OK) {
                std::cout << "Store Template - Sensor response: 0x" << std::hex << (int)sensor_confirmation_code
                          << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                ret = sensor_confirmation_code;
            } else {
                std::cout << "Store Template - Driver error: 0x" << std::hex << (int)driver_status
                          << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status) << ")" << std::endl;
                ret = driver_status;
            }
        }
    } else if (strcmp(command_str, "setled") == 0) {
        if (argc < 7) { // command + 4 args = 5. argv[0] + port + command + 4 args = 7
            std::cerr << "Usage: " << argv[0] << " " << port << " setled <ctrl_code> <speed> <color_index> <count>\n"
                      << "  ctrl_code: 1=breathing, 2=flashing, 3=on, 4=off, 5=gradual_on, 6=gradual_off (see FingerprintSensor::LED_CTRL_... constants)\n"
                      << "  speed: 0-255 (effect speed)\n"
                      << "  color_index: 1=red, 2=blue, 3=purple, etc. (see FingerprintSensor::LED_COLOR_... constants)\n"
                      << "  count: 0-255 (0 for infinite for breathing/flashing)\n";
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint8_t ctrl = (uint8_t)atoi(argv[3]);
            uint8_t speed = (uint8_t)atoi(argv[4]);
            uint8_t color = (uint8_t)atoi(argv[5]);
            uint8_t count = (uint8_t)atoi(argv[6]);
            std::cout << "Setting LED: Ctrl=0x" << std::hex << (int)ctrl
                      << ", Speed=" << std::dec << (int)speed
                      << ", Color=0x" << std::hex << (int)color
                      << ", Count=" << std::dec << (int)count << std::endl;

            uint8_t sensor_confirmation_code;
            uint8_t driver_status = sensor.setAuraLedConfig(ctrl, speed, color, count, &sensor_confirmation_code);
            
            if (driver_status == R502A_CONF_OK) {
                std::cout << "Set LED - Sensor response: 0x" << std::hex << (int)sensor_confirmation_code
                          << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                ret = sensor_confirmation_code;
            } else {
                std::cout << "Set LED - Driver error: 0x" << std::hex << (int)driver_status
                          << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status) << ")" << std::endl;
                ret = driver_status;
            }
        }
    } else if (strcmp(command_str, "empty") == 0) {
        std::cout << "Erasing all stored fingerprints from the device..." << std::endl;
        uint8_t driver_status = sensor.emptyFingerprintLibrary();
        if (driver_status == R502A_CONF_OK) {
            std::cout << "All fingerprints erased successfully." << std::endl;
            ret = R502A_CONF_OK;
        } else {
            std::cerr << "Failed to erase fingerprints: 0x" << std::hex << (int)driver_status
                      << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status) << ")" << std::endl;
            ret = driver_status;
        }
    } else if (strcmp(command_str, "enroll") == 0) {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " " << port << " enroll <page_id>\n";
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint16_t page_id = (uint16_t)atoi(argv[3]);
            std::cout << "Starting interactive enrollment for Page ID " << page_id << "." << std::endl;

            uint8_t sensor_confirmation_code;
            uint8_t driver_status;

            // --- First Scan ---
            std::cout << "Step 1: Place your finger on the sensor for the FIRST scan, then press Enter." << std::endl;
            sensor.setAuraLedConfig(Grow::FingerprintSensor::LED_CTRL_ON, 0, Grow::FingerprintSensor::LED_COLOR_BLUE, 1, &sensor_confirmation_code);
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Capturing first image..." << std::endl;
            driver_status = sensor.generateImage(&sensor_confirmation_code);
            if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                std::cerr << "Enrollment failed: GetImage (1) - Driver: 0x"
                          << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                          << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
            } else {
                std::cout << "First image captured successfully. Generating template for CharBuffer1..." << std::endl;
                driver_status = sensor.imageToTemplate(0x01, &sensor_confirmation_code);
                if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                    std::cerr << "Enrollment failed: Img2Tz (1) - Driver: 0x"
                              << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                              << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                    ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                } else {
                    std::cout << "Template for CharBuffer1 generated. Remove finger." << std::endl;
                    sensor.setAuraLedConfig(Grow::FingerprintSensor::LED_CTRL_OFF, 0, 0, 0, &sensor_confirmation_code);
                    sleep(2);

                    // --- Second Scan ---
                    std::cout << "Step 2: Place the SAME finger on the sensor for the SECOND scan, then press Enter." << std::endl;
                    sensor.setAuraLedConfig(Grow::FingerprintSensor::LED_CTRL_ON, 0, Grow::FingerprintSensor::LED_COLOR_BLUE, 1, &sensor_confirmation_code);
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cout << "Capturing second image..." << std::endl;
                    driver_status = sensor.generateImage(&sensor_confirmation_code);
                    if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                        std::cerr << "Enrollment failed: GetImage (2) - Driver: 0x"
                                  << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                                  << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                        ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                    } else {
                        std::cout << "Second image captured successfully. Generating template for CharBuffer2..." << std::endl;
                        driver_status = sensor.imageToTemplate(0x02, &sensor_confirmation_code);
                        if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                            std::cerr << "Enrollment failed: Img2Tz (2) - Driver: 0x"
                                      << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                                      << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                            ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                        } else {
                            std::cout << "Template for CharBuffer2 generated. Creating combined template..." << std::endl;
                            sensor.setAuraLedConfig(Grow::FingerprintSensor::LED_CTRL_OFF, 0, 0, 0, &sensor_confirmation_code);
                            driver_status = sensor.createTemplate(&sensor_confirmation_code);
                            if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                                std::cerr << "Enrollment failed: CreateTemplate - Driver: 0x"
                                          << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                                          << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                                if (sensor_confirmation_code == R502A_CONF_FINGER_NOMATCH) {
                                    std::cerr << "Note: The two fingerprints did not match. Please try again with the same finger." << std::endl;
                                }
                                ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                            } else {
                                std::cout << "Combined template created successfully. Storing to Page ID " << page_id << "..." << std::endl;
                                driver_status = sensor.storeTemplate(0x01, page_id, &sensor_confirmation_code);
                                if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                                    std::cerr << "Enrollment failed: StoreTemplate - Driver: 0x"
                                              << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                                              << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                                    ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                                } else {
                                    std::cout << "Fingerprint successfully enrolled and stored at Page ID " << page_id << "!" << std::endl;
                                    ret = R502A_CONF_OK;
                                    // Flash green LED for success
                                    sensor.setAuraLedConfig(Grow::FingerprintSensor::LED_CTRL_FLASHING, 150, Grow::FingerprintSensor::LED_COLOR_GREEN, 3, &sensor_confirmation_code);
                                }
                            }
                        }
                    }
                }
            }
            // Check the return code, if it's an error, flash red LED
            if (ret != R502A_CONF_OK) {
                sensor.setAuraLedConfig(Grow::FingerprintSensor::LED_CTRL_FLASHING, 150, Grow::FingerprintSensor::LED_COLOR_RED, 3, &sensor_confirmation_code);
                std::cerr << "Enrollment process failed with code: 0x" << std::hex << (int)ret
                          << " (" << Grow::FingerprintSensor::errorCodeToString(ret) << ")" << std::endl;
            } else {
                std::cout << "Enrollment completed successfully." << std::endl;
            }
        }
    } else if (strcmp(command_str, "verify") == 0) {
        std::cout << "Starting fingerprint verification..." << std::endl;
        uint8_t sensor_confirmation_code;
        uint8_t driver_status = sensor.generateImage(&sensor_confirmation_code);
        if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
            std::cerr << "Verification failed: GetImage - Driver: 0x"
                      << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                      << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
            ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
        } else {
            std::cout << "Image captured successfully. Converting to template..." << std::endl;
            driver_status = sensor.imageToTemplate(0x01, &sensor_confirmation_code);
            if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                std::cerr << "Verification failed: Img2Tz - Driver: 0x"
                          << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                          << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
            } else {
                std::cout << "Template generated successfully. Searching in library..." << std::endl;
                for(uint16_t page_id = 0; page_id < 200; page_id += 10) {
                    uint8_t buffer_id = 0x01;
                    r502a_search_result_t search_result;
                    driver_status = sensor.searchFingerprint(buffer_id, page_id, page_id + 10, search_result);
                    if (driver_status == R502A_CONF_OK) {
                        std::cout << "Fingerprint verified successfully! Found at Page ID " << search_result.page_id << "." << std::endl;
                        std::cout << "Match Score: " << search_result.match_score << std::endl;
                        ret = R502A_CONF_OK;
                        sensor.setAuraLedConfig(Grow::FingerprintSensor::LED_CTRL_FLASHING, 150, Grow::FingerprintSensor::LED_COLOR_GREEN, 3, &sensor_confirmation_code);
                        break; // Exit loop on successful verification
                    } else {
                        std::cerr << "Verification failed: Search - Driver: 0x"
                                << std::hex << (int)driver_status << " (" << Grow::FingerprintSensor::errorCodeToString(driver_status)
                                << "), Sensor: 0x" << (int)sensor_confirmation_code << " (" << Grow::FingerprintSensor::errorCodeToString(sensor_confirmation_code) << ")" << std::endl;
                        ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                    }
                }
            }
        }
        // Check the return code, if it's an error, flash red LED
        if (ret != R502A_CONF_OK) {
            sensor.setAuraLedConfig(Grow::FingerprintSensor::LED_CTRL_FLASHING, 150, Grow::FingerprintSensor::LED_COLOR_RED, 3, &sensor_confirmation_code);
            std::cerr << "Verification process failed with code: 0x" << std::hex << (int)ret
                      << " (" << Grow::FingerprintSensor::errorCodeToString(ret) << ")" << std::endl;
        } else {
            std::cout << "Verification completed successfully." << std::endl;
        }
    }
    // Add other command handlers here
    else {
        std::cerr << "Unknown command: " << command_str << std::endl;
        ret = R502A_ERR_INVALID_ARGS; // Using a driver error code for unknown app command
    }

    uart_posix_close_cpp();
    // Adjust return logic: 0 for R502A_CONF_OK, 1 for any other driver/sensor code.
    // The initial `ret = 0xFF` or `ret = 1` for arg errors should also lead to exit 1.
    if (ret == R502A_ERR_INVALID_ARGS && (strcmp(command_str, "verifypwd") == 0 || strcmp(command_str, "img2tz") == 0 || strcmp(command_str, "storetpl") == 0 || strcmp(command_str, "setled") == 0 || strcmp(command_str, "unknown") == 0) ) {
         // For arg errors detected in main before calling driver, or unknown command
         return 1;
    }
    return (ret == R502A_CONF_OK) ? 0 : 1;
}