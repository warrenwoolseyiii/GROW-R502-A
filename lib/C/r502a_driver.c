#include "r502a_driver.h"
#include <string.h> // For memcpy

// --- Private Helper Functions ---

// Calculates the 2-byte checksum for a packet.
// Sum of: PID + Length + Content
static uint16_t calculate_checksum(const uint8_t* buffer, uint16_t length) {
    uint16_t sum = 0;
    for (uint16_t i = 0; i < length; i++) {
        sum += buffer[i];
    }
    return sum;
}

// Sends a command packet and receives an acknowledge packet.
// This is a simplified version; more robust error handling and data packet handling will be added.
static uint8_t send_command_and_receive_ack(r502a_handle_t* handle, uint8_t cmd_code,
                                            const uint8_t* params, uint16_t params_len,
                                            uint8_t* ack_params, uint16_t* ack_params_len) {
    // Pre-condition check (already done by public API functions, but good for internal safety)
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }

    uint8_t tx_buffer[256]; // Max packet size
    uint8_t rx_buffer[256]; // Max packet size
    uint16_t packet_idx = 0;

    // 1. Construct Command Packet
    // Header
    tx_buffer[packet_idx++] = R502A_PACKET_HEADER_HIGH;
    tx_buffer[packet_idx++] = R502A_PACKET_HEADER_LOW;

    // Address
    tx_buffer[packet_idx++] = (handle->device_address >> 24) & 0xFF;
    tx_buffer[packet_idx++] = (handle->device_address >> 16) & 0xFF;
    tx_buffer[packet_idx++] = (handle->device_address >> 8) & 0xFF;
    tx_buffer[packet_idx++] = (handle->device_address) & 0xFF;

    // PID (Command)
    tx_buffer[packet_idx++] = R502A_PID_COMMAND;

    // Length (Content + Checksum_Length(2))
    uint16_t content_len = 1 + params_len; // cmd_code + params
    uint16_t package_len = content_len + 2;
    tx_buffer[packet_idx++] = (package_len >> 8) & 0xFF;
    tx_buffer[packet_idx++] = package_len & 0xFF;

    // Content: Instruction Code
    tx_buffer[packet_idx++] = cmd_code;

    // Content: Parameters
    if (params != NULL && params_len > 0) {
        memcpy(&tx_buffer[packet_idx], params, params_len);
        packet_idx += params_len;
    }

    // Checksum (PID + Length + Content)
    uint16_t checksum = calculate_checksum(&tx_buffer[6], 3 + content_len); // Start from PID
    tx_buffer[packet_idx++] = (checksum >> 8) & 0xFF;
    tx_buffer[packet_idx++] = checksum & 0xFF;

    // 2. Send Packet
    if (handle->write_uart(tx_buffer, packet_idx) != 0) {
        return R502A_ERR_UART_WRITE_FAIL;
    }

    // 3. Receive Acknowledge Packet
    // Read Header, Address, PID, Length fields first (9 bytes)
    int bytes_read = handle->read_uart(rx_buffer, 9, 1000); // 1s timeout
    if (bytes_read == -1) { // UART read function indicates error (e.g. port closed, system error)
        return R502A_ERR_UART_READ_FAIL;
    }
    if (bytes_read < 9) { // Not enough bytes for header, likely timeout from sensor perspective
        return R502A_CONF_TIMEOUT;
    }

    // Validate Header
    if (rx_buffer[0] != R502A_PACKET_HEADER_HIGH || rx_buffer[1] != R502A_PACKET_HEADER_LOW) {
        return R502A_ERR_INVALID_ACK_PACKET; // Invalid header
    }
    // Validate PID
    if (rx_buffer[6] != R502A_PID_ACK) {
        return R502A_ERR_INVALID_ACK_PACKET; // Expected ACK packet, got something else
    }

    // Address check (optional, could be useful for multi-module setups)
    // uint32_t packet_address = ((uint32_t)rx_buffer[2] << 24) | ((uint32_t)rx_buffer[3] << 16) | \
    //                            ((uint32_t)rx_buffer[4] << 8)  | rx_buffer[5];
    // if (packet_address != handle->device_address) {
    //     // Address mismatch, though for single device usually 0xFFFFFFFF
    // }

    uint16_t ack_package_fields_len = ((uint16_t)rx_buffer[7] << 8) | rx_buffer[8]; // Length of (Content + Checksum)

    if (ack_package_fields_len < 3) { // Min content is 1 byte (ConfCode) + 2 bytes (Checksum)
        return R502A_ERR_INVALID_ACK_PACKET; // Invalid package length (too short)
    }
    if (9 + ack_package_fields_len > sizeof(rx_buffer)) {
        return R502A_ERR_INVALID_ACK_PACKET; // Packet too large for our buffer
    }

    // Read the rest of the packet (Content + Checksum)
    bytes_read = handle->read_uart(&rx_buffer[9], ack_package_fields_len, 1000);
    if (bytes_read == -1) { // UART read function indicates error
        return R502A_ERR_UART_READ_FAIL;
    }
    if (bytes_read < ack_package_fields_len) { // Not enough bytes, likely timeout
        return R502A_CONF_TIMEOUT;
    }

    // 4. Validate Checksum
    // Checksum is calculated over PID, Length_Field, and Package_Content
    // Total length for checksum calculation: 1 (PID) + 2 (Length_Field) + (ack_package_fields_len - 2) (Content)
    uint16_t checksum_calc_len = 1 + 2 + (ack_package_fields_len - 2);
    uint16_t calculated_ack_checksum = calculate_checksum(&rx_buffer[6], checksum_calc_len);
    uint16_t received_checksum = ((uint16_t)rx_buffer[9 + ack_package_fields_len - 2] << 8) | rx_buffer[9 + ack_package_fields_len - 1];

    if (received_checksum != calculated_ack_checksum) {
        return R502A_ERR_ACK_CHECKSUM_FAIL;
    }

    // 5. Extract Confirmation Code
    uint8_t confirmation_code = rx_buffer[9];

    // 6. Extract Parameters from ACK (if any)
    uint16_t ack_content_len = ack_package_fields_len - 2; // Total content length (ConfCode + Params)
    uint16_t actual_ack_params_len = 0;
    if (ack_content_len > 1) { // If there's more than just the confirmation code
        actual_ack_params_len = ack_content_len - 1; // Number of parameter bytes in the ACK
    }

    if (ack_params != NULL && ack_params_len != NULL) {
        // Caller wants to receive parameters
        if (*ack_params_len >= actual_ack_params_len) {
            // Buffer is large enough
            if (actual_ack_params_len > 0) {
                memcpy(ack_params, &rx_buffer[10], actual_ack_params_len); // Params start after ConfCode
            }
            *ack_params_len = actual_ack_params_len; // Report how many bytes were actually in the ACK params
        } else {
            // Buffer is too small for all received parameters.
            *ack_params_len = 0; // Report 0 bytes copied.
            // This is an error condition if parameters were expected.
            // We return the sensor's confirmation code, but the caller should also check *ack_params_len.
            // Optionally, if actual_ack_params_len > 0, we could return R502A_ERR_ACK_UNEXPECTED_LEN here
            // to override the sensor's confirmation code, indicating a buffer-too-small problem.
            // For now, let the original confirmation_code pass through.
        }
    } else if (ack_params_len != NULL) {
        // Caller provided ack_params_len (to know how many bytes *would* be returned) but ack_params is NULL.
        *ack_params_len = actual_ack_params_len;
    }
    // If both ack_params and ack_params_len are NULL, we do nothing and ignore received parameters.
    
    // A stricter check could be added here: if the command *always* expects a certain number
    // of ack parameters, and actual_ack_params_len doesn't match, return R502A_ERR_ACK_UNEXPECTED_LEN,
    // even if the confirmation_code was OK. This is deferred for now as it requires command-specific knowledge.

    return confirmation_code;
}


