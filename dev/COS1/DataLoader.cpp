#include "DataLoader.h"
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <cmath>
#include <array>
#include <cstdio>
#include <map>

std::string DataLoader::normalize_base_url(const std::string& base_url) {
    std::string clean = base_url;
    while (!clean.empty() && (clean.back() == '/' || clean.back() == ' ')) {
        clean.pop_back();
    }
    return clean;
}

std::string DataLoader::url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex << std::uppercase;

    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~' || c == ':' || c == ',') {
            escaped << c;
        }
        else {
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    return escaped.str();
}

std::string DataLoader::fetch_http_text(const std::string& url) {
    std::wstring wurl(url.begin(), url.end());
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwHostNameLength = static_cast<DWORD>(-1);
    urlComp.dwUrlPathLength = static_cast<DWORD>(-1);
    urlComp.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.length()), 0, &urlComp)) {
        throw std::runtime_error("Invalid ERDDAP URL format.");
    }

    std::wstring host = urlComp.lpszHostName
        ? std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength)
        : L"";
    std::wstring path = urlComp.lpszUrlPath
        ? std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength)
        : L"";
    if (urlComp.lpszExtraInfo && urlComp.dwExtraInfoLength > 0) {
        path.append(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }

    HINTERNET hSession = WinHttpOpen(L"Portfolio1-NeuralNet/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) throw std::runtime_error("WinHttpOpen failed.");

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("Failed to connect to ERDDAP host.");
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        throw std::runtime_error("WinHttpOpenRequest failed.");
    }

    // Follow HTTP 301/302/307 redirects automatically (needed for NOAA PMEL load balancers)
    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY,
        &redirect_policy, sizeof(redirect_policy));

    std::string response_body;
    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr);

    if (ok) {
        DWORD status_code = 0;
        DWORD status_size = sizeof(status_code);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

        if (status_code != 200) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (status_code == 404) {
                throw std::runtime_error("ERDDAP returned HTTP 404 (No matching datasets or rows found).");
            }
            throw std::runtime_error("ERDDAP server returned HTTP status " + std::to_string(status_code));
        }

        wchar_t content_type[256]{};
        DWORD ct_size = sizeof(content_type);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_TYPE,
            WINHTTP_HEADER_NAME_BY_INDEX,
            content_type, &ct_size, WINHTTP_NO_HEADER_INDEX)) {
            std::wstring wct(content_type);
            if (wct.find(L"text/csv") == std::wstring::npos &&
                wct.find(L"text/plain") == std::wstring::npos) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                throw std::runtime_error("ERDDAP response is not in CSV format.");
            }
        }

        DWORD bytes_available = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
            std::vector<char> buffer(bytes_available);
            DWORD bytes_read = 0;
            if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read)) {
                response_body.append(buffer.data(), bytes_read);
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (!ok || response_body.empty()) {
        throw std::runtime_error("Failed to download data from ERDDAP.");
    }

    return response_body;
}

std::string DataLoader::fetch_erddap_csv(const std::string& url) {
    size_t query_pos = url.find('?');
    std::string base_path = (query_pos != std::string::npos) ? url.substr(0, query_pos) : url;
    if (base_path.size() < 4 || base_path.substr(base_path.size() - 4) != ".csv") {
        throw std::invalid_argument("URL path must end in .csv before query parameters.");
    }
    return fetch_http_text(url);
}

std::vector<ErddapDatasetInfo> DataLoader::search_erddap(const std::string& base_url,
    const std::string& search_query) {
    std::string clean_base = normalize_base_url(base_url);
    std::string search_url = clean_base + "/search/index.csv?page=1&itemsPerPage=25&searchFor=" +
        url_encode(search_query);

    std::string csv_text = fetch_http_text(search_url);
    std::istringstream stream(csv_text);
    std::string line;

    if (!std::getline(stream, line)) {
        throw std::runtime_error("Empty search response from ERDDAP.");
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto headers = split_csv_line(line);

    int col_tabledap = -1, col_griddap = -1, col_title = -1, col_id = -1;
    for (int i = 0; i < static_cast<int>(headers.size()); ++i) {
        if (headers[i] == "tabledap") col_tabledap = i;
        else if (headers[i] == "griddap") col_griddap = i;
        else if (headers[i] == "Title") col_title = i;
        else if (headers[i] == "Dataset ID") col_id = i;
    }

    if (col_id < 0 || col_title < 0) {
        throw std::runtime_error("Unexpected ERDDAP search/index.csv schema.");
    }

    std::vector<ErddapDatasetInfo> results;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tokens = split_csv_line(line);
        if (tokens.size() != headers.size()) continue;

        ErddapDatasetInfo info;
        info.dataset_id = tokens[col_id];
        info.title = tokens[col_title];

        if (col_tabledap >= 0 && !tokens[col_tabledap].empty()) {
            info.protocol = "tabledap";
        }
        else if (col_griddap >= 0 && !tokens[col_griddap].empty()) {
            info.protocol = "griddap";
        }
        else {
            continue;
        }

        if (!info.dataset_id.empty()) {
            results.push_back(std::move(info));
        }
    }

    return results;
}

