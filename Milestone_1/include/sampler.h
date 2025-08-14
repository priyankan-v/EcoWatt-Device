#ifndef SAMPLER_H
#define SAMPLER_H

struct Sample {
    double t;
    double voltage;
    double current;
    double power;
};

Sample acquire_sample();

#endif // SAMPLER_H