// --- Public API Implementations ---

uint8_t r502a_init(r502a_handle_t* handle, uint32_t device_addr,
                   r502a_uart_write_fn write_func, r502a_uart_read_fn read_func) {
    if (handle == NULL) {
        return R502A_ERR_INVALID_ARGS; // Handle cannot be null
    }
    if (write_func == NULL || read_func == NULL) {
        return R502A_ERR_NOT_INITIALIZED; // UART functions must be provided
    }

    handle->device_address = device_addr;
    handle->write_uart = write_func;
    handle->read_uart = read_func;

    return R502A_CONF_OK;
}

uint8_t r502a_handshake(r502a_handle_t* handle) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }

    // Handshake command has no parameters.
    // Expected ACK: Confirmation Code (0x00 for success)
    // ACK packet content length for handshake is 1 byte (the confirmation code).
    // ACK package length field will be 0x0003 (content_len(1) + checksum_len(2)).
    return send_command_and_receive_ack(handle, R502A_CMD_HANDSHAKE, NULL, 0, NULL, NULL);
}

uint8_t r502a_read_system_parameters(r502a_handle_t* handle, r502a_system_params_t* params) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }
    if (params == NULL) {
        return R502A_ERR_INVALID_ARGS;
    }

    uint8_t ack_raw_params[16]; // Datasheet: ACK returns 16 bytes of parameters
    uint16_t ack_raw_params_len = sizeof(ack_raw_params);

    uint8_t confirmation_code = send_command_and_receive_ack(
        handle, R502A_CMD_READ_SYS_PARA, NULL, 0, ack_raw_params, &ack_raw_params_len
    );

    if (confirmation_code == R502A_CONF_OK) {
        if (ack_raw_params_len == 16) {
            // Parse the 16 bytes into the struct (Big Endian)
            // Ref: Datasheet page 13, "Acknowledge package format" table
            // Offset (word) means offset in 2-byte words.
            params->status_register         = ((uint16_t)ack_raw_params[0] << 8) | ack_raw_params[1];
            params->system_identifier_code  = ((uint16_t)ack_raw_params[2] << 8) | ack_raw_params[3]; // Should be 0x0000
            params->finger_library_size     = ((uint16_t)ack_raw_params[4] << 8) | ack_raw_params[5];
            params->security_level          = ((uint16_t)ack_raw_params[6] << 8) | ack_raw_params[7];
            params->device_address          = ((uint32_t)ack_raw_params[8] << 24) | \
                                              ((uint32_t)ack_raw_params[9] << 16) | \
                                              ((uint32_t)ack_raw_params[10] << 8) | \
                                              ack_raw_params[11];
            params->data_packet_size_code   = ((uint16_t)ack_raw_params[12] << 8) | ack_raw_params[13];
            params->baud_rate_N             = ((uint16_t)ack_raw_params[14] << 8) | ack_raw_params[15];
        } else {
            // If confirmation code was OK, but we didn't get the expected 16 parameter bytes,
            // this is an unexpected ACK format from the sensor for this command.
            return R502A_ERR_ACK_UNEXPECTED_LEN;
        }
    }
    return confirmation_code;
}

