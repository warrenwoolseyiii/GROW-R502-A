import serial
import time
import struct

# Constants from the C header (can be defined here or imported if C header is parsed)
R502A_PACKET_HEADER_HIGH = 0xEF
R502A_PACKET_HEADER_LOW  = 0x01
R502A_DEFAULT_ADDRESS    = 0xFFFFFFFF

R502A_PID_COMMAND        = 0x01
R502A_PID_DATA           = 0x02
R502A_PID_ACK            = 0x07
R502A_PID_END_DATA       = 0x08

R502A_CMD_HANDSHAKE      = 0x40
R502A_CMD_READ_SYS_PARA  = 0x0F
R502A_CMD_VFY_PWD        = 0x13
R502A_CMD_GET_IMG_EX     = 0x28
R502A_CMD_GEN_CHAR       = 0x02
R502A_CMD_REG_MODEL      = 0x05
R502A_CMD_STORE          = 0x06
R502A_CMD_SEARCH         = 0x04
R502A_CMD_DELETE_CHAR    = 0x0C
R502A_CMD_EMPTY          = 0x0D

R502A_CONF_OK            = 0x00
R502A_CONF_ERR_RECV      = 0x01
R502A_CONF_NO_FINGER     = 0x02
R502A_CONF_FAIL_ENROLL   = 0x03
R502A_CONF_FAIL_GEN_CHAR_DISORDERLY = 0x06
R502A_CONF_FAIL_GEN_CHAR_SMALL_POINT = 0x07
R502A_CONF_FINGER_NOMATCH = 0x08
R502A_CONF_FAIL_FIND_MATCH = 0x09
R502A_CONF_FAIL_COMBINE  = 0x0A
R502A_CONF_ADDR_BEYOND_LIB = 0x0B
R502A_CONF_ERR_READ_TEMPLATE = 0x0C
R502A_CONF_ERR_UPLOAD_TEMPLATE = 0x0D
R502A_CONF_ERR_RECV_FOLLOWING_DATA = 0x0E
R502A_CONF_ERR_UPLOAD_IMAGE = 0x0F
R502A_CONF_FAIL_DELETE_TEMPLATE = 0x10
R502A_CONF_FAIL_CLEAR_LIB = 0x11
R502A_CONF_PWD_FAIL      = 0x13
R502A_CONF_FAIL_GEN_IMAGE_NO_PRIMARY = 0x15
R502A_CONF_ERR_WRITE_FLASH = 0x18
R502A_CONF_TIMEOUT       = 0x26


