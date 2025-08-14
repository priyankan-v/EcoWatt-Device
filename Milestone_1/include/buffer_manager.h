#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <vector>
#include <mutex>
#include "sampler.h"

class BufferManager {
public:
    void push(const Sample& s);
    size_t size();
    std::vector<Sample> get_and_clear();
private:
    std::vector<Sample> buffer;
    std::mutex mtx;
};

#endif // BUFFER_MANAGER_H
