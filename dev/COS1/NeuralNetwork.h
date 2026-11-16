#pragma once
#include <vector>
#include <string>
#include <cstddef>
#include "Layer.h"

class NeuralNetwork {
private:
    std::vector<Layer> layers;
    double learning_rate;

    // Dataset metadata & normalization bounds
    std::vector<std::string> feature_names;
    std::string target_name = "target";
    std::vector<double> feature_min;
    std::vector<double> feature_max;

public:
    explicit NeuralNetwork(double lr = 0.01);

    void add_layer(size_t input_size, size_t output_size, ActivationType act_type);

    // Stores column names and min/max scaling bounds from Dataset
    void set_metadata(const std::vector<std::string>& feat_names,
        const std::string& tgt_name,
        const std::vector<double>& f_min,
        const std::vector<double>& f_max);

    const std::vector<std::string>& get_feature_names() const { return feature_names; }
    const std::string& get_target_name() const { return target_name; }

    // Expects already-normalized inputs (used during training)
    std::vector<double> predict(const std::vector<double>& input);

    // Auto-scales raw user inputs using saved feature_min/feature_max before predicting
    std::vector<double> predict_raw(const std::vector<double>& raw_input);

    double train_step(const std::vector<double>& input, const std::vector<double>& target);
    void fit(const std::vector<std::vector<double>>& X,
        const std::vector<std::vector<double>>& Y,
        size_t epochs,
        bool verbose = true);

    void save(const std::string& filepath) const;
    static NeuralNetwork load(const std::string& filepath);
    std::string summary() const;
};