// buffer_data.cpp
#include "buffer_data.h"

vector<float> buffer_data(float value, vector<float> array) {
    array.push_back(value);

    return array;
}