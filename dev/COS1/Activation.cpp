#include "Activation.h"
#include <cmath>

Activation Activation::get(ActivationType type) {
    switch (type) {
    case ActivationType::ReLU:
        return {
            [](double x) { return x > 0.0 ? x : 0.0; },
            [](double x) { return x > 0.0 ? 1.0 : 0.0; }
        };
    case ActivationType::Sigmoid:
        return {
            [](double x) { return 1.0 / (1.0 + std::exp(-x)); },
            [](double x) {
                double s = 1.0 / (1.0 + std::exp(-x));
                return s * (1.0 - s);
            }
        };
    case ActivationType::Tanh:
        return {
            [](double x) { return std::tanh(x); },
            [](double x) {
                double t = std::tanh(x);
                return 1.0 - t * t;
            }
        };
    case ActivationType::Linear:
    default:
        return {
            [](double x) { return x; },
            [](double x) { return 1.0; }
        };
    }
}