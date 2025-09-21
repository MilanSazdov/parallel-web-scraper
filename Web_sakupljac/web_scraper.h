#ifndef WEB_SCRAPER_H
#define WEB_SCRAPER_H

#include <string>
#include <vector>
#include <atomic>
#include <tbb/tbb.h>
#include <tbb/concurrent_unordered_set.h>
#include <tbb/task_group.h>
#include <curl/curl.h>
#include <mutex>
#include <queue>

// Struct for book data (used temporarily in parsing)
struct Book {
    std::string title;
    double price;
    int rating;
    std::string category;
};

// Struct for min/max book info
struct BookInfo {
    std::string title;
    int rating;
    double price;
};

// Function prototypes
std::string download(const std::string& url);
void parse(const std::string& html, const std::string& url, std::string& category, std::vector<Book>& books, std::vector<std::string>& new_urls);
std::string make_absolute(const std::string& relative_url, const std::string& base_url);
void scrape(const std::string& url); // Parallel version
void serial_scrape(const std::string& start_url); // Serial version
void reset_globals();

// Global concurrent structures and atomics
extern tbb::concurrent_unordered_set<std::string> visited;
extern tbb::concurrent_unordered_set<std::string> categories;
extern std::atomic<int> total_books;
extern std::atomic<int> one_star;
extern std::atomic<int> two_stars;
extern std::atomic<int> three_stars;
extern std::atomic<int> four_stars;
extern std::atomic<int> five_stars;
extern std::atomic<double> sum_ratings;
extern std::atomic<double> total_price;
extern std::atomic<double> max_price;
extern std::atomic<double> min_price;
extern BookInfo min_book;
extern BookInfo max_book;
extern std::mutex stats_mutex;

#endif // WEB_SCRAPER_H