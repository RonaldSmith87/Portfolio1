#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstddef>

struct Dataset {
    std::vector<std::vector<double>> X;      // 80% Training features
    std::vector<std::vector<double>> Y;      // 80% Training targets
    std::vector<std::vector<double>> X_test; // 20% Holdout validation features
    std::vector<std::vector<double>> Y_test; // 20% Holdout validation targets
    std::vector<std::string> feature_names;
    std::string target_name;
    std::vector<double> feature_min;
    std::vector<double> feature_max;
    size_t num_features = 0;
    size_t num_outputs = 1;
};

struct ErddapDatasetInfo {
    std::string dataset_id;
    std::string title;
    std::string protocol; // "tabledap" or "griddap"
};

struct ErddapVariableInfo {
    std::string name;
    std::string row_type;  // "variable" or "dimension"
    std::string data_type;
    std::string long_name;
    std::string units;
};

class DataLoader {
public:
    // Fetches raw CSV text from a direct ERDDAP .csv URL
    static std::string fetch_erddap_csv(const std::string& url);

    // Searches an ERDDAP server (e.g. https://data.pmel.noaa.gov/pmel/erddap) for datasets
    static std::vector<ErddapDatasetInfo> search_erddap(const std::string& base_url,
        const std::string& search_query);

    // Fetches variables, dimensions, data types, and units for a given dataset ID
    static std::vector<ErddapVariableInfo> fetch_erddap_variables(const std::string& base_url,
        const std::string& dataset_id);

    // Builds a tabledap or griddap .csv query URL with optional ISO-8601 time constraints
    static std::string build_erddap_query_url(const std::string& base_url,
        const ErddapDatasetInfo& ds_info,
        const std::vector<ErddapVariableInfo>& all_vars,
        const std::vector<std::string>& selected_vars,
        const std::string& start_iso = "",
        const std::string& end_iso = "");

    // Reads raw CSV text from a local file
    static std::string read_file_text(const std::string& filepath);

    // Extracts column names from row 1 so the user can choose a target column
    static std::vector<std::string> extract_column_names(const std::string& csv_content);

    // Parses CSV text, encodes ISO-8601 datetime columns cyclically, builds lookback windows, splits 80/20, and normalizes
    static Dataset parse_csv_string(const std::string& csv_content,
        size_t target_col_index,
        size_t lookback_steps = 0);

    // Parses ISO-8601 string ("YYYY-MM-DDTHH:MM:SSZ") into [hour_sin, hour_cos, doy_sin, doy_cos]
    static bool try_parse_iso_datetime(const std::string& str, std::array<double, 4>& cyclical_out);

private:
    static std::string fetch_http_text(const std::string& url);
    static std::string normalize_base_url(const std::string& base_url);
    static std::string url_encode(const std::string& value);
    static std::vector<std::string> split_csv_line(const std::string& line);
    static bool try_parse_double(const std::string& str, double& out_val);
    static void normalize_features(Dataset& data);
};