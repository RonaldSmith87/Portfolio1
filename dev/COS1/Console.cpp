#include "Console.h"
#include <array>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

Console::Console(const std::string& models_dir)
    : model_manager(models_dir) {
}

void Console::print_menu() const {
    std::cout << "\n========================================\n";
    std::cout << " Active Model   : "
        << (active_model.has_value() ? active_model_name + " [" + active_model->summary() + "]" : "None") << "\n";
    std::cout << " Active Dataset : "
        << (active_dataset.has_value()
            ? std::to_string(active_dataset->X.size()) + " train / " +
            std::to_string(active_dataset->X_test.size()) + " val rows, " +
            std::to_string(active_dataset->num_features) + " features"
            : "None") << "\n";
    std::cout << "========================================\n";
    std::cout << "1. Learn\n";
    std::cout << "2. Predict\n";
    std::cout << "3. Load model\n";
    std::cout << "4. Remove model\n";
    std::cout << "5. Load csv from File\n";
    std::cout << "6. Browse & Query ERDDAP API (PMEL)\n";
    std::cout << "7. Load csv from Direct ERDDAP URL\n";
    std::cout << "8. Exit\n";
    std::cout << "Select an option: ";
}

void Console::run() {
    while (true) {
        print_menu();
        int choice = 0;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Invalid input. Please enter a number 1-8.\n";
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        try {
            switch (choice) {
            case 1: handle_learn(); break;
            case 2: handle_predict(); break;
            case 3: handle_load_model(); break;
            case 4: handle_remove_model(); break;
            case 5: handle_load_csv_file(); break;
            case 6: handle_browse_erddap_api(); break;
            case 7: handle_load_erddap_url(); break;
            case 8: std::cout << "Exiting...\n"; return;
            default: std::cout << "Please select a valid option (1-8).\n"; break;
            }
        }
        catch (const std::exception& ex) {
            std::cout << "Error: " << ex.what() << "\n";
        }
    }
}

void Console::handle_learn() {
    if (!active_dataset.has_value()) {
        std::cout << "No dataset loaded. Please load a CSV file or ERDDAP dataset first.\n";
        return;
    }

    if (active_model.has_value()) {
        std::cout << "Continuing training on loaded model: " << active_model_name << "\n";
        size_t epochs = prompt_size_t("Enter additional epochs: ");
        active_model->fit(active_dataset->X, active_dataset->Y, epochs, true,
            active_dataset->X_test, active_dataset->Y_test);
    }
    else {
        std::cout << "No model loaded. Configuring a new neural network...\n";
        NeuralNetwork nn = prompt_new_network(active_dataset->num_features, active_dataset->num_outputs);
        nn.set_metadata(
            active_dataset->feature_names,
            active_dataset->target_name,
            active_dataset->feature_min,
            active_dataset->feature_max
        );
        size_t epochs = prompt_size_t("Enter training epochs: ");
        nn.fit(active_dataset->X, active_dataset->Y, epochs, true,
            active_dataset->X_test, active_dataset->Y_test);
        active_model = std::move(nn);
        active_model_name = "unsaved_model";
    }

    prompt_save_model();
}

void Console::handle_predict() {
    if (!active_model.has_value()) {
        std::cout << "No model loaded. Train or load a model first.\n";
        return;
    }

    const auto& names = active_model->get_feature_names();
    if (names.empty()) {
        std::cout << "Loaded model has no saved feature metadata.\n";
        return;
    }

    std::cout << "\nEnter raw feature values for prediction:\n";
    std::vector<double> raw_inputs(names.size());
    for (size_t i = 0; i < names.size(); ) {
        if (i + 3 < names.size() &&
            names[i].size() > 9 &&
            names[i].substr(names[i].size() - 9) == "_hour_sin") {
            std::string base_label = names[i].substr(0, names[i].size() - 9);
            while (true) {
                std::cout << "  " << base_label << " (ISO-8601, e.g. 2026-11-20T19:00:00Z): ";
                std::string dt_str;
                std::getline(std::cin, dt_str);
                std::array<double, 4> cyc{};
                if (DataLoader::try_parse_iso_datetime(dt_str, cyc)) {
                    raw_inputs[i] = cyc[0];
                    raw_inputs[i + 1] = cyc[1];
                    raw_inputs[i + 2] = cyc[2];
                    raw_inputs[i + 3] = cyc[3];
                    i += 4;
                    break;
                }
                std::cout << "Invalid ISO-8601 timestamp format. Try again.\n";
            }
        }
        else {
            raw_inputs[i] = prompt_any_double("  " + names[i] + ": ");
            ++i;
        }
    }

    auto result = active_model->predict_raw(raw_inputs);
    std::cout << "-> Predicted " << active_model->get_target_name()
        << ": " << result[0] << "\n";
}

