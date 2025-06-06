#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <termios.h> // For POSIX terminal control
#include <unistd.h>  // For read/write/close
#include <fcntl.h>   // For file control options (O_RDWR, O_NOCTTY)
#include <errno.h>   // For error number
#include <time.h>    // For time()

#include "../../lib/C/r502a_driver.h"

// --- POSIX UART Implementation ---
static int uart_fd = -1; // File descriptor for the serial port

int uart_posix_open(const char* port_name, int baud_rate_val) {
    uart_fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_fd == -1) {
        perror("Error opening serial port");
        return -1;
    }

    fcntl(uart_fd, F_SETFL, 0); // Clear O_NDELAY for blocking reads

    struct termios options;
    tcgetattr(uart_fd, &options);

    // Set baud rate (assuming B57600 for R502-A default)
    // You might need a mapping from int baud_rate_val to speed_t constants
    speed_t baud;
    switch (baud_rate_val) {
        case 9600:   baud = B9600;   break;
        case 19200:  baud = B19200;  break;
        case 38400:  baud = B38400;  break;
        case 57600:  baud = B57600;  break;
        case 115200: baud = B115200; break;
        default:
            fprintf(stderr, "Unsupported baud rate: %d\n", baud_rate_val);
            close(uart_fd);
            uart_fd = -1;
            return -1;
    }
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    // Set 8N1 (8 data bits, no parity, 1 stop bit)
    options.c_cflag &= ~PARENB; // No parity
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;  // Clear data size bits
    options.c_cflag |= CS8;     // 8 data bits
    options.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines

    // Raw input
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    // Raw output
    options.c_oflag &= ~OPOST;

    // Timeouts
    options.c_cc[VMIN] = 0;  // Read at least 0 bytes
    options.c_cc[VTIME] = 10; // Wait up to 1 second (10 * 0.1s) for data

    tcflush(uart_fd, TCIFLUSH); // Flush RX buffer
    if (tcsetattr(uart_fd, TCSANOW, &options) != 0) {
        perror("Error setting terminal options");
        close(uart_fd);
        uart_fd = -1;
        return -1;
    }
    return 0;
}

void uart_posix_close() {
    if (uart_fd != -1) {
        close(uart_fd);
        uart_fd = -1;
    }
}

int uart_posix_write(const uint8_t* data, uint16_t length) {
    if (uart_fd == -1) return -1;
    ssize_t written = write(uart_fd, data, length);
    if (written < 0) {
        perror("UART write error");
        return -1;
    }
    if (written != length) {
        fprintf(stderr, "UART write incomplete: wrote %zd of %d bytes\n", written, length);
        return -1; // Or handle partial writes if necessary
    }
    return 0; // Success
}

int uart_posix_read(uint8_t* buffer, uint16_t length, uint32_t timeout_ms) {
    if (uart_fd == -1) return -1;

    // The VTIME in termios is in tenths of a second.
    // For more precise timeout, select() or poll() would be better.
    // This implementation uses blocking read with termios timeout.
    // For simplicity, we'll rely on the VTIME set during open.
    // A more robust implementation might adjust VTIME here or use select/poll.

    ssize_t bytes_read = 0;
    uint16_t total_read = 0;
    uint32_t start_time = time(NULL); // Simple timeout mechanism

    fcntl(uart_fd, F_SETFL, 0); // Ensure blocking read for this attempt

    struct termios options;
    tcgetattr(uart_fd, &options);
    options.c_cc[VMIN] = 0; // Non-blocking for individual read calls if length > 1
    options.c_cc[VTIME] = (timeout_ms + 99) / 100; // Convert ms to 0.1s units, rounding up
    if (options.c_cc[VTIME] == 0 && timeout_ms > 0) options.c_cc[VTIME] = 1; // Minimum 0.1s if timeout > 0
    tcsetattr(uart_fd, TCSANOW, &options);


    while (total_read < length) {
        if ((time(NULL) - start_time) * 1000 > timeout_ms && timeout_ms > 0) {
            // printf("Read timeout occurred.\n");
            break; // Timeout
        }
        bytes_read = read(uart_fd, buffer + total_read, length - total_read);
        if (bytes_read > 0) {
            total_read += bytes_read;
        } else if (bytes_read == 0) {
            // No data, continue polling or break if timeout logic is external
            if (timeout_ms == 0) break; // If no timeout, and no data, break.
            usleep(10000); // Sleep for 10ms before trying again
        } else { // bytes_read < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                 if (timeout_ms == 0) break; // Non-blocking and no data
                 usleep(10000); // Sleep for 10ms
                 continue;
            }
            perror("UART read error");
            return -1; // Error
        }
    }
    return total_read;
}


