#include "ModelManager.h"
#include <filesystem>
#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;

ModelManager::ModelManager(const std::string& directory) : models_dir(directory) {
    if (!fs::exists(models_dir)) {
        fs::create_directories(models_dir);
    }
}

std::string ModelManager::build_filepath(const std::string& model_name) const {
    fs::path p = fs::path(models_dir) / model_name;
    if (p.extension() != ".nn") {
        p += ".nn";
    }
    return p.string();
}

std::vector<std::string> ModelManager::list_models() const {
    std::vector<std::string> models;
    if (!fs::exists(models_dir)) {
        return models;
    }

    for (const auto& entry : fs::directory_iterator(models_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".nn") {
            models.push_back(entry.path().filename().string());
        }
    }

    std::sort(models.begin(), models.end());
    return models;
}

void ModelManager::save_model(const NeuralNetwork& nn, const std::string& model_name) const {
    if (model_name.empty()) {
        throw std::invalid_argument("Model name cannot be empty.");
    }
    nn.save(build_filepath(model_name));
}

NeuralNetwork ModelManager::load_model(const std::string& filename) const {
    fs::path p = fs::path(models_dir) / filename;
    if (!fs::exists(p)) {
        throw std::runtime_error("Model file not found: " + p.string());
    }
    return NeuralNetwork::load(p.string());
}

bool ModelManager::remove_model(const std::string& filename) const {
    fs::path p = fs::path(models_dir) / filename;
    if (!fs::exists(p)) {
        return false;
    }
    return fs::remove(p);
}