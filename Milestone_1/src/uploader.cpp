#include "../include/uploader.h"
#include <iostream>
#include <thread>

void upload_buffer(const std::vector<Sample>& buffer) {
    std::cout << "[Upload Timer = 15s] tick -> Upload Ready\n";
    std::cout << "Uploading (" << buffer.size() << " samples)\n";
    // std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Simulate upload
    std::cout << "Received ACK <- Idle\n";
}
