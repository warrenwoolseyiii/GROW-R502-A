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
        fprintf(stderr, "  getimage\n");
        fprintf(stderr, "  genchar <buffer_id(1-6)>\n");
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
        fprintf(stderr, "Failed to initialize R502-A driver: 0x%02X\n", ret);
        uart_posix_close();
        return 1;
    }

    printf("R502-A driver initialized. Executing command: %s\n", command);

    if (strcmp(command, "handshake") == 0) {
        ret = r502a_handshake(&sensor_handle);
        printf("Handshake result: 0x%02X (%s)\n", ret, (ret == R502A_CONF_OK) ? "OK" : "FAIL");
    } else if (strcmp(command, "readparams") == 0) {
        r502a_system_params_t params;
        ret = r502a_read_system_parameters(&sensor_handle, &params);
        printf("Read System Parameters result: 0x%02X (%s)\n", ret, (ret == R502A_CONF_OK) ? "OK" : "FAIL");
        if (ret == R502A_CONF_OK) {
            print_system_params(&params);
        }
    } else if (strcmp(command, "verifypwd") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s verifypwd <password_hex_4_bytes>\n", argv[0], port);
            ret = 0xFF; // Indicate error
        } else {
            uint32_t password = (uint32_t)strtoul(argv[3], NULL, 16);
            printf("Verifying password: 0x%08X\n", password);
            ret = r502a_verify_password(&sensor_handle, password);
            printf("Verify Password result: 0x%02X (%s)\n", ret, (ret == R502A_CONF_OK) ? "OK" : (ret == R502A_CONF_PWD_FAIL ? "WRONG_PWD" : "FAIL"));
        }
    } else if (strcmp(command, "getimage") == 0) {
        printf("Attempting to get image (GetImageEx)...\n");
        ret = r502a_get_image_extended(&sensor_handle);
        printf("Get Image Extended result: 0x%02X (", ret);
        switch(ret) {
            case R502A_CONF_OK: printf("OK"); break;
            case R502A_CONF_NO_FINGER: printf("NO_FINGER"); break;
            case R502A_CONF_FAIL_ENROLL: printf("FAIL_COLLECT"); break; // 0x03 is also fail to enroll
            case R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT: printf("POOR_IMAGE_QUALITY"); break; // 0x07 is also this for GetImageEx
            default: printf("FAIL/OTHER"); break;
        }
        printf(")\n");
    } else if (strcmp(command, "genchar") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s genchar <buffer_id(1-6)>\n", argv[0], port);
            ret = 0xFF;
        } else {
            uint8_t buffer_id = (uint8_t)atoi(argv[3]);
            if (buffer_id < 1 || buffer_id > 6) {
                fprintf(stderr, "Invalid buffer_id. Must be 1-6.\n");
                ret = 0xFF;
            } else {
                printf("Generating character file in buffer %u...\n", buffer_id);
                ret = r502a_generate_character_file(&sensor_handle, buffer_id);
                printf("Generate Character File result: 0x%02X (", ret);
                 switch(ret) {
                    case R502A_CONF_OK: printf("OK"); break;
                    case R502A_CONF_FAIL_GEN_CHAR_DISORDERLY: printf("FAIL_DISORDERLY_IMG"); break;
                    case R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT: printf("FAIL_SMALL_POINT_IMG"); break;
                    case R502A_CONF_FAIL_GEN_IMAGE_NO_PRIMARY: printf("FAIL_NO_PRIMARY_IMG"); break;
                    default: printf("FAIL/OTHER"); break;
                }
                printf(")\n");
            }
        }
    }
    // Add other command handlers here:
    // else if (strcmp(command, "regmodel") == 0) { ... }
    // else if (strcmp(command, "store") == 0) { ... }
    // else if (strcmp(command, "search") == 0) { ... }
    // else if (strcmp(command, "delete") == 0) { ... }
    // else if (strcmp(command, "empty") == 0) { ... }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        ret = 1;
    }

    uart_posix_close();
    return (ret == R502A_CONF_OK || ret == 0) ? 0 : 1; // Ensure 0 for shell success on OK
}