void Console::handle_load_model() {
    auto models = model_manager.list_models();
    if (models.empty()) {
        std::cout << "No saved models found.\n";
        return;
    }

    std::cout << "\nAvailable Models:\n";
    for (size_t i = 0; i < models.size(); ++i) {
        std::cout << (i + 1) << ". " << models[i] << "\n";
    }

    size_t choice = prompt_size_t("Select model number to load (0 to cancel): ");
    if (choice == 0 || choice > models.size()) {
        std::cout << "Load cancelled.\n";
        return;
    }

    active_model_name = models[choice - 1];
    active_model = model_manager.load_model(active_model_name);
    std::cout << "Loaded model: " << active_model_name << " (" << active_model->summary() << ")\n";
}

void Console::handle_remove_model() {
    auto models = model_manager.list_models();
    if (models.empty()) {
        std::cout << "No saved models found.\n";
        return;
    }

    std::cout << "\nAvailable Models:\n";
    for (size_t i = 0; i < models.size(); ++i) {
        std::cout << (i + 1) << ". " << models[i] << "\n";
    }

    size_t choice = prompt_size_t("Select model number to remove (0 to cancel): ");
    if (choice == 0 || choice > models.size()) {
        std::cout << "Remove cancelled.\n";
        return;
    }

    std::string target = models[choice - 1];
    if (model_manager.remove_model(target)) {
        std::cout << "Removed model: " << target << "\n";
        if (active_model_name == target) {
            active_model.reset();
            active_model_name.clear();
        }
    }
    else {
        std::cout << "Failed to remove model.\n";
    }
}

void Console::handle_load_csv_file() {
    std::cout << "Enter CSV file path: ";
    std::string filepath;
    std::getline(std::cin, filepath);

    std::string csv_text = DataLoader::read_file_text(filepath);
    auto cols = DataLoader::extract_column_names(csv_text);

    std::cout << "\nColumns found:\n";
    for (size_t i = 0; i < cols.size(); ++i) {
        std::cout << "  " << i << ": " << cols[i] << "\n";
    }

    size_t target_col = prompt_size_t("Select target column index: ");
    size_t lookback = prompt_size_t("Enter past time steps (lookback window, 0 for single-row): ");
    active_dataset = DataLoader::parse_csv_string(csv_text, target_col, lookback);
    std::cout << "Dataset loaded: " << active_dataset->X.size() << " train rows, "
        << active_dataset->X_test.size() << " validation rows, "
        << active_dataset->num_features << " features -> Target: "
        << active_dataset->target_name << "\n";
}

void Console::handle_browse_erddap_api() {
    std::cout << "ERDDAP Base URL [Enter for https://data.pmel.noaa.gov/pmel/erddap]: ";
    std::string base_url;
    std::getline(std::cin, base_url);
    if (base_url.empty()) {
        base_url = "https://data.pmel.noaa.gov/pmel/erddap";
    }

    std::cout << "Search keyword (e.g., TAO, mooring, temperature, wind, saildrone): ";
    std::string query;
    std::getline(std::cin, query);
    if (query.empty()) {
        std::cout << "Search cancelled.\n";
        return;
    }

    std::cout << "Searching ERDDAP catalog...\n";
    auto datasets = DataLoader::search_erddap(base_url, query);
    if (datasets.empty()) {
        std::cout << "No matching datasets found.\n";
        return;
    }

    std::cout << "\nMatching Datasets:\n";
    for (size_t i = 0; i < datasets.size(); ++i) {
        std::cout << "  " << (i + 1) << ". [" << datasets[i].protocol << "] "
            << datasets[i].dataset_id << " - " << datasets[i].title << "\n";
    }

    size_t ds_choice = prompt_size_t("Select dataset number (0 to cancel): ");
    if (ds_choice == 0 || ds_choice > datasets.size()) {
        std::cout << "Selection cancelled.\n";
        return;
    }

    const auto& chosen_ds = datasets[ds_choice - 1];
    std::cout << "Fetching schema for " << chosen_ds.dataset_id << "...\n";
    auto vars = DataLoader::fetch_erddap_variables(base_url, chosen_ds.dataset_id);

    std::cout << "\nAvailable Variables & Dimensions:\n";
    for (size_t i = 0; i < vars.size(); ++i) {
        std::cout << "  " << i << ": " << vars[i].name
            << " (" << vars[i].row_type << ", " << vars[i].data_type << ")";
        if (!vars[i].long_name.empty()) std::cout << " - " << vars[i].long_name;
        if (!vars[i].units.empty()) std::cout << " [" << vars[i].units << "]";
        std::cout << "\n";
    }

    std::cout << "\nEnter comma-separated variable indices to fetch (or 'all' for all variables): ";
    std::string var_input;
    std::getline(std::cin, var_input);

    std::vector<std::string> selected_vars;
    if (var_input == "all" || var_input == "ALL") {
        for (const auto& v : vars) {
            selected_vars.push_back(v.name);
        }
    }
    else {
        std::stringstream ss(var_input);
        std::string item;
        while (std::getline(ss, item, ',')) {
            try {
                size_t idx = std::stoul(item);
                if (idx < vars.size()) {
                    selected_vars.push_back(vars[idx].name);
                }
            }
            catch (...) {}
        }
    }

    if (selected_vars.empty()) {
        std::cout << "No valid variables selected.\n";
        return;
    }

    std::cout << "Start time ISO-8601 (e.g., 2025-01-01T00:00:00Z, or Enter for earliest): ";
    std::string start_iso;
    std::getline(std::cin, start_iso);

    std::cout << "End time ISO-8601 (e.g., 2025-06-01T00:00:00Z, or Enter for latest): ";
    std::string end_iso;
    std::getline(std::cin, end_iso);

    std::string query_url = DataLoader::build_erddap_query_url(
        base_url, chosen_ds, vars, selected_vars, start_iso, end_iso);
    std::cout << "Querying: " << query_url << "\n";

    std::string csv_text = DataLoader::fetch_erddap_csv(query_url);
    auto cols = DataLoader::extract_column_names(csv_text);

    std::cout << "\nDownloaded Columns:\n";
    for (size_t i = 0; i < cols.size(); ++i) {
        std::cout << "  " << i << ": " << cols[i] << "\n";
    }

    size_t target_col = prompt_size_t("Select target column index: ");
    size_t lookback = prompt_size_t("Enter past time steps (lookback window, 0 for single-row): ");
    active_dataset = DataLoader::parse_csv_string(csv_text, target_col, lookback);
    std::cout << "ERDDAP dataset loaded: " << active_dataset->X.size() << " train rows, "
        << active_dataset->X_test.size() << " validation rows, "
        << active_dataset->num_features << " features -> Target: "
        << active_dataset->target_name << "\n";
}