std::vector<ErddapVariableInfo> DataLoader::fetch_erddap_variables(const std::string& base_url,
    const std::string& dataset_id) {
    std::string clean_base = normalize_base_url(base_url);
    std::string info_url = clean_base + "/info/" + url_encode(dataset_id) + "/index.csv";

    std::string csv_text = fetch_http_text(info_url);
    std::istringstream stream(csv_text);
    std::string line;

    if (!std::getline(stream, line)) {
        throw std::runtime_error("Empty metadata response from ERDDAP.");
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto headers = split_csv_line(line);

    int col_row_type = -1, col_var_name = -1, col_attr_name = -1, col_data_type = -1, col_value = -1;
    for (int i = 0; i < static_cast<int>(headers.size()); ++i) {
        if (headers[i] == "Row Type") col_row_type = i;
        else if (headers[i] == "Variable Name") col_var_name = i;
        else if (headers[i] == "Attribute Name") col_attr_name = i;
        else if (headers[i] == "Data Type") col_data_type = i;
        else if (headers[i] == "Value") col_value = i;
    }

    if (col_row_type < 0 || col_var_name < 0) {
        throw std::runtime_error("Unexpected ERDDAP info/index.csv schema.");
    }

    std::vector<ErddapVariableInfo> vars;
    std::map<std::string, size_t> var_index;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tokens = split_csv_line(line);
        if (tokens.size() != headers.size()) continue;

        const std::string& rtype = tokens[col_row_type];
        const std::string& vname = tokens[col_var_name];

        if (rtype == "variable" || rtype == "dimension") {
            if (var_index.find(vname) == var_index.end()) {
                ErddapVariableInfo vi;
                vi.name = vname;
                vi.row_type = rtype;
                if (col_data_type >= 0) vi.data_type = tokens[col_data_type];
                var_index[vname] = vars.size();
                vars.push_back(std::move(vi));
            }
        }
        else if (rtype == "attribute" && col_attr_name >= 0 && col_value >= 0) {
            auto it = var_index.find(vname);
            if (it != var_index.end()) {
                const std::string& attr = tokens[col_attr_name];
                if (attr == "long_name") vars[it->second].long_name = tokens[col_value];
                else if (attr == "units") vars[it->second].units = tokens[col_value];
            }
        }
    }

    return vars;
}

std::string DataLoader::build_erddap_query_url(const std::string& base_url,
    const ErddapDatasetInfo& ds_info,
    const std::vector<ErddapVariableInfo>& all_vars,
    const std::vector<std::string>& selected_vars,
    const std::string& start_iso,
    const std::string& end_iso) {
    if (selected_vars.empty()) {
        throw std::invalid_argument("At least one variable must be selected.");
    }

    std::string clean_base = normalize_base_url(base_url);

    if (ds_info.protocol == "tabledap") {
        std::string url = clean_base + "/tabledap/" + ds_info.dataset_id + ".csv?";
        for (size_t i = 0; i < selected_vars.size(); ++i) {
            if (i > 0) url += ",";
            url += selected_vars[i];
        }
        if (!start_iso.empty()) {
            url += "&time%3E=" + url_encode(start_iso);
        }
        if (!end_iso.empty()) {
            url += "&time%3C=" + url_encode(end_iso);
        }
        return url;
    }
    else {
        std::string dim_slice;
        for (const auto& v : all_vars) {
            if (v.row_type != "dimension") continue;
            if (v.name == "time") {
                std::string t_start = start_iso.empty() ? "last" : ("(" + start_iso + ")");
                std::string t_end = end_iso.empty() ? "last" : ("(" + end_iso + ")");
                if (start_iso.empty() && end_iso.empty()) {
                    dim_slice += "[(last)]";
                }
                else {
                    dim_slice += "[" + t_start + ":1:" + t_end + "]";
                }
            }
            else {
                dim_slice += "[(last)]";
            }
        }

        std::string url = clean_base + "/griddap/" + ds_info.dataset_id + ".csv?";
        bool first = true;
        for (const auto& sv : selected_vars) {
            bool is_dim = false;
            for (const auto& v : all_vars) {
                if (v.name == sv && v.row_type == "dimension") {
                    is_dim = true;
                    break;
                }
            }
            if (is_dim) continue;

            if (!first) url += ",";
            url += sv + dim_slice;
            first = false;
        }
        if (first) {
            throw std::invalid_argument("For griddap datasets, select at least one data variable (not just dimensions).");
        }
        return url;
    }
}