uint8_t r502a_verify_password(r502a_handle_t* handle, uint32_t password) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }

    uint8_t params[4];
    params[0] = (password >> 24) & 0xFF;
    params[1] = (password >> 16) & 0xFF;
    params[2] = (password >> 8) & 0xFF;
    params[3] = password & 0xFF;

    // The acknowledge packet for VfyPwd contains only the confirmation code.
    // Content length = 1 byte (ConfCode). Package length field = 0x0003.
    return send_command_and_receive_ack(handle, R502A_CMD_VFY_PWD, params, 4, NULL, NULL);
}

uint8_t r502a_get_image_extended(r502a_handle_t* handle) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }

    // GetImageEx command has no parameters.
    // The acknowledge packet contains only the confirmation code.
    // Content length = 1 byte (ConfCode). Package length field = 0x0003.
    return send_command_and_receive_ack(handle, R502A_CMD_GET_IMG_EX, NULL, 0, NULL, NULL);
}

uint8_t r502a_generate_character_file(r502a_handle_t* handle, uint8_t buffer_id) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }
    if (buffer_id < 1 || buffer_id > 6) { // Datasheet implies CharBufferID 1 or 2 for some ops, but GenChar can be 1-6
        return R502A_ERR_INVALID_ARGS;
    }

    uint8_t param = buffer_id;

    // GenChar command has one parameter: BufferID (1 byte).
    // The acknowledge packet contains only the confirmation code.
    // Content length = 1 byte (ConfCode). Package length field = 0x0003.
    return send_command_and_receive_ack(handle, R502A_CMD_GEN_CHAR, &param, 1, NULL, NULL);
}

