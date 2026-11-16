#pragma once
#include <vector>
#include <cstddef>
#include "Activation.h"

class Layer {
public:
    size_t input_size;
    size_t output_size;
    ActivationType act_type;
    Activation activation;

    std::vector<std::vector<double>> weights;
    std::vector<double> biases;

    std::vector<double> last_input;
    std::vector<double> last_z;
    std::vector<double> last_activation;

    Layer(size_t in_size, size_t out_size, ActivationType type);

    std::vector<double> forward(const std::vector<double>& input);
    std::vector<double> backward(const std::vector<double>& grad_output, double learning_rate);
};