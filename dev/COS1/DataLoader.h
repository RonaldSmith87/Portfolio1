#pragma once
#include <string>
#include <vector>
#include <cstddef>

struct Dataset {
    std::vector<std::vector<double>> X;
    std::vector<std::vector<double>> Y;
    std::vector<std::string> feature_names;
    std::string target_name;
    std::vector<double> feature_min;
    std::vector<double> feature_max;
    size_t num_features = 0;
    size_t num_outputs = 1;
};

class DataLoader {
public:
    // Fetches raw CSV text over HTTP/HTTPS using Windows WinHTTP and verifies CSV format
    static std::string fetch_erddap_csv(const std::string& url);

    // Reads raw CSV text from a local file
    static std::string read_file_text(const std::string& filepath);

    // Extracts column names from row 1 so the user can choose a target column
    static std::vector<std::string> extract_column_names(const std::string& csv_content);

    // Parses CSV text, keeps only numeric columns (ignoring strings/timestamps/NaNs), and normalizes X
    static Dataset parse_csv_string(const std::string& csv_content, size_t target_col_index);

private:
    static std::vector<std::string> split_csv_line(const std::string& line);
    static bool try_parse_double(const std::string& str, double& out_val);
    static void normalize_features(Dataset& data);
};