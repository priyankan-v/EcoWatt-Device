#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include "../include/sampler.h"
#include "../include/buffer_manager.h"
#include "../include/uploader.h"

void print_sample(const Sample& s) {
    std::cout << "Sample Ready {'t': " << std::fixed << std::setprecision(2) << s.t
              << ", 'voltage': " << s.voltage
              << ", 'current': " << s.current
              << ", 'power': " << s.power << "}\n";
}

int main() {
    const int poll_interval_ms = 3000;
    const int upload_interval_ms = 15000;
    const int buffer_max = 5;

    BufferManager bufferManager;
    auto last_upload = std::chrono::steady_clock::now();

    while (true) {
        std::cout << "Idle started\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
        std::cout << "[Poll Timer = 3s] tick -> Poll Ready\n";
        std::cout << "Not Uploading = Polling\n";

        Sample s = acquire_sample();
        print_sample(s);
        bufferManager.push(s);
        std::cout << "Pushed | Buffer size = " << bufferManager.size() << "\n";

        if (bufferManager.size() >= buffer_max ||
            std::chrono::steady_clock::now() - last_upload > std::chrono::milliseconds(upload_interval_ms)) {
            auto buf = bufferManager.get_and_clear();
            upload_buffer(buf);
            last_upload = std::chrono::steady_clock::now();
        }
    }
    return 0;
}