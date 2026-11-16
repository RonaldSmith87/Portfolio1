#include "DataLoader.h"
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <cmath>

std::string DataLoader::fetch_erddap_csv(const std::string& url) {
    // 1. Validate URL format before making network calls
    size_t query_pos = url.find('?');
    std::string base_path = (query_pos != std::string::npos) ? url.substr(0, query_pos) : url;
    if (base_path.size() < 4 || base_path.substr(base_path.size() - 4) != ".csv") {
        throw std::invalid_argument("URL path must end in .csv before query parameters.");
    }

    // 2. Crack URL into host, port, and path using WinHTTP
    std::wstring wurl(url.begin(), url.end());
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwHostNameLength = static_cast<DWORD>(-1);
    urlComp.dwUrlPathLength = static_cast<DWORD>(-1);
    urlComp.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.length()), 0, &urlComp)) {
        throw std::runtime_error("Invalid ERDDAP URL format.");
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
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

    std::string response_body;
    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr);

    if (ok) {
        // Check HTTP status code
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
            throw std::runtime_error("ERDDAP server returned HTTP status " + std::to_string(status_code));
        }

        // Verify Content-Type is CSV/text and not an HTML error page
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
                throw std::runtime_error("Response is not in CSV format.");
            }
        }

        // Read response stream
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
        throw std::runtime_error("Failed to download CSV data from ERDDAP URL.");
    }

    return response_body;
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

std::vector<std::string> DataLoader::split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, ',')) {
        tokens.push_back(cell);
    }
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

std::vector<std::string> DataLoader::extract_column_names(const std::string& csv_content) {
    std::istringstream stream(csv_content);
    std::string header_line;
    if (!std::getline(stream, header_line)) {
        throw std::runtime_error("CSV content is empty.");
    }
    if (!header_line.empty() && header_line.back() == '\r') header_line.pop_back();
    return split_csv_line(header_line);
}

Dataset DataLoader::parse_csv_string(const std::string& csv_content, size_t target_col_index) {
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

    // Detect which columns are numeric by inspecting the first non-units data row
    size_t sample_row_idx = (raw_rows.size() > 1) ? 1 : 0;
    std::vector<bool> is_numeric_col(headers.size(), false);
    for (size_t c = 0; c < headers.size(); ++c) {
        double dummy = 0.0;
        is_numeric_col[c] = try_parse_double(raw_rows[sample_row_idx][c], dummy);
    }

    if (!is_numeric_col[target_col_index]) {
        throw std::invalid_argument("Selected target column '" + headers[target_col_index] + "' is not numeric.");
    }

    Dataset data;
    data.target_name = headers[target_col_index];
    for (size_t c = 0; c < headers.size(); ++c) {
        if (c != target_col_index && is_numeric_col[c]) {
            data.feature_names.push_back(headers[c]);
        }
    }

    if (data.feature_names.empty()) {
        throw std::runtime_error("No numeric feature columns found alongside target.");
    }

    // Parse rows, skipping ERDDAP units rows or rows containing NaN in numeric columns
    for (const auto& row : raw_rows) {
        double target_val = 0.0;
        if (!try_parse_double(row[target_col_index], target_val)) continue;

        std::vector<double> features;
        bool valid_row = true;
        for (size_t c = 0; c < headers.size(); ++c) {
            if (c != target_col_index && is_numeric_col[c]) {
                double val = 0.0;
                if (!try_parse_double(row[c], val)) {
                    valid_row = false;
                    break;
                }
                features.push_back(val);
            }
        }

        if (valid_row) {
            data.X.push_back(std::move(features));
            data.Y.push_back({ target_val });
        }
    }

    if (data.X.empty()) {
        throw std::runtime_error("No complete numeric rows remained after filtering NaNs.");
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

    for (auto& row : data.X) {
        for (size_t c = 0; c < cols; ++c) {
            double range = data.feature_max[c] - data.feature_min[c];
            row[c] = (range > 1e-12) ? (row[c] - data.feature_min[c]) / range : 0.0;
        }
    }
}