void Console::handle_load_erddap_url() {
    std::cout << "Enter ERDDAP CSV URL: ";
    std::string url;
    std::getline(std::cin, url);

    std::cout << "Downloading CSV from ERDDAP...\n";
    std::string csv_text = DataLoader::fetch_erddap_csv(url);
    auto cols = DataLoader::extract_column_names(csv_text);

    std::cout << "\nColumns found:\n";
    for (size_t i = 0; i < cols.size(); ++i) {
        std::cout << "  " << i << ": " << cols[i] << "\n";
    }

    size_t target_col = prompt_size_t("Select target column index: ");
    size_t lookback = prompt_size_t("Enter past time steps (lookback window, 0 for single-row): ");
    active_dataset = DataLoader::parse_csv_string(csv_text, target_col, lookback);
    std::cout << "ERDDAP dataset loaded: " << active_dataset->X.size() << " train rows, "
        << active_dataset->X_test.size() << " validation rows, "
        << active_dataset->num_features << " features -> Target: "
        << active_dataset->target_name << "\n";
}

size_t Console::prompt_size_t(const std::string& prompt) const {
    size_t val = 0;
    while (true) {
        std::cout << prompt;
        if (std::cin >> val) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return val;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid number. Try again.\n";
    }
}

double Console::prompt_double(const std::string& prompt) const {
    double val = 0.0;
    while (true) {
        std::cout << prompt;
        if (std::cin >> val && val > 0.0) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return val;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid positive number. Try again.\n";
    }
}

double Console::prompt_any_double(const std::string& prompt) const {
    double val = 0.0;
    while (true) {
        std::cout << prompt;
        if (std::cin >> val) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return val;
        }
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid number. Try again.\n";
    }
}

ActivationType Console::prompt_activation() const {
    std::cout << "Select activation function (1=ReLU, 2=Sigmoid, 3=Tanh, 4=Linear): ";
    size_t choice = prompt_size_t("");
    switch (choice) {
    case 1: return ActivationType::ReLU;
    case 2: return ActivationType::Sigmoid;
    case 3: return ActivationType::Tanh;
    case 4: return ActivationType::Linear;
    default:
        std::cout << "Defaulting to Tanh.\n";
        return ActivationType::Tanh;
    }
}

NeuralNetwork Console::prompt_new_network(size_t input_size, size_t output_size) const {
    double lr = prompt_double("Enter learning rate (e.g., 0.05): ");
    size_t num_hidden = prompt_size_t("Enter number of hidden layers: ");
    ActivationType hidden_act = prompt_activation();

    NeuralNetwork nn(lr);
    size_t prev_size = input_size;

    for (size_t i = 0; i < num_hidden; ++i) {
        size_t neurons = prompt_size_t("  Neurons in hidden layer " + std::to_string(i + 1) + ": ");
        nn.add_layer(prev_size, neurons, hidden_act);
        prev_size = neurons;
    }

    std::cout << "Output layer activation:\n";
    ActivationType out_act = prompt_activation();
    nn.add_layer(prev_size, output_size, out_act);

    return nn;
}

void Console::prompt_save_model() {
    std::cout << "Save model to disk? (y/n): ";
    char ans = 'n';
    std::cin >> ans;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (ans == 'y' || ans == 'Y') {
        std::cout << "Enter model name (without extension): ";
        std::string name;
        std::getline(std::cin, name);
        if (!name.empty()) {
            model_manager.save_model(*active_model, name);
            active_model_name = name + ".nn";
            std::cout << "Model saved as " << active_model_name << "\n";
        }
    }
}