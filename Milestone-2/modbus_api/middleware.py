from flask import request, jsonify

API_KEY = "ColdPlay2025"

def require_api_key():
    """Validate the API key from the request headers."""
    api_key = request.headers.get("api-key")
    if not api_key or api_key != API_KEY:
        return jsonify({"error": "Unauthorized. Invalid or missing API key."}), 401
    
# function to check CRC of the payload
def check_crc(payload):
    """Check if the CRC in the payload is valid."""
    try:
        data = [int(payload[i:i+2], 16) for i in range(0, len(payload)-4, 2)]
        # received crc is the last two bytes of the payload in little-endian format
        received_crc = int(payload[-2:] + payload[-4:-2], 16)
        calculated_crc = calculate_crc(data)
        return calculated_crc == received_crc
    except Exception as e:
        return False

def calculate_crc(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF