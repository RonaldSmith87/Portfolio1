# Milestone 4 Summary — Portfolio1 (`COS1`)

## Features Completed or Added

* **ERDDAP REST API Browser & Query Builder (`tabledap` & `griddap`):**
  * Added interactive dataset search (`DataLoader::search_erddap`) querying `/search/index.csv` with default targeting for NOAA PMEL ERDDAP (`https://data.pmel.noaa.gov/pmel/erddap`).
  * Added schema and variable inspection (`DataLoader::fetch_erddap_variables`) querying `/info/{datasetID}/index.csv` to display variable names, data types, long names, and physical units.
  * Added dynamic query URL construction (`DataLoader::build_erddap_query_url`) supporting URL-encoded ISO-8601 time constraints (`time>=` / `time<=`) for `tabledap` and dimension slicing (`[(last)]` / `[start:1:end]`) for `griddap`.
* **Time-Series Feature Engineering:**
  * **Cyclical ISO-8601 Encoding:** Implemented `DataLoader::try_parse_iso_datetime` to transform ISO-8601 timestamps into four continuous cyclical features (`_hour_sin`, `_hour_cos`, `_doy_sin`, `_doy_cos`).
  * **Sliding Lookback Windows:** Added configurable lag window generation (`lookback_steps`) in `DataLoader::parse_csv_string`, appending historical features and target values ($t - k \dots t - 1$) to each training vector.
* **Leakage-Free 80/20 Validation & RMSE Evaluation:**
  * Implemented a chronological 80/20 train/validation split (`X_test`, `Y_test`) in `DataLoader::parse_csv_string` so future observations are held out from training.
  * Added `NeuralNetwork::evaluate` to compute validation MSE loss and Root Mean Squared Error (RMSE) in the target's physical units at periodic training intervals and at the final epoch.
* **Self-Contained `.nn` Model Persistence:**
  * Extended `NeuralNetwork::save` and `NeuralNetwork::load` to serialize `target_name`, `feature_names`, `feature_min`, and `feature_max` alongside network topology, activation types, biases, and weights.
  * Added `NeuralNetwork::predict_raw` to automatically scale raw user inputs using saved `feature_min`/`feature_max` bounds during inference.

---

## Refactoring and Code Improvements

* **Unified WinHTTP Transport Layer:** Consolidated HTTP GET handling into a shared private helper (`DataLoader::fetch_http_text`) reused across catalog searches, variable metadata lookups, direct CSV URL downloads, and constructed ERDDAP queries.
* **Quote-Aware RFC-4180 CSV Parsing:** Upgraded `DataLoader::split_csv_line` to handle commas and escaped double quotes inside quoted strings so ERDDAP dataset titles and metadata attributes parse without column misalignment.
* **Chronological Ordering Enforcement:** Added timestamp direction detection in `DataLoader::parse_csv_string` to automatically reverse descending datasets (`newest -> oldest`) into ascending order (`oldest -> newest`) before constructing lag windows and 80/20 splits.
* **Leakage-Free Feature Scaling:** Refactored `DataLoader::normalize_features` to compute min-max scaling bounds strictly from the 80% training split and apply those exact bounds to the 20% validation split.
* **Release Readiness & Documentation:** Merged `dev` into `main`, updated `README.md` with build and PMEL ERDDAP quick-start instructions, and published the `v1.0.0` GitHub release.

---

## Bugs Resolved

* **HTTP `302` Redirect Failures on NOAA PMEL ERDDAP:** Configured `WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS` via `WinHttpSetOption` in `DataLoader::fetch_http_text` so WinHTTP automatically follows load-balancer redirects (`301`/`302`/`307`).
* **MSVC `C4996` (`sscanf`) & Static Analysis `C6387` Errors/Warnings:** Replaced unsafe `std::sscanf` calls in `DataLoader::try_parse_iso_datetime` with bounds-checked `std::stoi` substring parsing, and added explicit null-pointer guards around `URL_COMPONENTS` pointers (`lpszHostName`, `lpszUrlPath`, `lpszExtraInfo`).
* **Missing Standard Headers & Brace Mismatches:** Added missing `<iomanip>` (for `std::setw` in URL encoding) and `<array>` includes, and resolved a truncated brace block (`C1075`) in `Console::handle_predict`.
* **Windows Application Control Execution Block:** Resolved local debug/release launch blocking on newly compiled unsigned binaries in `D:\Fullsail\Portfolio1\dev\COS1\x64\`.

---

## Usability Improvements

* **Guided ERDDAP Catalog Workflow:** Replaced manual URL construction with a numbered menu flow (Option 6: `Browse & Query ERDDAP API (PMEL)`) featuring a default PMEL base URL shortcut, keyword search, numbered variable selection (or `all`), and optional ISO-8601 start/end date prompts.
* **ISO-8601 Timestamp Inference Input:** Updated `Console::handle_predict` to detect `_hour_sin` cyclical feature blocks and prompt for a single human-readable ISO-8601 timestamp (`YYYY-MM-DDTHH:MM:SSZ`) rather than requiring four raw sine/cosine floats.
* **Live Dataset & Split Telemetry in Main Menu:** Updated the console status header to display active training row count, validation row count, total engineered feature count, and target variable alongside the active model summary.
* **Standalone Inference Without Reloading Data:** Enabled loading any saved `.nn` model from disk and running predictions immediately without needing to re-fetch or reload the original CSV dataset.