uint8_t r502a_generate_template(r502a_handle_t* handle) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }

    // RegModel command has no parameters (datasheet page 19).
    // It combines info from CharBuffer1 and CharBuffer2.
    // The acknowledge packet contains only the confirmation code.
    // Content length = 1 byte (ConfCode). Package length field = 0x0003.
    return send_command_and_receive_ack(handle, R502A_CMD_REG_MODEL, NULL, 0, NULL, NULL);
}

uint8_t r502a_store_template(r502a_handle_t* handle, uint8_t buffer_id, uint16_t model_id) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }
    // Datasheet page 21, note for Store (0x06) command:
    // "CharBufferID is filled with 0x01"
    // We'll allow the user to specify it but they should be aware of this.
    // A check for buffer_id range (1-2 or 1-6 based on context) could be added if strictness is desired.

    uint8_t params[3];
    params[0] = buffer_id;
    params[1] = (model_id >> 8) & 0xFF; // ModelID High Byte
    params[2] = model_id & 0xFF;      // ModelID Low Byte

    // Store command has two parameters: CharBufferID (1 byte) and ModelID (2 bytes). Total 3 bytes.
    // The acknowledge packet contains only the confirmation code.
    // Content length = 1 byte (ConfCode). Package length field = 0x0003.
    return send_command_and_receive_ack(handle, R502A_CMD_STORE, params, 3, NULL, NULL);
}

uint8_t r502a_search_fingerprint(r502a_handle_t* handle, uint8_t buffer_id,
                                 uint16_t start_page, uint16_t num_pages,
                                 r502a_search_result_t* result) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }
    if (result == NULL) {
        return R502A_ERR_INVALID_ARGS;
    }
    // Datasheet page 24, note for Search (0x04) command:
    // "CharBufferID is filled with 0x01"
    // We'll allow the user to specify it.

    uint8_t params[5];
    params[0] = buffer_id;
    params[1] = (start_page >> 8) & 0xFF; // StartID High Byte
    params[2] = start_page & 0xFF;      // StartID Low Byte
    params[3] = (num_pages >> 8) & 0xFF;  // Num High Byte
    params[4] = num_pages & 0xFF;       // Num Low Byte

    uint8_t ack_raw_params[4]; // ACK returns PageID (2 bytes) + MatchScore (2 bytes)
    uint16_t ack_raw_params_len = sizeof(ack_raw_params);

    uint8_t confirmation_code = send_command_and_receive_ack(
        handle, R502A_CMD_SEARCH, params, sizeof(params), ack_raw_params, &ack_raw_params_len
    );

    if (confirmation_code == R502A_CONF_OK) { // Found a match
        if (ack_raw_params_len == 4) {
            result->page_id    = ((uint16_t)ack_raw_params[0] << 8) | ack_raw_params[1];
            result->match_score = ((uint16_t)ack_raw_params[2] << 8) | ack_raw_params[3];
        } else {
            // If confirmation code was OK, but we didn't get the expected 4 parameter bytes,
            // this is an unexpected ACK format from the sensor for this command.
            return R502A_ERR_ACK_UNEXPECTED_LEN;
        }
    } else {
        // If no match (0x09) or other error, zero out results
        result->page_id = 0;
        result->match_score = 0;
    }

    return confirmation_code;
}

uint8_t r502a_delete_template(r502a_handle_t* handle, uint16_t start_page, uint16_t num_to_delete) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }

    uint8_t params[4];
    params[0] = (start_page >> 8) & 0xFF;    // StartID High Byte
    params[1] = start_page & 0xFF;         // StartID Low Byte
    params[2] = (num_to_delete >> 8) & 0xFF; // Num High Byte
    params[3] = num_to_delete & 0xFF;      // Num Low Byte

    // DeletChar command has two parameters: StartID (2 bytes) and Num (2 bytes). Total 4 bytes.
    // The acknowledge packet contains only the confirmation code.
    // Content length = 1 byte (ConfCode). Package length field = 0x0003.
    return send_command_and_receive_ack(handle, R502A_CMD_DELETE_CHAR, params, sizeof(params), NULL, NULL);
}

uint8_t r502a_empty_fingerprint_library(r502a_handle_t* handle) {
    if (handle == NULL || handle->write_uart == NULL || handle->read_uart == NULL) {
        return R502A_ERR_NOT_INITIALIZED;
    }

    // Empty command has no parameters.
    // The acknowledge packet contains only the confirmation code.
    // Content length = 1 byte (ConfCode). Package length field = 0x0003.
    return send_command_and_receive_ack(handle, R502A_CMD_EMPTY, NULL, 0, NULL, NULL);
}

