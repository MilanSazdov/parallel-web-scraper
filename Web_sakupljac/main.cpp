#include "web_scraper.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>

void output_results(std::ostream& os, int num_pages, double serial_time, double parallel_time, double time_per_book, double time_per_page, double avg_price, double avg_rating, const BookInfo& min_b, const BookInfo& max_b) {
    os << std::fixed << std::setprecision(5);
    os << "Parallel fetching results.\n";
    os << "Number of downloaded pages: " << num_pages << "\n";
    os << "Number of unique URLs: " << num_pages << "\n";
    os << "Calculations took " << parallel_time << " seconds.\n";
    os << "Time per book: " << time_per_book << " seconds.\n";
    os << "Time per page: " << time_per_page << " seconds.\n\n";
    os << "Star Rating Analytics:\n";
    os << "Number of books with 1 star(s): " << one_star.load() << "\n";
    os << "Number of books with 2 star(s): " << two_stars.load() << "\n";
    os << "Number of books with 3 star(s): " << three_stars.load() << "\n";
    os << "Number of books with 4 star(s): " << four_stars.load() << "\n";
    os << "Number of books with 5 star(s): " << five_stars.load() << "\n";
    os << "Average rating: " << avg_rating << " stars.\n\n";
    os << "Average price of book: " << avg_price << " GBP.\n\n";
    os << "The cheapest book:\n";
    os << "Title: " << min_b.title << "\n";
    os << "Price: " << std::fixed << std::setprecision(2) << min_b.price << "\n";
    os << "Stars: " << min_b.rating << "\n\n";
    os << "The most expensive book:\n";
    os << "Title: " << max_b.title << "\n";
    os << "Price: " << std::fixed << std::setprecision(2) << max_b.price << "\n";
    os << "Stars: " << max_b.rating << "\n\n";
    os << "Serial calculations took " << serial_time << " seconds.\n";
    os << "Parallel is " << (serial_time - parallel_time) << " seconds faster than serial.\n";
}

int main() {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::string start_url = "https://books.toscrape.com/index.html";

    // Run serial version
    auto serial_start = std::chrono::high_resolution_clock::now();
    serial_scrape(start_url);
    auto serial_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> serial_elapsed = serial_end - serial_start;
    double serial_time = serial_elapsed.count();

    // Reset globals for parallel run
    reset_globals();

    // Run parallel version
    auto parallel_start = std::chrono::high_resolution_clock::now();
    scrape(start_url);
    auto parallel_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> parallel_elapsed = parallel_end - parallel_start;
    double parallel_time = parallel_elapsed.count();

    int num_pages = static_cast<int>(visited.size());
    int tot_books = total_books.load();
    double time_per_book = (tot_books > 0) ? (parallel_time / tot_books) : 0.0;
    double time_per_page = (num_pages > 0) ? (parallel_time / num_pages) : 0.0;
    double avg_price = (tot_books > 0) ? (total_price.load() / tot_books) : 0.0;
    double avg_rating = (tot_books > 0) ? (sum_ratings.load() / tot_books) : 0.0;

    // Lock to get consistent min/max book
    std::lock_guard<std::mutex> lock(stats_mutex);

    std::ofstream out("results.txt");
    if (out) {
        output_results(out, num_pages, serial_time, parallel_time, time_per_book, time_per_page, avg_price, avg_rating, min_book, max_book);
    }
    else {
        std::cerr << "Failed to open results.txt\n";
    }

    // Also output to standard output
    output_results(std::cout, num_pages, serial_time, parallel_time, time_per_book, time_per_page, avg_price, avg_rating, min_book, max_book);

    curl_global_cleanup();
    return 0;
}