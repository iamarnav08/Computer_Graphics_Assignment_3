#include "math_utils.h"

struct Light {
    Vector3f position;
    Vector3f color;
    bool enabled;
    float intensity;
};

const int NUM_LIGHTS = 3;