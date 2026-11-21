#pragma once
#include <optional>
#include <string>
#include "NeuralNetwork.h"
#include "DataLoader.h"
#include "ModelManager.h"

class Console {
private:
    std::optional<NeuralNetwork> active_model;
    std::string active_model_name;
    std::optional<Dataset> active_dataset;
    ModelManager model_manager;

    void print_menu() const;
    void handle_learn();
    void handle_predict();
    void handle_load_model();
    void handle_remove_model();
    void handle_load_csv_file();
    void handle_load_erddap_url();
    void handle_browse_erddap_api();

    size_t prompt_size_t(const std::string& prompt) const;
    double prompt_double(const std::string& prompt) const;
    double prompt_any_double(const std::string& prompt) const;
    ActivationType prompt_activation() const;
    NeuralNetwork prompt_new_network(size_t input_size, size_t output_size) const;
    void prompt_save_model();

public:
    explicit Console(const std::string& models_dir = "models");
    void run();
};