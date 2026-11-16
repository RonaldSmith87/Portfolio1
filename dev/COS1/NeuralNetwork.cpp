#include "NeuralNetwork.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

NeuralNetwork::NeuralNetwork(double lr) : learning_rate(lr) {}

void NeuralNetwork::add_layer(size_t input_size, size_t output_size, ActivationType act_type) {
    if (!layers.empty() && layers.back().output_size != input_size) {
        throw std::invalid_argument("New layer input_size must match previous layer output_size.");
    }
    layers.emplace_back(input_size, output_size, act_type);
}

void NeuralNetwork::set_metadata(const std::vector<std::string>& feat_names,
    const std::string& tgt_name,
    const std::vector<double>& f_min,
    const std::vector<double>& f_max) {
    if (feat_names.size() != f_min.size() || f_min.size() != f_max.size()) {
        throw std::invalid_argument("Metadata vector sizes do not match.");
    }
    feature_names = feat_names;
    target_name = tgt_name;
    feature_min = f_min;
    feature_max = f_max;
}

std::vector<double> NeuralNetwork::predict(const std::vector<double>& input) {
    std::vector<double> current = input;
    for (auto& layer : layers) {
        current = layer.forward(current);
    }
    return current;
}

std::vector<double> NeuralNetwork::predict_raw(const std::vector<double>& raw_input) {
    if (feature_min.empty() || raw_input.size() != feature_min.size()) {
        throw std::invalid_argument("Raw input size does not match model feature metadata.");
    }

    std::vector<double> scaled(raw_input.size());
    for (size_t i = 0; i < raw_input.size(); ++i) {
        double range = feature_max[i] - feature_min[i];
        scaled[i] = (range > 1e-12) ? (raw_input[i] - feature_min[i]) / range : 0.0;
    }

    return predict(scaled);
}

double NeuralNetwork::train_step(const std::vector<double>& input, const std::vector<double>& target) {
    std::vector<double> output = predict(input);
    if (output.size() != target.size()) {
        throw std::invalid_argument("Target size mismatch with network output.");
    }

    double loss = 0.0;
    std::vector<double> grad(output.size());
    for (size_t i = 0; i < output.size(); ++i) {
        double error = output[i] - target[i];
        loss += 0.5 * error * error;
        grad[i] = error;
    }

    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
        grad = layers[i].backward(grad, learning_rate);
    }

    return loss;
}

void NeuralNetwork::fit(const std::vector<std::vector<double>>& X,
    const std::vector<std::vector<double>>& Y,
    size_t epochs,
    bool verbose) {
    for (size_t epoch = 0; epoch < epochs; ++epoch) {
        double total_loss = 0.0;
        for (size_t i = 0; i < X.size(); ++i) {
            total_loss += train_step(X[i], Y[i]);
        }
        if (verbose && (epoch % 500 == 0 || epoch == epochs - 1)) {
            std::cout << "Epoch " << epoch + 1 << "/" << epochs
                << " - Loss: " << (total_loss / X.size()) << "\n";
        }
    }
}

void NeuralNetwork::save(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open file for saving model: " + filepath);
    }

    out << std::setprecision(17);
    out << learning_rate << " " << layers.size() << " " << feature_names.size() << "\n";

    // Write target name and per-feature metadata (name, min, max)
    out << std::quoted(target_name) << "\n";
    for (size_t i = 0; i < feature_names.size(); ++i) {
        out << std::quoted(feature_names[i]) << " "
            << feature_min[i] << " "
            << feature_max[i] << "\n";
    }

    // Write layers
    for (const auto& layer : layers) {
        out << layer.input_size << " "
            << layer.output_size << " "
            << static_cast<int>(layer.act_type) << "\n";

        for (size_t i = 0; i < layer.output_size; ++i) {
            out << layer.biases[i] << (i + 1 < layer.output_size ? " " : "\n");
        }

        for (size_t i = 0; i < layer.output_size; ++i) {
            for (size_t j = 0; j < layer.input_size; ++j) {
                out << layer.weights[i][j] << (j + 1 < layer.input_size ? " " : "\n");
            }
        }
    }
}

NeuralNetwork NeuralNetwork::load(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open model file: " + filepath);
    }

    double lr = 0.01;
    size_t num_layers = 0;
    size_t num_features = 0;
    if (!(in >> lr >> num_layers >> num_features)) {
        throw std::runtime_error("Corrupted model file header.");
    }

    NeuralNetwork nn(lr);

    // Read target name and per-feature metadata
    std::string tgt_name;
    if (!(in >> std::quoted(tgt_name))) {
        throw std::runtime_error("Corrupted target metadata in model file.");
    }

    std::vector<std::string> f_names(num_features);
    std::vector<double> f_min(num_features);
    std::vector<double> f_max(num_features);
    for (size_t i = 0; i < num_features; ++i) {
        if (!(in >> std::quoted(f_names[i]) >> f_min[i] >> f_max[i])) {
            throw std::runtime_error("Corrupted feature bounds in model file.");
        }
    }
    nn.set_metadata(f_names, tgt_name, f_min, f_max);

    // Read layers
    for (size_t l = 0; l < num_layers; ++l) {
        size_t in_size = 0, out_size = 0;
        int act_int = 0;
        if (!(in >> in_size >> out_size >> act_int)) {
            throw std::runtime_error("Corrupted layer metadata in model file.");
        }

        Layer layer(in_size, out_size, static_cast<ActivationType>(act_int));

        for (size_t i = 0; i < out_size; ++i) {
            if (!(in >> layer.biases[i])) {
                throw std::runtime_error("Corrupted bias data in model file.");
            }
        }

        for (size_t i = 0; i < out_size; ++i) {
            for (size_t j = 0; j < in_size; ++j) {
                if (!(in >> layer.weights[i][j])) {
                    throw std::runtime_error("Corrupted weight data in model file.");
                }
            }
        }

        nn.layers.push_back(std::move(layer));
    }

    return nn;
}

std::string NeuralNetwork::summary() const {
    if (layers.empty()) return "Empty network (0 layers)";
    std::ostringstream oss;
    oss << layers.front().input_size;
    for (const auto& layer : layers) {
        oss << " -> " << layer.output_size;
    }
    oss << " | Target: " << target_name << " (lr=" << learning_rate << ")";
    return oss.str();
}