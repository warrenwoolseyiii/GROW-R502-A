#ifndef R502A_DRIVER_H
#define R502A_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // Closes #ifdef __cplusplus from line 7
// --- Constants ---
// Packet Structure
#define R502A_PACKET_HEADER_HIGH 0xEF
#define R502A_PACKET_HEADER_LOW  0x01
#define R502A_DEFAULT_ADDRESS    0xFFFFFFFF

// Packet Identifiers (PID)
#define R502A_PID_COMMAND        0x01 // Command packet
#define R502A_PID_DATA           0x02 // Data packet
#define R502A_PID_ACK            0x07 // Acknowledge packet
#define R502A_PID_END_DATA       0x08 // End of Data packet

// Command Codes (InstructionTable from datasheet pg 9)
#define R502A_CMD_HANDSHAKE      0x40
#define R502A_CMD_READ_SYS_PARA  0x0F
#define R502A_CMD_VFY_PWD        0x13
#define R502A_CMD_GET_IMG_EX     0x28
#define R502A_CMD_GEN_CHAR       0x02
#define R502A_CMD_REG_MODEL      0x05
#define R502A_CMD_STORE          0x06
#define R502A_CMD_SEARCH         0x04
#define R502A_CMD_DELETE_CHAR    0x0C
#define R502A_CMD_EMPTY          0x0D
#define R502A_CMD_AURA_LED_CONFIG 0x35 // Aura LED Control
// ... (add more as needed)

// --- LED Control Constants (for AuraLedConfig 0x35) ---
// Control Codes (Ctrl)
#define R502A_LED_CTRL_BREATHING   0x01 // Breathing light
#define R502A_LED_CTRL_FLASHING    0x02 // Flashing light
#define R502A_LED_CTRL_ON          0x03 // Light always ON
#define R502A_LED_CTRL_OFF         0x04 // Light always OFF
#define R502A_LED_CTRL_GRADUAL_ON  0x05 // Light gradually ON
#define R502A_LED_CTRL_GRADUAL_OFF 0x06 // Light gradually OFF

// Color Indicies (Color)
#define R502A_LED_COLOR_RED        0x01
#define R502A_LED_COLOR_BLUE       0x02
#define R502A_LED_COLOR_PURPLE     0x03
#define R502A_LED_COLOR_GREEN      0x04
#define R502A_LED_COLOR_YELLOW     0x05
#define R502A_LED_COLOR_CYAN       0x06
#define R502A_LED_COLOR_WHITE      0x07

// Confirmation Codes (Datasheet pg 9-10)
#define R502A_CONF_OK            0x00 // Command execution complete
#define R502A_CONF_ERR_RECV      0x01 // Error when receiving data package
#define R502A_CONF_NO_FINGER     0x02 // No finger on the sensor
#define R502A_CONF_FAIL_ENROLL   0x03 // Fail to enroll the finger
#define R502A_CONF_FAIL_GEN_CHAR_DISORDERLY 0x06 // Fail to generate char file due to disorderly image
#define R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT 0x07 // Fail to generate char file due to lackness of char point or smallness
#define R502A_CONF_FINGER_NOMATCH 0x08 // Finger doesn't match
#define R502A_CONF_FAIL_FIND_MATCH 0x09 // Fail to find the matching finger
#define R502A_CONF_FAIL_COMBINE  0x0A // Fail to combine the character files
#define R502A_CONF_ADDR_BEYOND_LIB 0x0B // Addressing PageID is beyond the finger library
#define R502A_CONF_ERR_READ_TEMPLATE 0x0C // Error when reading template from library or template is invalid
#define R502A_CONF_ERR_UPLOAD_TEMPLATE 0x0D // Error when uploading template
#define R502A_CONF_ERR_RECV_FOLLOWING_DATA 0x0E // Module can't receive the following data packages
#define R502A_CONF_ERR_UPLOAD_IMAGE 0x0F // Error when uploading image
#define R502A_CONF_FAIL_DELETE_TEMPLATE 0x10 // Fail to delete the template
#define R502A_CONF_FAIL_CLEAR_LIB 0x11 // Fail to clear finger library
#define R502A_CONF_PWD_FAIL      0x13 // Wrong password!
#define R502A_CONF_FAIL_GEN_IMAGE_NO_PRIMARY 0x15 // Fail to generate image for lackness of valid primary image
#define R502A_CONF_ERR_WRITE_FLASH 0x18 // Error when writing flash
#define R502A_CONF_TIMEOUT       0x26 // Timeout (from sensor or driver)
// ... (add more error codes as implemented)

// --- Driver-Specific Error Codes (distinct from sensor confirmation codes) ---
// These are typically returned by the driver when an operation fails before
// or during communication, or if the handle is invalid.
// Using a higher range to distinguish.
#define R502A_ERR_NOT_INITIALIZED     0xF0 // Handle not initialized or UART functions missing
#define R502A_ERR_UART_WRITE_FAIL     0xF1 // UART write function failed
#define R502A_ERR_UART_READ_FAIL      0xF2 // UART read function failed (generic, distinct from sensor timeout)
#define R502A_ERR_INVALID_ACK_PACKET  0xF3 // Received packet is not a valid ACK packet (header, PID, etc.)
#define R502A_ERR_ACK_CHECKSUM_FAIL   0xF4 // Received ACK packet checksum mismatch
#define R502A_ERR_ACK_UNEXPECTED_LEN  0xF5 // ACK packet has an unexpected number of parameters
#define R502A_ERR_MALLOC_FAIL         0xF6 // Memory allocation failed (if dynamic memory were used)
#define R502A_ERR_INVALID_ARGS        0xF7 // Invalid arguments passed to a driver function


// --- UART Function Pointers ---
/**
 * @brief Typedef for the UART write function.
 * @param data Pointer to the data buffer to write.
 * @param length Number of bytes to write.
 * @return 0 on success, non-zero on failure.
 */
typedef int (*r502a_uart_write_fn)(const uint8_t* data, uint16_t length);

/**
 * @brief Typedef for the UART read function.
 * @param buffer Pointer to the buffer to store read data.
 * @param length Number of bytes to read.
 * @param timeout_ms Timeout in milliseconds for the read operation.
 * @return Number of bytes read, or -1 on timeout/error.
 */
typedef int (*r502a_uart_read_fn)(uint8_t* buffer, uint16_t length, uint32_t timeout_ms);

// --- Structures ---
typedef struct {
    uint32_t device_address;
    r502a_uart_write_fn write_uart;
    r502a_uart_read_fn read_uart;
    // Potentially add a user_data pointer if needed for uart functions
    // void* user_uart_data;
} r502a_handle_t;

typedef struct {
    uint16_t status_register;
    uint16_t system_identifier_code; // Should be 0x0000
    uint16_t finger_library_size;
    uint16_t security_level;         // 1-5
    uint32_t device_address;         // The address stored in the module
    uint16_t data_packet_size_code;  // 0:32, 1:64, 2:128, 3:256 bytes
    uint16_t baud_rate_N;            // Baud = 9600 * N
} r502a_system_params_t;

typedef struct {
    uint16_t page_id;    // Template number found
    uint16_t match_score; // Matching score
} r502a_search_result_t;

// --- Public API ---

/**
 * @brief Initializes the R502-A fingerprint sensor module handle.
 *
 * @param handle Pointer to the r502a_handle_t structure to initialize.
 * @param device_addr The 4-byte device address (default is 0xFFFFFFFF).
 * @param write_func Pointer to the user-provided UART write function.
 * @param read_func Pointer to the user-provided UART read function.
 * @return R502A_CONF_OK on success, or an error code.
 */
uint8_t r502a_init(r502a_handle_t* handle, uint32_t device_addr,
                   r502a_uart_write_fn write_func, r502a_uart_read_fn read_func);

