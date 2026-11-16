#pragma once
#include <functional>

enum class ActivationType {
    ReLU,
    Sigmoid,
    Tanh,
    Linear
};

struct Activation {
    std::function<double(double)> forward;
    std::function<double(double)> derivative;

    static Activation get(ActivationType type);
};