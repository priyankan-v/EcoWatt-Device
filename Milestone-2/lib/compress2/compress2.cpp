#include "compress2.h"

uint16_t compress_buffer(const register_reading_t* buffer, size_t count, uint8_t* output) {
    if (count == 0 || buffer == nullptr || output == nullptr) {
        return 0; // Invalid input
    }

    size_t output_index = 0;

    for (size_t i = 0; i < count; i++) {
        const register_reading_t& reading = buffer[i];

        for (size_t j = 0; j < READ_REGISTER_COUNT; j++) {
            uint16_t value = reading.values[j];
            uint8_t high_byte = (value >> 8) & 0xFF;
            uint8_t low_byte = value & 0xFF;

            // Add the high and low bytes to the output
            if (output_index + 2 > MAX_COMPRESSION_SIZE) {
                return 0; // Output buffer overflow
            }
            output[output_index++] = high_byte;
            output[output_index++] = low_byte;
        }

        // Add timestamp (4 bytes)
        uint32_t timestamp = reading.timestamp;
        for (int k = 0; k < 4; k++) {
            if (output_index + 1 > MAX_COMPRESSION_SIZE) {
                return 0; // Output buffer overflow
            }
            output[output_index++] = (timestamp >> (8 * (3 - k))) & 0xFF;
        }
    }

    return output_index; // Return the size of the compressed data
}
