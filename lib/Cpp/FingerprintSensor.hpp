#ifndef FINGERPRINT_SENSOR_HPP
#define FINGERPRINT_SENSOR_HPP

#include <stdint.h> // Using C-style header for types
#include <vector>   // For std::vector
#include <cstddef>  // For std::size_t (often included by vector)


// Assuming the C driver header is accessible for UART function pointer types
// and potentially other definitions if we decide to wrap it.
// For now, we'll redefine the function pointers for clarity within the C++ context,
// or ensure the C header is C++ compatible.
// Let's assume C header is C++ compatible (uses extern "C" if needed, or is C-like).
#include "../C/r502a_driver.h" // For r502a_uart_write_fn, r502a_uart_read_fn, and status codes

namespace Grow {

class FingerprintSensor {
public:
    // --- LED Control Constants (mirroring C driver defines) ---
    // Control Codes
    static constexpr uint8_t LED_CTRL_BREATHING   = 0x01;
    static constexpr uint8_t LED_CTRL_FLASHING    = 0x02;
    static constexpr uint8_t LED_CTRL_ON          = 0x03;
    static constexpr uint8_t LED_CTRL_OFF         = 0x04;
    static constexpr uint8_t LED_CTRL_GRADUAL_ON  = 0x05;
    static constexpr uint8_t LED_CTRL_GRADUAL_OFF = 0x06;

    // Color Indicies
    static constexpr uint8_t LED_COLOR_RED        = 0x01;
    static constexpr uint8_t LED_COLOR_BLUE       = 0x02;
    static constexpr uint8_t LED_COLOR_PURPLE     = 0x03;
    static constexpr uint8_t LED_COLOR_GREEN      = 0x04;
    static constexpr uint8_t LED_COLOR_YELLOW     = 0x05;
    static constexpr uint8_t LED_COLOR_CYAN       = 0x06;
    static constexpr uint8_t LED_COLOR_WHITE      = 0x07;


    FingerprintSensor(uint32_t device_address,
                      r502a_uart_write_fn write_uart_fn,
                      r502a_uart_read_fn read_uart_fn);

    // Initialize the sensor handle (can be called implicitly by constructor or explicitly)
    uint8_t init();

    // --- Basic Commands ---
    uint8_t handshake();
    uint8_t verifyPassword(uint32_t password);
    uint8_t readSystemParameters(r502a_system_params_t& params); // Using C struct for now
    // Old enrollment functions removed, new ones below
    uint8_t searchFingerprint(uint8_t buffer_id, uint16_t start_page, uint16_t num_pages, r502a_search_result_t& result);
    uint8_t deleteTemplate(uint16_t start_page, uint16_t num_to_delete);
    uint8_t emptyFingerprintLibrary();

    // Potentially add methods for data transfer commands like UpImage, DownImage, UpChar, DownChar later.

    // --- Utility ---
    /**
     * @brief Converts an R502-A confirmation or driver error code to a human-readable string.
     * This is a static wrapper around the C driver's r502a_error_code_to_string function.
     * @param errorCode The error code from the sensor or C driver.
     * @return A constant string describing the error code.
     */
    static const char* errorCodeToString(uint8_t errorCode);

    // --- New Enrollment Functions ---
    /**
     * @brief Collects a fingerprint image from the sensor and stores it in the sensor's ImageBuffer.
     *
     * @param confirmationCode Pointer to store the confirmation code from the sensor.
     * @return R502A_CONF_OK on successful driver operation (check confirmationCode for sensor status),
     *         or a driver-level error code on failure.
     */
    uint8_t generateImage(uint8_t* confirmationCode);

    /**
     * @brief Generates a character file (template) from the fingerprint image in ImageBuffer
     *        and stores it in CharBuffer1 or CharBuffer2.
     *
     * @param bufferId The character buffer to store the generated template (R502A_CHAR_BUFFER_1 or R502A_CHAR_BUFFER_2).
     * @param confirmationCode Pointer to store the confirmation code from the sensor.
     * @return R502A_CONF_OK on successful driver operation (check confirmationCode for sensor status),
     *         or a driver-level error code on failure.
     */
    uint8_t imageToTemplate(uint8_t bufferId, uint8_t* confirmationCode);

    /**
     * @brief Combines character files from CharBuffer1 and CharBuffer2 to generate a template.
     *
     * @param confirmationCode Pointer to store the confirmation code from the sensor.
     * @return R502A_CONF_OK on successful driver operation (check confirmationCode for sensor status),
     *         or a driver-level error code on failure.
     */
    uint8_t createTemplate(uint8_t* confirmationCode);

    /**
     * @brief Stores the template from CharBuffer1 or CharBuffer2 into a specified pageID
     *        in the fingerprint library.
     *
     * @param bufferId The character buffer containing the template (R502A_CHAR_BUFFER_1 or R502A_CHAR_BUFFER_2).
     * @param pageId The page ID where the template will be stored.
     * @param confirmationCode Pointer to store the confirmation code from the sensor.
     * @return R502A_CONF_OK on successful driver operation (check confirmationCode for sensor status),
     *         or a driver-level error code on failure.
     */
    uint8_t storeTemplate(uint8_t bufferId, uint16_t pageId, uint8_t* confirmationCode);

    // --- LED Control ---
    /**
     * @brief Configures the sensor's Aura LED.
     *
     * @param ctrl_code The LED control mode (e.g., FingerprintSensor::LED_CTRL_BREATHING).
     * @param speed Speed of the LED effect (0-255).
     * @param color_index The LED color (e.g., FingerprintSensor::LED_COLOR_BLUE).
     * @param count Number of times for the effect to repeat (0 for infinite).
     * @param confirmationCode Pointer to store the confirmation code from the sensor.
     * @return R502A_CONF_OK on successful C driver operation (check confirmationCode for sensor status),
     *         or a C driver-level error code on failure.
     */
    uint8_t setAuraLedConfig(uint8_t ctrl_code,
                             uint8_t speed,
                             uint8_t color_index,
                             uint8_t count,
                             uint8_t* confirmationCode);
private:
    r502a_handle_t sensor_handle_; // Use the C handle internally
    bool initialized_;

    // Internal helper to wrap send_command_and_receive_ack or re-implement packet logic
    uint8_t sendCommandAndReceiveAck(uint8_t cmd_code,
                                     const std::vector<uint8_t>& params,
                                     std::vector<uint8_t>* ack_params_data);
    
    // Helper to calculate checksum (could be static or in a utility namespace)
    static uint16_t calculateChecksum(const uint8_t* buffer, uint16_t length);

}; // class FingerprintSensor

} // namespace Grow

#endif // FINGERPRINT_SENSOR_HPP