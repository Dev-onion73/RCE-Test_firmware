#ifndef TEST_MATRIX_H
#define TEST_MATRIX_H

#include <Arduino.h>
#include <stdint.h>

namespace Radio {
namespace TestMatrix {

static constexpr size_t MAX_VALUES = 32;

struct Configuration {
    float frequency[MAX_VALUES];
    size_t frequencyCount;

    uint8_t spreadingFactor[MAX_VALUES];
    size_t spreadingFactorCount;

    float bandwidth[MAX_VALUES];
    size_t bandwidthCount;

    uint8_t codingRate[MAX_VALUES];
    size_t codingRateCount;

    int8_t txPower[MAX_VALUES];
    size_t txPowerCount;
};

void begin();
void feature();

bool load();
bool save();

uint64_t configurationCount();

const Configuration& getConfiguration();

}

}

#endif