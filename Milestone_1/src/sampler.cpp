#include "../include/sampler.h"
#include <random>
#include <chrono>

Sample acquire_sample() {
    static std::default_random_engine eng{std::random_device{}()};
    static std::uniform_real_distribution<double> v_dist(210, 240), c_dist(0.2, 2.0);
    double t = std::chrono::system_clock::now().time_since_epoch().count() / 1e9;
    double voltage = v_dist(eng);
    double current = c_dist(eng);
    double power = voltage * current;
    return {t, voltage, current, power};
}
