# ⚡ Parallel Web Scraper – High-Performance C++ + Intel TBB

![Parallel Web Scraper Banner](./assets/parallel-web-scraper-banner.png)

<div align="center">

**Blazing-fast web scraper for structured websites, built with C++20, Intel TBB and libcurl.**  
Scrape, parse and analyse large sets of pages – in serial and in parallel – and measure real speedup on modern CPUs.

[![Language: C++20](https://img.shields.io/badge/Language-C++20-00599C.svg)]()
[![Parallelism: Intel TBB](https://img.shields.io/badge/Parallelism-Intel%20TBB-5936EE.svg)]()
[![HTTP: libcurl](https://img.shields.io/badge/HTTP-libcurl-0A7EB2.svg)]()
[![Status: Stable](https://img.shields.io/badge/Status-Stable-brightgreen.svg)]()

</div>

---

## 📖 Table of Contents

- [💥 The Problem](#-the-problem)
- [💡 The Solution](#-the-solution)
- [✨ Key Features](#-key-features)
- [🏗 Architecture & Tech Stack](#-architecture--tech-stack)
- [🔥 How It Works (The Flow)](#-how-it-works-the-flow)
- [📈 Performance](#-performance)
- [🛠 Getting Started](#-getting-started)
  - [1. Clone the Repository](#1-clone-the-repository)
  - [2. Build](#2-build)
  - [3. Run](#3-run)
- [📂 Project Structure](#-project-structure)
- [👥 Author](#-author)
- [📄 License](#-license)

---

## 💥 The Problem

Traditional web scrapers usually:

- Run **single-threaded**, leaving most CPU cores idle.
- Mix **network I/O**, **HTML parsing** and **data aggregation** in one big ball of mud.
- Make it hard to **measure** and **compare** performance between serial and parallel approaches.

If you want to **experiment with parallelism**, **benchmark speedups**, or **learn Intel TBB** on a real-world workload (HTTP calls + parsing + stats), you need a clean, focused playground.

---

## 💡 The Solution

**Parallel Web Scraper** is a C++20 project that:

- Scrapes a structured demo site (e.g. an online book catalogue).
- Implements both **serial** and **parallel** scraping pipelines.
- Uses **Intel Threading Building Blocks (TBB)** for task-based parallelism.
- Aggregates statistics in a **thread-safe** way.
- Outputs comparable metrics so you can see the **actual speedup**.

It’s designed as a **practical reference** for:

- Parallel programming with Intel TBB  
- Efficient use of libcurl in C++  
- Lock-aware, thread-safe statistics aggregation

---

## ✨ Key Features

- ⚙️ **Dual implementation** – serial scraper vs. parallel scraper
- 🧵 **Task-based parallelism** with Intel TBB (no manual thread management)
- 🌐 **HTTP layer** using libcurl with timeouts & retry logic
- 📊 **Rich statistics:**
  - Total pages visited & unique URLs
  - Total items/books scraped
  - Rating distribution (1★–5★)
  - Average rating & average price
  - Cheapest & most expensive item
- 🧷 **Thread-safe stats** using `std::atomic`, `std::mutex` and TBB concurrent containers
- 📝 **Human-readable report** printed to console and saved to `results.txt`
- 🧪 Perfect as a **benchmark / learning project** for parallel patterns

---

## 🏗 Architecture & Tech Stack

### Tech Stack

- **Language:** C++20  
- **Parallelism:** Intel Threading Building Blocks (TBB)  
- **Networking:** libcurl  
- **Standard Library:** `<atomic>`, `<mutex>`, `<queue>`, `<vector>`, `<string>`, `<chrono>`, `<fstream>`, `<iostream>`, etc.

### High-Level Components

- **`Book` / `BookInfo` (`Book.h`)**  
  Small structs representing a scraped item (title, price, rating, category) and helper info for min/max book.

- **`Stats` (`Stats.h`, `Stats.cpp`)**  
  Thread-safe statistics aggregator:
  - Uses `std::atomic` counters for counts, sums and price accumulators.
  - Uses a `std::mutex` to protect min/max book updates.
  - `update(const std::vector<Book>&)` merges a local batch into global stats.
  - `reset()` clears everything for a fresh run.

- **Downloader (`Downloader.h`, `Downloader.cpp`)**  
  Thin wrapper around **libcurl**:
  - Performs HTTP GET with a timeout.
  - Retries failed requests a few times before giving up.
  - Returns HTML as `std::string`.

- **Parser (`Parser.cpp` + header)**  
  Lightweight HTML parser tuned for the target site:
  - Extracts title, price, rating and category from each product block.
  - Finds links to the next page and other relevant URLs.
  - Normalizes relative URLs into absolute ones.

- **Scraper (`Scraper.cpp` + header)**  
  Orchestrates the whole process:
  - Maintains:
    - `tbb::concurrent_unordered_set<std::string> visited;`
    - `tbb::concurrent_unordered_set<std::string> categories;`
    - A shared `Stats` instance.
  - Provides both:
    - `serialScrape(startUrl)`
    - `parallelScrape(startUrl)`
  - Uses **task groups / tasks** in the parallel version to recursively spawn new work.

- **Entry Point (`main.cpp`)**  
  - Initializes libcurl.
  - Defines the starting URL (e.g. `https://books.toscrape.com/index.html`).
  - Runs **serial** scrape and measures execution time.
  - Resets shared state.
  - Runs **parallel** scrape and measures execution time again.
  - Computes averages and min/max data.
  - Writes a detailed report to:
    - `std::cout`
    - `results.txt`

---

## 🔥 How It Works (The Flow)

1. **Bootstrap** 🚀  
   `main.cpp` initializes libcurl and creates a `Scraper` instance with fresh `Stats` and empty visited sets.

2. **Serial Scrape** 🐢  
   - Starts from the root URL.
   - Uses a simple queue (BFS-style) to:
     - Download page → parse HTML → extract books & new URLs.
     - Update statistics.
     - Enqueue new URLs that haven’t been visited.

3. **Reset** 🔄  
   - Clears `visited` and other shared data.
   - Resets `Stats` to zero.

4. **Parallel Scrape** ⚡  
   - Starts from the same root URL.
   - Creates TBB tasks for discovered URLs:
     - Each task checks `visited` (concurrent set).
     - Downloads and parses the page.
     - Updates shared `Stats` using thread-safe APIs.
     - Spawns new tasks for newly discovered URLs.

5. **Reporting** 📊  
   - Once both runs finish, the program:
     - Prints serial vs. parallel stats.
     - Shows total time, time per page/book, and computed speedup.
     - Saves the same report to `results.txt` for later inspection.

---

## 📈 Performance

The project is designed to **showcase speedup** on multi-core CPUs:

- Parallel execution leverages **multiple cores** for downloading & parsing.
- Thread-safe statistics ensure **correctness** despite concurrency.
- Serial and parallel implementations scrape the **same dataset**, so metrics are directly comparable.

> Exact timings and speedup depend on your machine and network, but on a modern multi-core CPU you should clearly see the parallel version outperform the serial baseline.

---

## 🛠 Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/MilanSazdov/parallel-web-scraper.git
cd parallel-web-scraper/Web_sakupljac
```

### 2. Build

You have two options:

#### Option A – Visual Studio (Windows)

1. Open `Web_sakupljac/Web_sakupljac.sln` in Visual Studio.  
2. Select the desired configuration (e.g. `x64-Debug` or `x64-Release`).  
3. Press **Build → Build Solution** or `Ctrl+Shift+B`.  
4. Run the project with **F5**.

#### Option B – g++ / CLI build

If you want to build from the command line (Linux, WSL, MinGW, etc.):

Make sure you have:

- C++20 compatible compiler (`g++`, `clang++`, etc.)  
- Intel TBB installed  
- libcurl development libraries installed  

From inside `Web_sakupljac/`:

```bash
g++ -std=c++20 -O2 \
    main.cpp web_scraper.cpp \
    -ltbb -lcurl \
    -o web_scraper
```
> Adjust library flags (-ltbb, -lcurl) if your platform uses different names.

### 3. Run

From the `Web_sakupljac/` directory:

```bash
./web_scraper
# or on Windows
web_scraper.exe
```

The program will:
  1. Run the serial scraper.
  2. Run the parallel scraper.
  3. Print a side-by-side comparison.
  4. Generate results.txt with the same report.

---

## 📂 Project Structure

```text
.
├── Book.h               # Book & BookInfo structs
├── Downloader.h         # Downloader interface
├── Downloader.cpp       # libcurl implementation
├── Parser.cpp           # HTML parsing & URL discovery
├── Scraper.cpp          # Scraper class (serial + parallel)
├── Stats.h              # Thread-safe statistics (declaration)
├── Stats.cpp            # Thread-safe statistics (implementation)
├── main.cpp             # Entry point, timing & reporting

```

---

## 👥 Author

Project developed by:
* **Milan Sazdov** - [LinkedIn](https://www.linkedin.com/in/milansazdov) | [Email](mailto:milansazdov@gmail.com) 

---

## 📄 License

This project is licensed under the **MIT License**.

You are free to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the software, subject to the conditions stated in the license.

See the [LICENSE](./LICENSE) file for the full text.

