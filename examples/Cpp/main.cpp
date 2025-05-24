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
                  << "  getimage\n"
                  << "  genchar <buffer_id(1-6)>\n"
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
        std::cout << "Attempting to get image (GetImageEx)..." << std::endl;
        ret = sensor.getImageExtended();
        std::cout << "Get Image Extended result: 0x" << std::hex << (int)ret
                  << " (" << Grow::FingerprintSensor::errorCodeToString(ret) << ")" << std::endl;
    } else if (strcmp(command_str, "genchar") == 0) {
        if (argc < 4) {
            std::cerr << "Usage: " << argv[0] << " " << port << " genchar <buffer_id(1-6)>\n";
            ret = R502A_ERR_INVALID_ARGS;
        } else {
            uint8_t buffer_id = (uint8_t)atoi(argv[3]);
            // The C++ wrapper calls the C function which now validates buffer_id
            std::cout << "Generating character file in buffer " << (int)buffer_id << "...\n";
            ret = sensor.generateCharacterFile(buffer_id);
            std::cout << "Generate Character File result: 0x" << std::hex << (int)ret
                      << " (" << Grow::FingerprintSensor::errorCodeToString(ret) << ")" << std::endl;
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
    if (ret == R502A_ERR_INVALID_ARGS && (strcmp(command_str, "verifypwd") == 0 || strcmp(command_str, "genchar") == 0 || strcmp(command_str, "unknown") == 0) ) {
         // For arg errors detected in main before calling driver, or unknown command
         return 1;
    }
    return (ret == R502A_CONF_OK) ? 0 : 1;
}