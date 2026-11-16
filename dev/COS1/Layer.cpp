#include "Layer.h"
#include <cmath>
#include <random>
#include <stdexcept>

Layer::Layer(size_t in_size, size_t out_size, ActivationType type)
    : input_size(in_size), output_size(out_size), act_type(type), activation(Activation::get(type)) {

    weights.resize(output_size, std::vector<double>(input_size));
    biases.resize(output_size, 0.0);

    double scale = (act_type == ActivationType::ReLU)
        ? std::sqrt(2.0 / static_cast<double>(in_size))
        : std::sqrt(1.0 / static_cast<double>(in_size));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, scale);

    for (size_t i = 0; i < output_size; ++i) {
        for (size_t j = 0; j < input_size; ++j) {
            weights[i][j] = dist(gen);
        }
    }
}

std::vector<double> Layer::forward(const std::vector<double>& input) {
    if (input.size() != input_size) {
        throw std::invalid_argument("Layer input size mismatch.");
    }
    last_input = input;
    last_z.resize(output_size);
    last_activation.resize(output_size);

    for (size_t i = 0; i < output_size; ++i) {
        double sum = biases[i];
        for (size_t j = 0; j < input_size; ++j) {
            sum += weights[i][j] * input[j];
        }
        last_z[i] = sum;
        last_activation[i] = activation.forward(sum);
    }
    return last_activation;
}

std::vector<double> Layer::backward(const std::vector<double>& grad_output, double learning_rate) {
    std::vector<double> grad_input(input_size, 0.0);

    for (size_t i = 0; i < output_size; ++i) {
        double delta = grad_output[i] * activation.derivative(last_z[i]);

        for (size_t j = 0; j < input_size; ++j) {
            grad_input[j] += weights[i][j] * delta;
            weights[i][j] -= learning_rate * delta * last_input[j];
        }
        biases[i] -= learning_rate * delta;
    }
    return grad_input;
}