class FingerprintSensor:
    def __init__(self, port, baudrate=57600, device_address=R502A_DEFAULT_ADDRESS, timeout=1):
        self.port = port
        self.baudrate = baudrate
        self.device_address = device_address
        self.timeout = timeout
        self.ser = None

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
            # Optional: Check if module sends 0x55 on power-on/reset
            # time.sleep(0.1) # Wait for module to initialize
            # initial_byte = self.ser.read(1)
            # if initial_byte == b'\x55':
            #     print("Module ready signal 0x55 received.")
            # else:
            #     print(f"No 0x55 signal, received: {initial_byte}")
            return True
        except serial.SerialException as e:
            print(f"Error opening serial port: {e}")
            self.ser = None
            return False

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.ser = None

    def _calculate_checksum(self, data_bytes):
        return sum(data_bytes) & 0xFFFF

    def _send_command_and_receive_ack(self, cmd_code, params=None):
        if not self.ser or not self.ser.is_open:
            # print("Serial port not open.")
            return R502A_CONF_ERR_RECV, None # Or raise an exception

        if params is None:
            params = b''

        # Construct command packet
        tx_packet = bytearray()
        tx_packet.append(R502A_PACKET_HEADER_HIGH)
        tx_packet.append(R502A_PACKET_HEADER_LOW)
        tx_packet.extend(self.device_address.to_bytes(4, 'big'))
        tx_packet.append(R502A_PID_COMMAND)

        content_len = 1 + len(params)  # cmd_code + params
        package_len = content_len + 2  # content + 2 for checksum
        tx_packet.extend(package_len.to_bytes(2, 'big'))
        
        # Content for checksum calculation
        checksum_content = bytearray()
        checksum_content.append(R502A_PID_COMMAND)
        checksum_content.extend(package_len.to_bytes(2, 'big'))
        checksum_content.append(cmd_code)
        checksum_content.extend(params)

        tx_packet.append(cmd_code)
        tx_packet.extend(params)

        checksum = self._calculate_checksum(checksum_content)
        tx_packet.extend(checksum.to_bytes(2, 'big'))

        self.ser.write(bytes(tx_packet))
        
        # Receive Acknowledge Packet
        # Read Header, Address, PID, Length fields first (9 bytes)
        rx_header_fields = self.ser.read(9)
        if len(rx_header_fields) < 9:
            # print("Timeout or error receiving ACK header.")
            return R502A_CONF_TIMEOUT, None

        if rx_header_fields[0] != R502A_PACKET_HEADER_HIGH or \
           rx_header_fields[1] != R502A_PACKET_HEADER_LOW:
            # print("Invalid ACK header.")
            return R502A_CONF_ERR_RECV, None
        
        # pid = rx_header_fields[6]
        if rx_header_fields[6] != R502A_PID_ACK:
            # print(f"Expected ACK PID, got {rx_header_fields[6]:02X}")
            return R502A_CONF_ERR_RECV, None

        ack_package_fields_len = int.from_bytes(rx_header_fields[7:9], 'big') # Len of (Content + Checksum)

        if ack_package_fields_len < 3: # Min content 1 (ConfCode) + 2 (Checksum)
            # print("Invalid ACK package length.")
            return R502A_CONF_ERR_RECV, None

        # Read the rest of the packet (Content + Checksum)
        ack_content_and_checksum = self.ser.read(ack_package_fields_len)
        if len(ack_content_and_checksum) < ack_package_fields_len:
            # print("Timeout or error receiving ACK content.")
            return R502A_CONF_TIMEOUT, None

        # Validate Checksum
        checksum_calc_data = bytearray()
        checksum_calc_data.append(rx_header_fields[6]) # PID
        checksum_calc_data.extend(rx_header_fields[7:9]) # Length field
        checksum_calc_data.extend(ack_content_and_checksum[:-2]) # Content (ConfCode + Params)
        
        calculated_ack_checksum = self._calculate_checksum(checksum_calc_data)
        received_checksum = int.from_bytes(ack_content_and_checksum[-2:], 'big')

        if received_checksum != calculated_ack_checksum:
            # print("ACK checksum mismatch.")
            return R502A_CONF_ERR_RECV, None

        confirmation_code = ack_content_and_checksum[0]
        ack_params_data = ack_content_and_checksum[1:-2] # Params are after ConfCode, before Checksum

        return confirmation_code, ack_params_data

    def handshake(self):
        """Sends a HandShake command to the module."""
        conf_code, _ = self._send_command_and_receive_ack(R502A_CMD_HANDSHAKE)
        return conf_code

    def verify_password(self, password):
        """Verifies the module's handshake password."""
        # Password is a 4-byte integer
        params = password.to_bytes(4, 'big')
        conf_code, _ = self._send_command_and_receive_ack(R502A_CMD_VFY_PWD, params)
        return conf_code

    def read_system_parameters(self):
        """Reads the system parameters from the module."""
        conf_code, ack_data = self._send_command_and_receive_ack(R502A_CMD_READ_SYS_PARA)
        if conf_code == R502A_CONF_OK and ack_data and len(ack_data) == 16:
            return conf_code, self.SystemParameters(ack_data)
        return conf_code, None

    def get_image_extended(self):
        """Collects a fingerprint image (extended version)."""
        conf_code, _ = self._send_command_and_receive_ack(R502A_CMD_GET_IMG_EX)
        return conf_code

    def generate_character_file(self, buffer_id):
        """
        Generates a character file from the image in ImageBuffer and stores it
        in the specified CharBuffer (1-6).
        """
        if not 1 <= buffer_id <= 6:
            raise ValueError("Buffer ID must be between 1 and 6")
        params = buffer_id.to_bytes(1, 'big')
        conf_code, _ = self._send_command_and_receive_ack(R502A_CMD_GEN_CHAR, params)
        return conf_code

    def generate_template(self):
        """
        Combines character files (e.g., from CharBuffer1 and CharBuffer2)
        to generate a template.
        """
        conf_code, _ = self._send_command_and_receive_ack(R502A_CMD_REG_MODEL)
        return conf_code

    def store_template(self, buffer_id, model_id):
        """
        Stores the template from the specified buffer (e.g., CharBuffer1)
        at the designated location (ModelID) in the Flash library.
        """
        # buffer_id (1 byte), model_id (2 bytes)
        params = bytearray()
        params.append(buffer_id)
        params.extend(model_id.to_bytes(2, 'big'))
        conf_code, _ = self._send_command_and_receive_ack(R502A_CMD_STORE, bytes(params))
        return conf_code

    def search_fingerprint(self, buffer_id, start_page, num_pages):
        """
        Searches the finger library for a template matching the one in CharBufferID.
        """
        # buffer_id (1 byte), start_page (2 bytes), num_pages (2 bytes)
        params = bytearray()
        params.append(buffer_id)
        params.extend(start_page.to_bytes(2, 'big'))
        params.extend(num_pages.to_bytes(2, 'big'))
        
        conf_code, ack_data = self._send_command_and_receive_ack(R502A_CMD_SEARCH, bytes(params))
        if conf_code == R502A_CONF_OK and ack_data and len(ack_data) == 4:
            return conf_code, self.SearchResult(ack_data)
        return conf_code, None
        
    def delete_template(self, start_page, num_to_delete):
        """
        Deletes a specified number of templates from the Flash library.
        """
        # start_page (2 bytes), num_to_delete (2 bytes)
        params = bytearray()
        params.extend(start_page.to_bytes(2, 'big'))
        params.extend(num_to_delete.to_bytes(2, 'big'))
        conf_code, _ = self._send_command_and_receive_ack(R502A_CMD_DELETE_CHAR, bytes(params))
        return conf_code

    def empty_fingerprint_library(self):
        """Deletes all templates from the Flash fingerprint library."""
        conf_code, _ = self._send_command_and_receive_ack(R502A_CMD_EMPTY)
        return conf_code

    # System Parameter structure (for parsing read_system_parameters)
    # Corresponds to r502a_system_params_t in C
    class SystemParameters:
        def __init__(self, data_bytes=None):
            self.status_register = 0
            self.system_identifier_code = 0
            self.finger_library_size = 0
            self.security_level = 0
            self.device_address = 0
            self.data_packet_size_code = 0
            self.baud_rate_N = 0
            if data_bytes and len(data_bytes) == 16:
                self.parse(data_bytes)

        def parse(self, data):
            self.status_register = struct.unpack('>H', data[0:2])[0]
            self.system_identifier_code = struct.unpack('>H', data[2:4])[0]
            self.finger_library_size = struct.unpack('>H', data[4:6])[0]
            self.security_level = struct.unpack('>H', data[6:8])[0]
            self.device_address = struct.unpack('>I', data[8:12])[0]
            self.data_packet_size_code = struct.unpack('>H', data[12:14])[0]
            self.baud_rate_N = struct.unpack('>H', data[14:16])[0]

        def __str__(self):
            return (f"Status: 0x{self.status_register:04X}, SysID: 0x{self.system_identifier_code:04X}, "
                    f"LibSize: {self.finger_library_size}, SecLvl: {self.security_level}, "
                    f"DevAddr: 0x{self.device_address:08X}, PktSizeCode: {self.data_packet_size_code}, "
                    f"BaudN: {self.baud_rate_N}")

    # Search Result structure
    class SearchResult:
        def __init__(self, data_bytes=None):
            self.page_id = 0
            self.match_score = 0
            if data_bytes and len(data_bytes) == 4:
                self.parse(data_bytes)
        
        def parse(self, data):
            self.page_id = struct.unpack('>H', data[0:2])[0]
            self.match_score = struct.unpack('>H', data[2:4])[0]

        def __str__(self):
            return f"PageID: {self.page_id}, Score: {self.match_score}"


if __name__ == '__main__':
    # Example Usage (requires a serial port connected to the R502-A)
    # Replace '/dev/ttyUSB0' or 'COM3' with your actual serial port
    # sensor = FingerprintSensor('/dev/tty.usbserial-0001') # Example for macOS
    # sensor = FingerprintSensor('COM3') # Example for Windows
    
    # print("Attempting to connect...")
    # if sensor.connect():
    #     print("Connected.")
    #     print("Sending Handshake...")
    #     ret_code = sensor.handshake()
    #     print(f"Handshake response: 0x{ret_code:02X} ({'OK' if ret_code == R502A_CONF_OK else 'FAIL'})")
        
    #     if ret_code == R502A_CONF_OK:
    #         # Add more command tests here
    #         pass

    #     sensor.disconnect()
    #     print("Disconnected.")
    # else:
    #     print("Failed to connect.")
    pass