std::string DataLoader::read_file_text(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open CSV file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Quote-aware RFC-4180 CSV line splitter (handles commas inside quoted ERDDAP titles/attributes)
std::vector<std::string> DataLoader::split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            }
            else {
                in_quotes = !in_quotes;
            }
        }
        else if (c == ',' && !in_quotes) {
            tokens.push_back(std::move(current));
            current.clear();
        }
        else {
            current.push_back(c);
        }
    }
    tokens.push_back(std::move(current));
    return tokens;
}

bool DataLoader::try_parse_double(const std::string& str, double& out_val) {
    if (str.empty() || str == "NaN" || str == "nan" || str == "NA") return false;
    try {
        size_t idx = 0;
        double val = std::stod(str, &idx);
        if (idx != str.size() || std::isnan(val) || std::isinf(val)) return false;
        out_val = val;
        return true;
    }
    catch (...) {
        return false;
    }
}

bool DataLoader::try_parse_iso_datetime(const std::string& str, std::array<double, 4>& cyclical_out) {
    if (str.size() < 19) return false;
    if (str[4] != '-' || str[7] != '-' || (str[10] != 'T' && str[10] != ' ') ||
        str[13] != ':' || str[16] != ':') {
        return false;
    }

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    try {
        year = std::stoi(str.substr(0, 4));
        month = std::stoi(str.substr(5, 2));
        day = std::stoi(str.substr(8, 2));
        hour = std::stoi(str.substr(11, 2));
        minute = std::stoi(str.substr(14, 2));
        second = std::stoi(str.substr(17, 2));
    }
    catch (...) {
        return false;
    }

    if (month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
        return false;
    }

    static const int days_before_month[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    bool is_leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    int doy = days_before_month[month - 1] + day + ((is_leap && month > 2) ? 1 : 0);

    constexpr double two_pi = 6.28318530717958647692;
    double fractional_hour = static_cast<double>(hour) +
        static_cast<double>(minute) / 60.0 +
        static_cast<double>(second) / 3600.0;

    double hour_angle = two_pi * (fractional_hour / 24.0);
    double doy_angle = two_pi * (static_cast<double>(doy - 1) / (is_leap ? 366.0 : 365.0));

    cyclical_out[0] = std::sin(hour_angle);
    cyclical_out[1] = std::cos(hour_angle);
    cyclical_out[2] = std::sin(doy_angle);
    cyclical_out[3] = std::cos(doy_angle);
    return true;
}

std::vector<std::string> DataLoader::extract_column_names(const std::string& csv_content) {
    std::istringstream stream(csv_content);
    std::string header_line;
    if (!std::getline(stream, header_line)) {
        throw std::runtime_error("CSV content is empty.");
    }
    if (!header_line.empty() && header_line.back() == '\r') header_line.pop_back();
    return split_csv_line(header_line);
}

Dataset DataLoader::parse_csv_string(const std::string& csv_content,
    size_t target_col_index,
    size_t lookback_steps) {
    std::istringstream stream(csv_content);
    std::string line;

    if (!std::getline(stream, line)) {
        throw std::runtime_error("CSV content is empty.");
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::vector<std::string> headers = split_csv_line(line);

    if (target_col_index >= headers.size()) {
        throw std::out_of_range("Target column index out of range.");
    }

    std::vector<std::vector<std::string>> raw_rows;
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tokens = split_csv_line(line);
        if (tokens.size() == headers.size()) {
            raw_rows.push_back(std::move(tokens));
        }
    }

    if (raw_rows.empty()) {
        throw std::runtime_error("No data rows found in CSV.");
    }

    enum class ColType { Ignore, Numeric, DateTime };
    size_t sample_row_idx = (raw_rows.size() > 1) ? 1 : 0;
    std::vector<ColType> col_types(headers.size(), ColType::Ignore);
    int first_dt_col = -1;

    for (size_t c = 0; c < headers.size(); ++c) {
        double dummy_num = 0.0;
        std::array<double, 4> dummy_dt{};
        if (try_parse_double(raw_rows[sample_row_idx][c], dummy_num)) {
            col_types[c] = ColType::Numeric;
        }
        else if (try_parse_iso_datetime(raw_rows[sample_row_idx][c], dummy_dt)) {
            col_types[c] = ColType::DateTime;
            if (first_dt_col < 0) first_dt_col = static_cast<int>(c);
        }
    }

    if (col_types[target_col_index] != ColType::Numeric) {
        throw std::invalid_argument("Selected target column '" + headers[target_col_index] + "' must be numeric.");
    }

    std::vector<std::string> step_feature_names;
    for (size_t c = 0; c < headers.size(); ++c) {
        if (c == target_col_index) continue;
        if (col_types[c] == ColType::DateTime) {
            step_feature_names.push_back(headers[c] + "_hour_sin");
            step_feature_names.push_back(headers[c] + "_hour_cos");
            step_feature_names.push_back(headers[c] + "_doy_sin");
            step_feature_names.push_back(headers[c] + "_doy_cos");
        }
        else if (col_types[c] == ColType::Numeric) {
            step_feature_names.push_back(headers[c]);
        }
    }

    if (step_feature_names.empty() && lookback_steps == 0) {
        throw std::runtime_error("No numeric or datetime feature columns found alongside target.");
    }

    std::vector<std::vector<double>> step_X;
    std::vector<double> step_Y;
    std::vector<std::string> valid_timestamps;

    for (const auto& row : raw_rows) {
        double target_val = 0.0;
        if (!try_parse_double(row[target_col_index], target_val)) continue;

        std::vector<double> features;
        bool valid_row = true;

        for (size_t c = 0; c < headers.size(); ++c) {
            if (c == target_col_index) continue;

            if (col_types[c] == ColType::DateTime) {
                std::array<double, 4> dt_feats{};
                if (!try_parse_iso_datetime(row[c], dt_feats)) {
                    valid_row = false;
                    break;
                }
                features.insert(features.end(), dt_feats.begin(), dt_feats.end());
            }
            else if (col_types[c] == ColType::Numeric) {
                double val = 0.0;
                if (!try_parse_double(row[c], val)) {
                    valid_row = false;
                    break;
                }
                features.push_back(val);
            }
        }

        if (valid_row) {
            if (first_dt_col >= 0) {
                valid_timestamps.push_back(row[first_dt_col]);
            }
            step_X.push_back(std::move(features));
            step_Y.push_back(target_val);
        }
    }

    // Ensure ascending chronological order (oldest -> newest) for lookback windows
    if (valid_timestamps.size() >= 2 && valid_timestamps.front() > valid_timestamps.back()) {
        std::reverse(step_X.begin(), step_X.end());
        std::reverse(step_Y.begin(), step_Y.end());
    }

    if (step_X.size() <= lookback_steps) {
        throw std::runtime_error("Not enough valid rows (" + std::to_string(step_X.size()) +
            ") for lookback window of " + std::to_string(lookback_steps) + " steps.");
    }

    Dataset data;
    data.target_name = headers[target_col_index];

    if (lookback_steps == 0) {
        data.feature_names = std::move(step_feature_names);
        data.X = std::move(step_X);
        data.Y.reserve(step_Y.size());
        for (double y : step_Y) {
            data.Y.push_back({ y });
        }
    }
    else {
        data.feature_names = step_feature_names;
        for (size_t lag = lookback_steps; lag >= 1; --lag) {
            std::string suffix = "_lag" + std::to_string(lag);
            for (const auto& fname : step_feature_names) {
                data.feature_names.push_back(fname + suffix);
            }
            data.feature_names.push_back(data.target_name + suffix);
        }

        for (size_t t = lookback_steps; t < step_X.size(); ++t) {
            std::vector<double> window_row = step_X[t];
            for (size_t lag = lookback_steps; lag >= 1; --lag) {
                const auto& past_x = step_X[t - lag];
                window_row.insert(window_row.end(), past_x.begin(), past_x.end());
                window_row.push_back(step_Y[t - lag]);
            }
            data.X.push_back(std::move(window_row));
            data.Y.push_back({ step_Y[t] });
        }
    }

    // Chronological 80/20 holdout split (last 20% reserved for validation)
    size_t total_rows = data.X.size();
    if (total_rows >= 5) {
        size_t split_idx = static_cast<size_t>(total_rows * 0.8);
        data.X_test.assign(data.X.begin() + split_idx, data.X.end());
        data.Y_test.assign(data.Y.begin() + split_idx, data.Y.end());
        data.X.resize(split_idx);
        data.Y.resize(split_idx);
    }

    data.num_features = data.X[0].size();
    data.num_outputs = 1;
    normalize_features(data);
    return data;
}

void DataLoader::normalize_features(Dataset& data) {
    size_t cols = data.num_features;
    data.feature_min.assign(cols, std::numeric_limits<double>::max());
    data.feature_max.assign(cols, std::numeric_limits<double>::lowest());

    for (const auto& row : data.X) {
        for (size_t c = 0; c < cols; ++c) {
            data.feature_min[c] = std::min(data.feature_min[c], row[c]);
            data.feature_max[c] = std::max(data.feature_max[c], row[c]);
        }
    }

    auto scale_matrix = [&](std::vector<std::vector<double>>& matrix) {
        for (auto& row : matrix) {
            for (size_t c = 0; c < cols; ++c) {
                double range = data.feature_max[c] - data.feature_min[c];
                row[c] = (range > 1e-12) ? (row[c] - data.feature_min[c]) / range : 0.0;
            }
        }
        };

    scale_matrix(data.X);
    scale_matrix(data.X_test);
}