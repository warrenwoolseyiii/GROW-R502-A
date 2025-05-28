# Constants from the C header for R502-A Fingerprint Sensor

# Packet Structure
R502A_PACKET_HEADER_HIGH = 0xEF
R502A_PACKET_HEADER_LOW  = 0x01
R502A_DEFAULT_ADDRESS    = 0xFFFFFFFF

# Packet Identifiers (PID)
R502A_PID_COMMAND        = 0x01 # Command packet
R502A_PID_DATA           = 0x02 # Data packet
R502A_PID_ACK            = 0x07 # Acknowledge packet
R502A_PID_END_DATA       = 0x08 # End of Data packet

# Command Codes
R502A_CMD_HANDSHAKE      = 0x40
R502A_CMD_READ_SYS_PARA  = 0x0F
R502A_CMD_VFY_PWD        = 0x13
R502A_CMD_GET_IMG_EX     = 0x28 # Used by generate_image in Python lib
R502A_CMD_GEN_CHAR       = 0x02 # Used by image_to_template in Python lib
R502A_CMD_REG_MODEL      = 0x05 # Used by create_template in Python lib
R502A_CMD_STORE          = 0x06 # Used by store_template in Python lib
R502A_CMD_SEARCH         = 0x04
R502A_CMD_DELETE_CHAR    = 0x0C
R502A_CMD_EMPTY          = 0x0D

# Confirmation Codes
R502A_CONF_OK            = 0x00 # Command execution complete
R502A_CONF_ERR_RECV      = 0x01 # Error when receiving data package
R502A_CONF_NO_FINGER     = 0x02 # No finger on the sensor
R502A_CONF_FAIL_ENROLL   = 0x03 # Fail to enroll the finger
R502A_CONF_FAIL_GEN_CHAR_DISORDERLY = 0x06
R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT = 0x07
R502A_CONF_FINGER_NOMATCH = 0x08 # Finger doesn't match
R502A_CONF_FAIL_FIND_MATCH = 0x09 # Fail to find the matching finger
R502A_CONF_FAIL_COMBINE  = 0x0A # Fail to combine the character files
R502A_CONF_ADDR_BEYOND_LIB = 0x0B
R502A_CONF_ERR_READ_TEMPLATE = 0x0C
R502A_CONF_ERR_UPLOAD_TEMPLATE = 0x0D
R502A_CONF_ERR_RECV_FOLLOWING_DATA = 0x0E
R502A_CONF_ERR_UPLOAD_IMAGE = 0x0F
R502A_CONF_FAIL_DELETE_TEMPLATE = 0x10
R502A_CONF_FAIL_CLEAR_LIB = 0x11
R502A_CONF_PWD_FAIL      = 0x13 # Wrong password!
R502A_CONF_FAIL_GEN_IMAGE_NO_PRIMARY = 0x15
R502A_CONF_ERR_WRITE_FLASH = 0x18
R502A_CONF_TIMEOUT       = 0x26 # Timeout (from sensor or driver)

# Driver-Specific Error Codes (from C driver, for reference)
R502A_ERR_NOT_INITIALIZED     = 0xF0
R502A_ERR_UART_WRITE_FAIL     = 0xF1
R502A_ERR_UART_READ_FAIL      = 0xF2
R502A_ERR_INVALID_ACK_PACKET  = 0xF3
R502A_ERR_ACK_CHECKSUM_FAIL   = 0xF4
R502A_ERR_ACK_UNEXPECTED_LEN  = 0xF5
R502A_ERR_MALLOC_FAIL         = 0xF6
R502A_ERR_INVALID_ARGS        = 0xF7

# Helper function to convert error codes to strings
def error_code_to_string(error_code):
    codes = {
        R502A_CONF_OK: "OK (Command execution complete)",
        R502A_CONF_ERR_RECV: "ERR_RECV (Error when receiving data package from sensor)",
        R502A_CONF_NO_FINGER: "NO_FINGER (No finger on the sensor)",
        R502A_CONF_FAIL_ENROLL: "FAIL_ENROLL (Fail to enroll the finger)",
        R502A_CONF_FAIL_GEN_CHAR_DISORDERLY: "FAIL_GEN_CHAR_DISORDERLY (Fail to generate char file due to disorderly image)",
        R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT: "FAIL_GEN_CHAR_SMALL_POINT (Fail to generate char file due to lack of char point or smallness)",
        R502A_CONF_FINGER_NOMATCH: "FINGER_NOMATCH (Finger doesn't match)",
        R502A_CONF_FAIL_FIND_MATCH: "FAIL_FIND_MATCH (Fail to find the matching finger)",
        R502A_CONF_FAIL_COMBINE: "FAIL_COMBINE (Fail to combine the character files)",
        R502A_CONF_ADDR_BEYOND_LIB: "ADDR_BEYOND_LIB (Addressing PageID is beyond the finger library)",
        R502A_CONF_ERR_READ_TEMPLATE: "ERR_READ_TEMPLATE (Error when reading template from library or template is invalid)",
        R502A_CONF_ERR_UPLOAD_TEMPLATE: "ERR_UPLOAD_TEMPLATE (Error when uploading template)",
        R502A_CONF_ERR_RECV_FOLLOWING_DATA: "ERR_RECV_FOLLOWING_DATA (Module can't receive the following data packages)",
        R502A_CONF_ERR_UPLOAD_IMAGE: "ERR_UPLOAD_IMAGE (Error when uploading image)",
        R502A_CONF_FAIL_DELETE_TEMPLATE: "FAIL_DELETE_TEMPLATE (Fail to delete the template)",
        R502A_CONF_FAIL_CLEAR_LIB: "FAIL_CLEAR_LIB (Fail to clear finger library)",
        R502A_CONF_PWD_FAIL: "PWD_FAIL (Wrong password!)",
        R502A_CONF_FAIL_GEN_IMAGE_NO_PRIMARY: "FAIL_GEN_IMAGE_NO_PRIMARY (Fail to generate image for lackness of valid primary image)",
        R502A_CONF_ERR_WRITE_FLASH: "ERR_WRITE_FLASH (Error when writing flash)",
        R502A_CONF_TIMEOUT: "TIMEOUT (Sensor or driver communication timeout)",
        # Python library specific errors (can be mapped to driver errors or be distinct)
        R502A_ERR_NOT_INITIALIZED: "DRIVER_ERR_NOT_INITIALIZED (Python: Serial port not open or init failed)",
        R502A_ERR_UART_WRITE_FAIL: "DRIVER_ERR_UART_WRITE_FAIL (Python: UART write error)",
        R502A_ERR_UART_READ_FAIL: "DRIVER_ERR_UART_READ_FAIL (Python: UART read error/timeout)",
        R502A_ERR_INVALID_ACK_PACKET: "DRIVER_ERR_INVALID_ACK_PACKET (Python: Invalid ACK packet)",
        R502A_ERR_ACK_CHECKSUM_FAIL: "DRIVER_ERR_ACK_CHECKSUM_FAIL (Python: ACK checksum mismatch)",
        R502A_ERR_INVALID_ARGS: "DRIVER_ERR_INVALID_ARGS (Python: Invalid arguments to method)",
    }
    return codes.get(error_code, f"Unknown error code (0x{error_code:02X})")