/**
 * @brief Collects a fingerprint image from the sensor and stores it in the sensor's ImageBuffer.
 *
 * This command instructs the sensor to capture a fingerprint image. The sensor will wait for a finger
 * to be placed on the surface.
 *
 * @param handle Pointer to the R502A handle.
 * @param confirmation_code Pointer to store the confirmation code from the sensor.
 *                          Possible values include:
 *                          - R502A_CONF_OK: Command execution successful.
 *                          - R502A_CONF_ERR_RECV: Error receiving package.
 *                          - R502A_CONF_NO_FINGER: No finger on the sensor.
 *                          - R502A_CONF_IMG_FAIL: Failed to collect finger.
 * @return R502A_ERR_OK on success, or an error code on failure (e.g., R502A_ERR_UART_WRITE_FAIL, R502A_ERR_UART_READ_FAIL, R502A_ERR_INVALID_ACK_PACKET).
 */
uint8_t r502a_generate_image(r502a_handle_t* handle, uint8_t* confirmation_code);

/**
 * @brief Generates a character file (template) from the fingerprint image in ImageBuffer
 *        and stores it in CharBuffer1 or CharBuffer2.
 *
 * @param handle Pointer to the R502A handle.
 * @param buffer_id The character buffer to store the generated template (R502A_CHAR_BUFFER_1 or R502A_CHAR_BUFFER_2).
 * @param confirmation_code Pointer to store the confirmation code from the sensor.
 *                          Possible values include:
 *                          - R502A_CONF_OK: Command execution successful.
 *                          - R502A_CONF_ERR_RECV: Error receiving package.
 *                          - R502A_CONF_IMG_TOO_DISORDERLY: Image is too disorderly to generate a character file.
 *                          - R502A_CONF_IMG_LACK_FEATURE_POINTS: Image lacks characteristic points (too small area).
 *                          - R502A_CONF_IMG_FAIL: Failed to generate character file.
 * @return R502A_ERR_OK on success, or an error code on failure.
 */
uint8_t r502a_image_to_template(r502a_handle_t* handle, uint8_t buffer_id, uint8_t* confirmation_code);

/**
 * @brief Combines character files from CharBuffer1 and CharBuffer2 to generate a more robust template
 *        and stores it back into both CharBuffer1 and CharBuffer2 (effectively overwriting them).
 *
 * This command is used during enrollment. Typically, two fingerprint images are captured,
 * converted to character files (one in CharBuffer1, the other in CharBuffer2), and then
 * this command is used to merge them into a single template.
 *
 * @param handle Pointer to the R502A handle.
 * @param confirmation_code Pointer to store the confirmation code from the sensor.
 *                          Possible values include:
 *                          - R502A_CONF_OK: Command execution successful.
 *                          - R502A_CONF_ERR_RECV: Error receiving package.
 *                          - R502A_CONF_FINGER_NOMATCH: The two fingerprints do not match.
 * @return R502A_ERR_OK on success, or an error code on failure.
 */
uint8_t r502a_create_template(r502a_handle_t* handle, uint8_t* confirmation_code);

/**
 * @brief Stores the template from CharBuffer1 or CharBuffer2 into a specified pageID (address)
 *        in the fingerprint library on the sensor's flash.
 *
 * @param handle Pointer to the R502A handle.
 * @param buffer_id The character buffer containing the template to store (R502A_CHAR_BUFFER_1 or R502A_CHAR_BUFFER_2).
 * @param page_id The page ID (0 to N-1, where N is the library capacity) where the template will be stored.
 * @param confirmation_code Pointer to store the confirmation code from the sensor.
 *                          Possible values include:
 *                          - R502A_CONF_OK: Command execution successful.
 *                          - R502A_CONF_ERR_RECV: Error receiving package.
 *                          - R502A_CONF_BAD_LOCATION: Invalid page ID.
 *                          - R502A_CONF_FLASH_ERR: Error writing to flash.
 * @return R502A_ERR_OK on success, or an error code on failure.
 */
uint8_t r502a_store_template(r502a_handle_t* handle, uint8_t buffer_id, uint16_t page_id, uint8_t* confirmation_code);

/**
 * @brief Sends a HandShake command to the module.
 *
 * @param handle Pointer to the initialized r502a_handle_t structure.
 * @return R502A_CONF_OK if handshake is successful, otherwise an error/confirmation code.
 */
uint8_t r502a_handshake(r502a_handle_t* handle);

// Add other function prototypes here as they are implemented, e.g.:

/**
 * @brief Verifies the module's handshake password.
 *
 * @param handle Pointer to the initialized r502a_handle_t structure.
 * @param password The 4-byte password to verify.
 * @return R502A_CONF_OK if password is correct, R502A_CONF_PWD_FAIL if incorrect,
 *         or another error/confirmation code.
 */
uint8_t r502a_verify_password(r502a_handle_t* handle, uint32_t password);


/**
 * @brief Searches the finger library for a template matching the one in CharBufferID,
 *        within a specified range.
 *
 * @param handle Pointer to the initialized r502a_handle_t structure.
 * @param buffer_id The CharBuffer ID (e.g., 0x01) containing the template to search for.
 * @param start_page The starting page ID in the library to search from.
 * @param num_pages The number of pages/templates to search.
 * @param result Pointer to a r502a_search_result_t structure to store the found PageID and MatchScore.
 * @return R502A_CONF_OK if a match is found, R502A_CONF_FAIL_FIND_MATCH if no match,
 *         or another error/confirmation code.
 */
uint8_t r502a_search_fingerprint(r502a_handle_t* handle, uint8_t buffer_id,
                                 uint16_t start_page, uint16_t num_pages,
                                 r502a_search_result_t* result);

/**
 * @brief Deletes a specified number of templates from the Flash library,
 *        starting from a given PageID.
 *
 * @param handle Pointer to the initialized r502a_handle_t structure.
 * @param start_page The starting PageID from which to delete templates.
 * @param num_to_delete The number of templates to delete.
 * @return R502A_CONF_OK if successful, R502A_CONF_FAIL_DELETE_TEMPLATE if failed,
 *         or another error/confirmation code.
 */
uint8_t r502a_delete_template(r502a_handle_t* handle, uint16_t start_page, uint16_t num_to_delete);

/**
 * @brief Deletes all templates from the Flash fingerprint library.
 *
 * @param handle Pointer to the initialized r502a_handle_t structure.
 * @return R502A_CONF_OK if successful, R502A_CONF_FAIL_CLEAR_LIB if failed,
 *         or another error/confirmation code.
 */
uint8_t r502a_empty_fingerprint_library(r502a_handle_t* handle);

/**
 * @brief Reads the system parameters from the module.
 *
 * @param handle Pointer to the initialized r502a_handle_t structure.
 * @param params Pointer to a r502a_system_params_t structure to store the parameters.
 * @return R502A_CONF_OK if successful, otherwise an error/confirmation code.
 */
uint8_t r502a_read_system_parameters(r502a_handle_t* handle, r502a_system_params_t* params);

/**
 * @brief Configures the sensor's Aura LED.
 *
 * @param handle Pointer to the initialized r502a_handle_t structure.
 * @param ctrl_code The LED control mode (e.g., R502A_LED_CTRL_BREATHING, R502A_LED_CTRL_ON).
 * @param speed Speed of the LED effect (0-255). Relevant for breathing, flashing, gradual on/off.
 * @param color_index The LED color (e.g., R502A_LED_COLOR_RED, R502A_LED_COLOR_BLUE).
 * @param count Number of times for the effect to repeat (0 for infinite). Relevant for breathing, flashing.
 * @param confirmation_code Pointer to store the confirmation code from the sensor.
 *                          Typically R502A_CONF_OK on success.
 * @return R502A_CONF_OK on successful driver operation and ACK from sensor,
 *         or a driver-level error code (e.g., R502A_ERR_UART_WRITE_FAIL),
 *         or a sensor error code if the command was rejected.
 */
uint8_t r502a_set_aura_led_config(r502a_handle_t* handle,
                                  uint8_t ctrl_code,
                                  uint8_t speed,
                                  uint8_t color_index,
                                  uint8_t count,
                                  uint8_t* confirmation_code);

/**
 * @brief Converts an R502-A confirmation or driver error code to a human-readable string.
 *
 * @param error_code The error code (either a sensor confirmation code or a driver-specific R502A_ERR_ code).
 * @return A constant string describing the error code. Returns "Unknown error code" if not recognized.
 */
const char* r502a_error_code_to_string(uint8_t error_code);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // R502A_DRIVER_H