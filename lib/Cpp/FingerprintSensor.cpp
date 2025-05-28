#include "FingerprintSensor.hpp"
#include <cstring> // For memcpy, if needed, though C functions handle it

// Forward declaration of the C library's internal send_command_and_receive_ack
// This is NOT ideal and is a temporary workaround.
// A better approach would be to expose necessary low-level C functions if C++ needs them,
// or for C++ to fully reimplement packet logic.
// For now, we assume C functions are sufficient for the public API.

// If we decide to re-implement packet logic in C++, we'd remove this.
// static uint8_t send_command_and_receive_ack_c_wrapper(r502a_handle_t* handle, uint8_t cmd_code,
//                                                     const uint8_t* params, uint16_t params_len,
//                                                     uint8_t* ack_params, uint16_t* ack_params_len);


namespace Grow {

FingerprintSensor::FingerprintSensor(uint32_t device_address,
                                     r502a_uart_write_fn write_uart_fn,
                                     r502a_uart_read_fn read_uart_fn)
    : initialized_(false) {
    sensor_handle_.device_address = device_address;
    sensor_handle_.write_uart = write_uart_fn;
    sensor_handle_.read_uart = read_uart_fn;
    // sensor_handle_ is now populated but r502a_init from C isn't strictly called yet.
    // The C init function doesn't do much more than this assignment.
    // We can consider the C++ object constructed as "handle ready".
}

uint8_t FingerprintSensor::init() {
    // The C r502a_init function primarily assigns the members of the handle.
    // Our constructor already does this.
    // If r502a_init had more complex logic (e.g., initial handshake, checks),
    // we would call it or replicate that logic here.
    // For now, construction implies readiness.
    if (sensor_handle_.write_uart == nullptr || sensor_handle_.read_uart == nullptr) {
        initialized_ = false;
        return R502A_CONF_ERR_RECV; // Or a more C++ specific error/exception
    }
    initialized_ = true;
    return R502A_CONF_OK;
}

uint8_t FingerprintSensor::handshake() {
    if (!initialized_) return R502A_CONF_ERR_RECV; // Indicate not initialized
    return r502a_handshake(&sensor_handle_);
}

uint8_t FingerprintSensor::verifyPassword(uint32_t password) {
    if (!initialized_) return R502A_CONF_ERR_RECV;
    return r502a_verify_password(&sensor_handle_, password);
}

uint8_t FingerprintSensor::readSystemParameters(r502a_system_params_t& params) {
    if (!initialized_) return R502A_CONF_ERR_RECV;
    return r502a_read_system_parameters(&sensor_handle_, &params);
}

uint8_t FingerprintSensor::generateImage(uint8_t* confirmationCode) {
    if (!initialized_) {
        if (confirmationCode) *confirmationCode = 0xFF; // Undefined
        return R502A_ERR_NOT_INITIALIZED; // Or a more C++ specific error
    }
    if (!confirmationCode) {
        return R502A_ERR_INVALID_ARGS; // confirmationCode pointer is mandatory
    }
    return r502a_generate_image(&sensor_handle_, confirmationCode);
}

uint8_t FingerprintSensor::imageToTemplate(uint8_t bufferId, uint8_t* confirmationCode) {
    if (!initialized_) {
        if (confirmationCode) *confirmationCode = 0xFF;
        return R502A_ERR_NOT_INITIALIZED;
    }
    if (!confirmationCode) {
        return R502A_ERR_INVALID_ARGS;
    }
    // C driver r502a_image_to_template already validates bufferId (0x01 or 0x02)
    return r502a_image_to_template(&sensor_handle_, bufferId, confirmationCode);
}

uint8_t FingerprintSensor::createTemplate(uint8_t* confirmationCode) {
    if (!initialized_) {
        if (confirmationCode) *confirmationCode = 0xFF;
        return R502A_ERR_NOT_INITIALIZED;
    }
    if (!confirmationCode) {
        return R502A_ERR_INVALID_ARGS;
    }
    return r502a_create_template(&sensor_handle_, confirmationCode);
}

uint8_t FingerprintSensor::storeTemplate(uint8_t bufferId, uint16_t pageId, uint8_t* confirmationCode) {
    if (!initialized_) {
        if (confirmationCode) *confirmationCode = 0xFF;
        return R502A_ERR_NOT_INITIALIZED;
    }
    if (!confirmationCode) {
        return R502A_ERR_INVALID_ARGS;
    }
    // C driver r502a_store_template already validates bufferId (0x01 or 0x02)
    return r502a_store_template(&sensor_handle_, bufferId, pageId, confirmationCode);
}

uint8_t FingerprintSensor::searchFingerprint(uint8_t buffer_id, uint16_t start_page, uint16_t num_pages, r502a_search_result_t& result) {
    if (!initialized_) return R502A_CONF_ERR_RECV;
    return r502a_search_fingerprint(&sensor_handle_, buffer_id, start_page, num_pages, &result);
}

uint8_t FingerprintSensor::deleteTemplate(uint16_t start_page, uint16_t num_to_delete) {
    if (!initialized_) return R502A_CONF_ERR_RECV;
    return r502a_delete_template(&sensor_handle_, start_page, num_to_delete);
}

uint8_t FingerprintSensor::emptyFingerprintLibrary() {
    if (!initialized_) return R502A_CONF_ERR_RECV;
    return r502a_empty_fingerprint_library(&sensor_handle_);
}

uint8_t FingerprintSensor::setAuraLedConfig(uint8_t ctrl_code,
                                            uint8_t speed,
                                            uint8_t color_index,
                                            uint8_t count,
                                            uint8_t* confirmationCode) {
    if (!initialized_) {
        if (confirmationCode) *confirmationCode = 0xFF; // Undefined
        return R502A_ERR_NOT_INITIALIZED;
    }
    if (!confirmationCode) {
        return R502A_ERR_INVALID_ARGS; // confirmationCode pointer is mandatory
    }
    // Parameter validation can be added here if desired, though the C driver
    // might also perform some. The C++ constants are available for reference.
    return r502a_set_aura_led_config(&sensor_handle_, ctrl_code, speed, color_index, count, confirmationCode);
}

const char* FingerprintSensor::errorCodeToString(uint8_t errorCode) {
    return r502a_error_code_to_string(errorCode);
}

// Re-implementing packet logic or a C++ specific helper would go here if not wrapping C.
// For now, the C functions are self-contained enough.
// uint16_t FingerprintSensor::calculateChecksum(const uint8_t* buffer, uint16_t length) {
//     uint16_t sum = 0;
//     for (uint16_t i = 0; i < length; i++) {
//         sum += buffer[i];
//     }
//     return sum;
// }
//
// uint8_t FingerprintSensor::sendCommandAndReceiveAck(uint8_t cmd_code,
//                                                     const std::vector<uint8_t>& params_data,
//                                                     std::vector<uint8_t>* ack_params_data_out) {
//     // This would be a C++ reimplementation of the C version's logic
//     // using std::vector and the UART function pointers from sensor_handle_.
//     // For brevity in this step, we are directly calling the C API functions.
//     return R502A_CONF_ERR_RECV; // Placeholder
// }


} // namespace Grow