void print_system_params(const r502a_system_params_t* params) {
    printf("System Parameters:\n");
    printf("  Status Register:         0x%04X\n", params->status_register);
    printf("  System Identifier Code:  0x%04X\n", params->system_identifier_code);
    printf("  Finger Library Size:     %u\n", params->finger_library_size);
    printf("  Security Level:          %u\n", params->security_level);
    printf("  Device Address:          0x%08X\n", params->device_address);
    printf("  Data Packet Size Code:   %u (Actual: %u bytes)\n",
           params->data_packet_size_code,
           32 * (1 << params->data_packet_size_code)); // 0->32, 1->64, 2->128, 3->256
    printf("  Baud Rate N:             %u (Actual: %u bps)\n",
           params->baud_rate_N, params->baud_rate_N * 9600);
}

void print_hex(const char* prefix, const uint8_t* data, size_t len) {
    printf("%s", prefix);
    for(size_t i = 0; i < len; ++i) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <serial_port> <command> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  handshake\n");
        fprintf(stderr, "  readparams\n");
        fprintf(stderr, "  verifypwd <password_hex>\n");
        fprintf(stderr, "  getimage (calls r502a_generate_image)\n");
        fprintf(stderr, "  img2tz <buffer_id(1 or 2)> (calls r502a_image_to_template)\n");
        fprintf(stderr, "  createtpl (calls r502a_create_template - combines CharBuffer1 & 2)\n");
        fprintf(stderr, "  storetpl <buffer_id(1 or 2)> <page_id> (calls r502a_store_template)\n");
        fprintf(stderr, "  setled <ctrl> <speed> <color> <count> (configures Aura LED; e.g., setled 1 200 2 0 for blue breathing)\n");
        fprintf(stderr, "  enroll <page_id> (interactive enrollment to specified page ID)\n");
        fprintf(stderr, "  verify (verify fingerprint against stored templates)\n");
        fprintf(stderr, "  empty (erase all stored fingerprints from the device)\n");
        // Add more commands as they are tested
        return 1;
    }

    const char* port = argv[1];
    const char* command = argv[2];

    if (uart_posix_open(port, 57600) != 0) { // Default R502-A baud rate
        return 1;
    }

    r502a_handle_t sensor_handle;
    uint8_t ret = r502a_init(&sensor_handle, R502A_DEFAULT_ADDRESS, uart_posix_write, uart_posix_read);
    if (ret != R502A_CONF_OK) {
        fprintf(stderr, "Failed to initialize R502-A driver: 0x%02X (%s)\n", ret, r502a_error_code_to_string(ret));
        uart_posix_close();
        return 1;
    }

    printf("R502-A driver initialized. Executing command: %s\n", command);

    if (strcmp(command, "handshake") == 0) {
        ret = r502a_handshake(&sensor_handle);
        printf("Handshake result: 0x%02X (%s)\n", ret, r502a_error_code_to_string(ret));
    } else if (strcmp(command, "readparams") == 0) {
        r502a_system_params_t params;
        ret = r502a_read_system_parameters(&sensor_handle, &params);
        printf("Read System Parameters result: 0x%02X (%s)\n", ret, r502a_error_code_to_string(ret));
        if (ret == R502A_CONF_OK) {
            print_system_params(&params);
        }
    } else if (strcmp(command, "verifypwd") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s verifypwd <password_hex_4_bytes>\n", argv[0], port);
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint32_t password = (uint32_t)strtoul(argv[3], NULL, 16);
            printf("Verifying password: 0x%08X\n", password);
            ret = r502a_verify_password(&sensor_handle, password);
            printf("Verify Password result: 0x%02X (%s)\n", ret, r502a_error_code_to_string(ret));
        }
    } else if (strcmp(command, "getimage") == 0) {
        printf("Attempting to generate image (calls r502a_generate_image)...\n");
        uint8_t sensor_confirmation_code;
        uint8_t driver_status = r502a_generate_image(&sensor_handle, &sensor_confirmation_code);
        if (driver_status == R502A_CONF_OK) { // Driver call successful
            printf("Generate Image - Sensor response: 0x%02X (%s)\n", sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
            ret = sensor_confirmation_code; // Use sensor's code for overall status
        } else { // Driver call failed
            printf("Generate Image - Driver error: 0x%02X (%s)\n", driver_status, r502a_error_code_to_string(driver_status));
            ret = driver_status; // Use driver's error code
        }
    } else if (strcmp(command, "img2tz") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s img2tz <buffer_id(1 or 2)>\n", argv[0], port);
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint8_t buffer_id = (uint8_t)atoi(argv[3]);
            // The driver function r502a_image_to_template checks buffer_id validity (0x01 or 0x02).
            printf("Converting image to template in buffer %u (calls r502a_image_to_template)...\n", buffer_id);
            uint8_t sensor_confirmation_code;
            uint8_t driver_status = r502a_image_to_template(&sensor_handle, buffer_id, &sensor_confirmation_code);
            if (driver_status == R502A_CONF_OK) { // Driver call successful
                printf("Image to Template - Sensor response: 0x%02X (%s)\n", sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                ret = sensor_confirmation_code; // Use sensor's code for overall status
            } else { // Driver call failed
                printf("Image to Template - Driver error: 0x%02X (%s)\n", driver_status, r502a_error_code_to_string(driver_status));
                ret = driver_status; // Use driver's error code
            }
        }
    } else if (strcmp(command, "createtpl") == 0) {
        printf("Attempting to create template (combines CharBuffer1 & 2, calls r502a_create_template)...\n");
        uint8_t sensor_confirmation_code;
        uint8_t driver_status = r502a_create_template(&sensor_handle, &sensor_confirmation_code);
        if (driver_status == R502A_CONF_OK) {
            printf("Create Template - Sensor response: 0x%02X (%s)\n", sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
            ret = sensor_confirmation_code;
        } else {
            printf("Create Template - Driver error: 0x%02X (%s)\n", driver_status, r502a_error_code_to_string(driver_status));
            ret = driver_status;
        }
    } else if (strcmp(command, "storetpl") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s %s storetpl <buffer_id(1 or 2)> <page_id>\n", argv[0], port);
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint8_t buffer_id = (uint8_t)atoi(argv[3]);
            uint16_t page_id = (uint16_t)atoi(argv[4]);
            printf("Storing template from buffer %u to page %u (calls r502a_store_template)...\n", buffer_id, page_id);
            uint8_t sensor_confirmation_code;
            uint8_t driver_status = r502a_store_template(&sensor_handle, buffer_id, page_id, &sensor_confirmation_code);
            if (driver_status == R502A_CONF_OK) {
                printf("Store Template - Sensor response: 0x%02X (%s)\n", sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                ret = sensor_confirmation_code;
            } else {
                printf("Store Template - Driver error: 0x%02X (%s)\n", driver_status, r502a_error_code_to_string(driver_status));
                ret = driver_status;
            }
        }
    } else if (strcmp(command, "setled") == 0) {
        if (argc < 7) { // command + 4 args = 5. argv[0] + port + command + 4 args = 7
            fprintf(stderr, "Usage: %s %s setled <ctrl_code> <speed> <color_index> <count>\n", argv[0], port);
            fprintf(stderr, "  ctrl_code: 1=breathing, 2=flashing, 3=on, 4=off, 5=gradual_on, 6=gradual_off\n");
            fprintf(stderr, "  speed: 0-255 (effect speed)\n");
            fprintf(stderr, "  color_index: 1=red, 2=blue, 3=purple, 4=green, 5=yellow, 6=cyan, 7=white\n");
            fprintf(stderr, "  count: 0-255 (0 for infinite for breathing/flashing)\n");
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint8_t ctrl = (uint8_t)atoi(argv[3]);
            uint8_t speed = (uint8_t)atoi(argv[4]);
            uint8_t color = (uint8_t)atoi(argv[5]);
            uint8_t count = (uint8_t)atoi(argv[6]);
            printf("Setting LED: Ctrl=0x%02X, Speed=%u, Color=0x%02X, Count=%u\n", ctrl, speed, color, count);

            uint8_t sensor_confirmation_code;
            uint8_t driver_status = r502a_set_aura_led_config(&sensor_handle, ctrl, speed, color, count, &sensor_confirmation_code);
            
            if (driver_status == R502A_CONF_OK) {
                printf("Set LED - Sensor response: 0x%02X (%s)\n", sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                ret = sensor_confirmation_code;
            } else {
                printf("Set LED - Driver error: 0x%02X (%s)\n", driver_status, r502a_error_code_to_string(driver_status));
                ret = driver_status;
            }
        }
    } else if (strcmp(command, "enroll") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s enroll <page_id>\n", argv[0], port);
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint16_t page_id = (uint16_t)atoi(argv[3]);
            printf("Starting interactive enrollment for Page ID %u.\n", page_id);

            uint8_t sensor_confirmation_code;
            uint8_t driver_status;

            // --- First Scan ---
            printf("Step 1: Place your finger on the sensor for the FIRST scan, then press Enter.\n");
            // Turn on LED to indicate scanning
            r502a_set_aura_led_config(&sensor_handle, R502A_LED_CTRL_ON, 0, R502A_LED_COLOR_BLUE, 1, &sensor_confirmation_code);
            while(getchar()!='\n'); // Wait for Enter key
            printf("Capturing first image...\n");
            driver_status = r502a_generate_image(&sensor_handle, &sensor_confirmation_code);
            if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                fprintf(stderr, "Enrollment failed: GetImage (1) - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                        driver_status, r502a_error_code_to_string(driver_status),
                        sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
            } else {
                printf("First image captured successfully. Generating template for CharBuffer1...\n");
                driver_status = r502a_image_to_template(&sensor_handle, 0x01, &sensor_confirmation_code);
                if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                    fprintf(stderr, "Enrollment failed: Img2Tz (1) - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                            driver_status, r502a_error_code_to_string(driver_status),
                            sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                    ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                } else {
                    printf("Template for CharBuffer1 generated. Remove finger.\n");
                    // Turn off LED or indicate to remove finger
                    r502a_set_aura_led_config(&sensor_handle, R502A_LED_CTRL_OFF, 0, 0, 0, &sensor_confirmation_code);
                    sleep(2); // Give user time to remove finger

                    // --- Second Scan ---
                    printf("Step 2: Place the SAME finger on the sensor for the SECOND scan, then press Enter.\n");
                    r502a_set_aura_led_config(&sensor_handle, R502A_LED_CTRL_ON, 0, R502A_LED_COLOR_BLUE, 1, &sensor_confirmation_code);
                    while(getchar()!='\n'); // Wait for Enter key
                    printf("Capturing second image...\n");
                    driver_status = r502a_generate_image(&sensor_handle, &sensor_confirmation_code);
                    if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                        fprintf(stderr, "Enrollment failed: GetImage (2) - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                                driver_status, r502a_error_code_to_string(driver_status),
                                sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                        ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                    } else {
                        printf("Second image captured successfully. Generating template for CharBuffer2...\n");
                        driver_status = r502a_image_to_template(&sensor_handle, 0x02, &sensor_confirmation_code);
                        if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                            fprintf(stderr, "Enrollment failed: Img2Tz (2) - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                                    driver_status, r502a_error_code_to_string(driver_status),
                                    sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                            ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                        } else {
                            printf("Template for CharBuffer2 generated. Creating combined template...\n");
                            r502a_set_aura_led_config(&sensor_handle, R502A_LED_CTRL_OFF, 0, 0, 0, &sensor_confirmation_code);
                            driver_status = r502a_create_template(&sensor_handle, &sensor_confirmation_code);
                            if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                                fprintf(stderr, "Enrollment failed: CreateTemplate - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                                        driver_status, r502a_error_code_to_string(driver_status),
                                        sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                                if (sensor_confirmation_code == R502A_CONF_FINGER_NOMATCH) {
                                     fprintf(stderr, "Note: The two fingerprints did not match. Please try again with the same finger.\n");
                                }
                                ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                            } else {
                                printf("Combined template created successfully. Storing to Page ID %u...\n", page_id);
                                driver_status = r502a_store_template(&sensor_handle, 0x01, page_id, &sensor_confirmation_code); // Store from CharBuffer1
                                if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                                    fprintf(stderr, "Enrollment failed: StoreTemplate - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                                            driver_status, r502a_error_code_to_string(driver_status),
                                            sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                                    ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
                                } else {
                                    printf("Fingerprint successfully enrolled and stored at Page ID %u!\n", page_id);
                                    ret = R502A_CONF_OK;
                                    // Flash green LED for success
                                    r502a_set_aura_led_config(&sensor_handle, R502A_LED_CTRL_FLASHING, 150, R502A_LED_COLOR_GREEN, 3, &sensor_confirmation_code);
                                }
                            }
                        }
                    }
                }
            }

            // Check the return code, if it's an error, flash red LED
            if (ret != R502A_CONF_OK) {
                // Flash red LED for error
                r502a_set_aura_led_config(&sensor_handle, R502A_LED_CTRL_FLASHING, 150, R502A_LED_COLOR_RED, 3, &sensor_confirmation_code);
                fprintf(stderr, "Enrollment process failed with code: 0x%02X (%s)\n", ret, r502a_error_code_to_string(ret));
            } else {
                printf("Enrollment completed successfully.\n");
            }
        }
    } else if(strcmp(command, "verify") == 0) {
        printf("Starting fingerprint verification...\n");
        // Get the fingerprint image
        uint8_t sensor_confirmation_code;
        uint8_t driver_status = r502a_generate_image(&sensor_handle, &sensor_confirmation_code);
        if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
            fprintf(stderr, "Verification failed: GetImage - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                    driver_status, r502a_error_code_to_string(driver_status),
                    sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
            ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
        } else {
            printf("Image captured successfully. Converting to template...\n");
            driver_status = r502a_image_to_template(&sensor_handle, 0x01, &sensor_confirmation_code);
            if (driver_status != R502A_CONF_OK || sensor_confirmation_code != R502A_CONF_OK) {
                fprintf(stderr, "Verification failed: Img2Tz - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                        driver_status, r502a_error_code_to_string(driver_status),
                        sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                ret = (driver_status != R502A_CONF_OK) ? driver_status : sensor_confirmation_code;
            } else {
                printf("Template generated successfully. Searching in library...\n");
                for(uint16_t page_id = 0; page_id < 200; page_id += 10) {
                    uint8_t buffer_id = 0x01; // Use CharBuffer1 for verification
                    r502a_search_result_t search_result;
                    driver_status = r502a_search_fingerprint(&sensor_handle, buffer_id, page_id, page_id + 10, &search_result);
                    if (driver_status == R502A_CONF_OK) {
                        printf("Fingerprint verified successfully! Found at Page ID %u.\n", search_result.page_id);
                        printf("Match Score: %u\n", search_result.match_score);
                        ret = R502A_CONF_OK;
                        // Flash green LED for success
                        r502a_set_aura_led_config(&sensor_handle, R502A_LED_CTRL_FLASHING, 150, R502A_LED_COLOR_GREEN, 3, &sensor_confirmation_code);
                        break; // Exit loop on successful verification
                    } else {
                        fprintf(stderr, "Verification failed: Search - Driver: 0x%02X (%s), Sensor: 0x%02X (%s)\n",
                                driver_status, r502a_error_code_to_string(driver_status),
                                sensor_confirmation_code, r502a_error_code_to_string(sensor_confirmation_code));
                        ret = (driver_status != R502A_CONF_OK
                                ? driver_status : sensor_confirmation_code);
                    }
                }
            }
        }

        // Check the return code, if it's an error, flash red LED
        if (ret != R502A_CONF_OK) {
            // Flash red LED for error
            r502a_set_aura_led_config(&sensor_handle, R502A_LED_CTRL_FLASHING, 150, R502A_LED_COLOR_RED, 3, &sensor_confirmation_code);
            fprintf(stderr, "Verification process failed with code: 0x%02X (%s)\n", ret, r502a_error_code_to_string(ret));
        } else {
            printf("Verification completed successfully.\n");
        }
    } else if (strcmp(command, "empty") == 0) {
        printf("Erasing all stored fingerprints from the device...\n");
        uint8_t driver_status = r502a_empty_fingerprint_library(&sensor_handle);
        if (driver_status == R502A_CONF_OK) {
            printf("All fingerprints erased successfully.\n");
            ret = R502A_CONF_OK;
        } else {
            fprintf(stderr, "Failed to erase fingerprints: 0x%02X (%s)\n", driver_status, r502a_error_code_to_string(driver_status));
            ret = driver_status;
        }
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        ret = R502A_ERR_INVALID_ARGS; // Using a driver error code for unknown app command
    }

    uart_posix_close();
    // Adjust return logic: 0 for R502A_CONF_OK, 1 for any other driver/sensor code.
    // The initial `ret = 0xFF` or `ret = 1` for arg errors should also lead to exit 1.
    if (ret == R502A_ERR_INVALID_ARGS && (strcmp(command, "verifypwd") == 0 || strcmp(command, "img2tz") == 0 || strcmp(command, "storetpl") == 0 || strcmp(command, "setled") == 0 || strcmp(command, "enroll") == 0 || strcmp(command, "unknown") == 0) ) {
         // For arg errors detected in main before calling driver, or unknown command
         return 1;
    }
    return (ret == R502A_CONF_OK) ? 0 : 1;
}