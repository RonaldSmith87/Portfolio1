#pragma once
#include <string>
#include <vector>
#include "NeuralNetwork.h"

class ModelManager {
private:
    std::string models_dir;

    std::string build_filepath(const std::string& model_name) const;

public:
    explicit ModelManager(const std::string& directory = "models");

    // Returns sorted list of saved model filenames (without directory path)
    std::vector<std::string> list_models() const;

    // Saves a network under models/<model_name>.nn
    void save_model(const NeuralNetwork& nn, const std::string& model_name) const;

    // Loads a network by filename (e.g., "iris.nn")
    NeuralNetwork load_model(const std::string& filename) const;

    // Deletes a model file by filename; returns true if removed
    bool remove_model(const std::string& filename) const;
};