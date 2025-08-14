#include "../include/buffer_manager.h"

void BufferManager::push(const Sample& s) {
    std::lock_guard<std::mutex> lock(mtx);
    buffer.push_back(s);
}

size_t BufferManager::size() {
    std::lock_guard<std::mutex> lock(mtx);
    return buffer.size();
}

std::vector<Sample> BufferManager::get_and_clear() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Sample> out = buffer;
    buffer.clear();
    return out;
}
