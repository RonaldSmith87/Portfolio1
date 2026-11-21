# Portfolio1 — C++ Time-Series Neural Network & ERDDAP Client

A zero-dependency C++ console application for oceanographic and environmental time-series forecasting. Built from scratch in native C++ using Windows WinHTTP, it connects directly to **NOAA PMEL ERDDAP** (and any standard ERDDAP server) to search catalogs, inspect dataset schemas, engineer cyclical time and sliding-window lag features, train configurable Multi-Layer Perceptrons (MLPs), and persist self-contained `.nn` models.

---

## Key Features

* **Interactive ERDDAP REST API Browser (`tabledap` & `griddap`):**
  * Search ERDDAP catalogs by keyword (defaults to `https://data.pmel.noaa.gov/pmel/erddap`).
  * Inspect variable metadata, data types, long names, and physical units via `/info/{datasetID}/index.csv`.
  * Dynamically construct time-bounded `tabledap` and `griddap` queries with automatic HTTP redirect handling.
  * Direct `.csv` URL and local CSV file ingestion also supported.
* **Time-Series Feature Engineering:**
  * **Cyclical ISO-8601 Encoding:** Automatically converts timestamp columns (`YYYY-MM-DDTHH:MM:SSZ`) into 4 continuous cyclical features (`_hour_sin`, `_hour_cos`, `_doy_sin`, `_doy_cos`).
  * **Sliding Lookback Windows:** Configurable lag window (`lookback_steps`) appends $t - k \dots t - 1$ historical features and target values to each training sample, automatically enforcing ascending chronological order.
* **Leakage-Free 80/20 Validation:**
  * Reserves the final 20% of chronological observations as a holdout validation split.
  * Computes min-max normalization bounds strictly on the 80% training split and reports both **Train Loss** and **Validation Loss / RMSE** in real physical units during training.
* **Self-Contained `.nn` Model Persistence:**
  * Saves network topology, activation functions, weights, biases, feature names, target name, and per-feature `min`/`max` bounds into human-readable `.nn` files inside `models/`.
  * Loaded models normalize raw user inputs automatically during inference without requiring the original CSV dataset.

---

## Project Structure

```text
Portfolio1/
├── dev/COS1/
│   ├── COS1.sln              # Visual Studio Solution
│   ├── Console.h / .cpp      # Interactive CLI menu, dataset & inference workflows
│   ├── DataLoader.h / .cpp   # WinHTTP ERDDAP client, RFC-4180 CSV parser, lag & cyclical encoding
│   ├── NeuralNetwork.h / .cpp# Custom MLP, forward/backward pass, RMSE evaluation, .nn serialization
│   ├── Layer.h / .cpp        # Dense layer implementation & activation functions (ReLU, Sigmoid, Tanh, Linear)
│   └── ModelManager.h / .cpp # Disk listing, loading, and deletion of saved .nn models
└── models/                   # Saved .nn model files (created at runtime)
Build Instructions (Windows / Visual Studio)PrerequisitesOS: Windows 10 or Windows 11 (uses native winhttp.lib)IDE / Compiler: Visual Studio 2022 with the Desktop development with C++ workload (C++17 or newer)BuildingClone the repository:Bashgit clone [https://github.com/RonaldSmith87/Portfolio1.git](https://github.com/RonaldSmith87/Portfolio1.git)
Open COS1.sln in Visual Studio.Set the solution configuration to Release and platform to x64.Build the solution via Build > Build Solution (Ctrl+Shift+B) and run (Ctrl+F5).Note for Windows 11 Users: If Windows Smart App Control blocks unsigned local builds, add a folder exclusion for your build output directory in Windows Security > Virus & threat protection > Exclusions.Quick-Start Walkthrough (PMEL ERDDAP)Launch COS1.exe and select Option 6 (Browse & Query ERDDAP API (PMEL)).Press Enter to accept the default server (https://data.pmel.noaa.gov/pmel/erddap), then enter a search term such as TAO or temperature.Select a dataset from the numbered list, inspect its variables/units, and enter the comma-separated variable indices you want to fetch (or all).Provide a bounded ISO-8601 date range (e.g., 2024-01-01T00:00:00Z to 2024-02-01T00:00:00Z) to keep the query fast, then select your target column index and lookback window (e.g., 3–5 steps).Select Option 1 (Learn) to configure and train your network:Recommended Regression Setup: Learning rate 0.01–0.05, 1–2 hidden layers with Tanh (3) activation, and Linear (4) activation on the output layer so predictions are not bounded to $[0, 1]$.Save the trained .nn model when prompted, then use Option 2 (Predict) or Option 3 (Load model) anytime for standalone inference.Roadmap[ ] Auto-compute lagged cyclical timestamps in Predict from a single target timestamp and sampling interval[ ] Optional learning-rate decay and mini-batch / epoch shuffling for non-sequential tabular datasets[ ] Export validation predictions vs. actuals to CSV for external plotting

```


# 🚀 Welcome to Project & Portfolio!

This repository will be used to keep track of research and development conducted in this class. Follow the instructions below to get started.
cos
### Getting Started

**1.** Review the [About](./docs/01_about/README.md) README and all of the links it provides.

**2.** Review the documentation and [Getting Started](./docs/02_getting_started/README.md) steps in this repository's docs folder.

**3.** Attend the first Live Lecture to see a live setup demo. If you are unable to attend, make sure to watch the archive.

### Next Steps...

After completing the above steps, feel free to begin updating this readme where indicated below. Remember to update this document each week to receive proper credit for the weekly Milestone assignment.

<br>

> ❗️ &nbsp; Now that you have read to this point, go ahead and delete this sentence and everything above it.

<br>

# Project & Portfolio 1

### Student First & Last Name

Hello my name is Ron Smith. I am a student from Washington State. The purpose of this repository is to practice development using version control. This work will help me begin to build a portfolio of skills and accomplishment that can be shared in the future.

<br>

## 📢 &nbsp; Weekly Stand Up

Each week I will summarize my milestone activity and progress by writing a stand-up. A stand-up is meant to be a succinct update on how things are going. Use these prompts as a guide on what to write about:

⚙️ Overview - What I worked on this past week
<br>
🌵 Challenges - What problems did I have & how I'm addressing them
<br>
🏆 Accomplishments - What is something I "leveled up" on this week
<br>
🔮 Next Steps - What I plan to prioritize and do next

<br>

### Week 1

Replace this paragraph with your stand up for this week. Use the prompts above to summarize your most recent milestone activity and work.

### Week 2

My next stand up will go here...

### Week 3

Stay tuned, this stand up is coming soon...

### Week 4

My final stand up...
