// main.cpp
#include "../include/poll_simulator.h"
#include "../include/buffer_data.h"
#include "../include/upload_to_cloud.h"
#include <iostream>
using namespace std;

int main()
{
    sim_dict sim_dict;
    vector<float> voltage;
    vector<float> current;

    for (int i = 0; i <30; i++){
        if (i % 3 == 0) {
            sim_dict = poll_simulator();

            voltage = buffer_data(sim_dict.voltage, voltage);
            current = buffer_data(sim_dict.current, current);
        }

        if (i % 15 == 0){
            cout << "Voltage data at t = " << i << " : ";
            upload_to_cloud(voltage);
            // Reset the buffer after uploading
            voltage = {};

            cout << "Current data at t = " << i << " : ";
            upload_to_cloud(current);
            current = {};
        }
    }

    return 0;
}