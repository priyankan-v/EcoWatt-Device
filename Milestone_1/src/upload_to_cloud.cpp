// upload_to_cloud.cpp
#include "upload_to_cloud.h"
#include <iostream>
using namespace std;

void upload_to_cloud(vector<float> array){
    float sum = 0;
    for (int i = 0; i < array.size(); i++) {
        sum = sum + array[i];
    }
    cout << sum/array.size() << endl;
};