const char* r502a_error_code_to_string(uint8_t error_code) {
    switch (error_code) {
        // Sensor Confirmation Codes
        case R502A_CONF_OK: return "OK (Command execution complete)";
        case R502A_CONF_ERR_RECV: return "ERR_RECV (Error when receiving data package from sensor)";
        case R502A_CONF_NO_FINGER: return "NO_FINGER (No finger on the sensor)";
        case R502A_CONF_FAIL_ENROLL: return "FAIL_ENROLL (Fail to enroll the finger)";
        case R502A_CONF_FAIL_GEN_CHAR_DISORDERLY: return "FAIL_GEN_CHAR_DISORDERLY (Fail to generate char file due to disorderly image)";
        case R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT: return "FAIL_GEN_CHAR_SMALL_POINT (Fail to generate char file due to lack of char point or smallness)";
        case R502A_CONF_FINGER_NOMATCH: return "FINGER_NOMATCH (Finger doesn't match)";
        case R502A_CONF_FAIL_FIND_MATCH: return "FAIL_FIND_MATCH (Fail to find the matching finger)";
        case R502A_CONF_FAIL_COMBINE: return "FAIL_COMBINE (Fail to combine the character files)";
        case R502A_CONF_ADDR_BEYOND_LIB: return "ADDR_BEYOND_LIB (Addressing PageID is beyond the finger library)";
        case R502A_CONF_ERR_READ_TEMPLATE: return "ERR_READ_TEMPLATE (Error when reading template from library or template is invalid)";
        case R502A_CONF_ERR_UPLOAD_TEMPLATE: return "ERR_UPLOAD_TEMPLATE (Error when uploading template)";
        case R502A_CONF_ERR_RECV_FOLLOWING_DATA: return "ERR_RECV_FOLLOWING_DATA (Module can't receive the following data packages)";
        case R502A_CONF_ERR_UPLOAD_IMAGE: return "ERR_UPLOAD_IMAGE (Error when uploading image)";
        case R502A_CONF_FAIL_DELETE_TEMPLATE: return "FAIL_DELETE_TEMPLATE (Fail to delete the template)";
        case R502A_CONF_FAIL_CLEAR_LIB: return "FAIL_CLEAR_LIB (Fail to clear finger library)";
        case R502A_CONF_PWD_FAIL: return "PWD_FAIL (Wrong password!)";
        case R502A_CONF_FAIL_GEN_IMAGE_NO_PRIMARY: return "FAIL_GEN_IMAGE_NO_PRIMARY (Fail to generate image for lackness of valid primary image)";
        case R502A_CONF_ERR_WRITE_FLASH: return "ERR_WRITE_FLASH (Error when writing flash)";
        case R502A_CONF_TIMEOUT: return "TIMEOUT (Sensor or driver communication timeout)";

        // Driver-Specific Error Codes
        case R502A_ERR_NOT_INITIALIZED: return "DRIVER_ERR_NOT_INITIALIZED (Handle not initialized or UART functions missing)";
        case R502A_ERR_UART_WRITE_FAIL: return "DRIVER_ERR_UART_WRITE_FAIL (UART write function failed)";
        case R502A_ERR_UART_READ_FAIL: return "DRIVER_ERR_UART_READ_FAIL (UART read function failed)";
        case R502A_ERR_INVALID_ACK_PACKET: return "DRIVER_ERR_INVALID_ACK_PACKET (Received packet is not a valid ACK packet)";
        case R502A_ERR_ACK_CHECKSUM_FAIL: return "DRIVER_ERR_ACK_CHECKSUM_FAIL (Received ACK packet checksum mismatch)";
        case R502A_ERR_ACK_UNEXPECTED_LEN: return "DRIVER_ERR_ACK_UNEXPECTED_LEN (ACK packet has an unexpected number of parameters)";
        case R502A_ERR_MALLOC_FAIL: return "DRIVER_ERR_MALLOC_FAIL (Memory allocation failed)";
        case R502A_ERR_INVALID_ARGS: return "DRIVER_ERR_INVALID_ARGS (Invalid arguments passed to a driver function)";
        
        default: return "Unknown error code";
    }
}