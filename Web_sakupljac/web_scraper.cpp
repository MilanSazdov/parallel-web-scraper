#include "web_scraper.h"
#include <iostream>
#include <algorithm>
#include <limits>
#include <sstream>

// Globals definitions
tbb::concurrent_unordered_set<std::string> visited;
tbb::concurrent_unordered_set<std::string> categories;
std::atomic<int> total_books(0);
std::atomic<int> one_star(0);
std::atomic<int> two_stars(0);
std::atomic<int> three_stars(0);
std::atomic<int> four_stars(0);
std::atomic<int> five_stars(0);
std::atomic<double> sum_ratings(0.0);
std::atomic<double> total_price(0.0);
std::atomic<double> max_price(0.0);
std::atomic<double> min_price(std::numeric_limits<double>::max());
BookInfo min_book = { "", 0, std::numeric_limits<double>::max() };
BookInfo max_book = { "", 0, 0.0 };
std::mutex stats_mutex;

// CURL write callback
static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    ((std::string*)userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string download(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string data;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10s timeout

    CURLcode res = curl_easy_perform(curl);
    int retries = 3;
    while (res != CURLE_OK && retries-- > 0) {
        data.clear();
        res = curl_easy_perform(curl);
    }
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";
    return data;
}

void parse(const std::string& html, const std::string& url_str, std::string& category, std::vector<Book>& books, std::vector<std::string>& new_urls) {
    category.clear();
    books.clear();
    new_urls.clear();

    // Extract category from <h1>
    size_t cat_start = html.find("<h1>");
    if (cat_start != std::string::npos) {
        cat_start += 4;
        size_t cat_end = html.find("</h1>", cat_start);
        if (cat_end != std::string::npos) {
            category = html.substr(cat_start, cat_end - cat_start);
        }
    }

    // Extract next page link (look for class="next"><a href="...)
    size_t next_start = html.find("class=\"next\">");
    if (next_start != std::string::npos) {
        next_start = html.find("href=\"", next_start);
        if (next_start != std::string::npos) {
            next_start += 6;
            size_t next_end = html.find("\"", next_start);
            if (next_end != std::string::npos) {
                new_urls.push_back(html.substr(next_start, next_end - next_start));
            }
        }
    }

    // If not a category page (e.g., home), extract category links
    if (url_str.find("/category/") == std::string::npos) {
        size_t pos = 0;
        while ((pos = html.find("href=\"catalogue/category/books/", pos)) != std::string::npos) {
            pos += 6; // Skip href="
            size_t end = html.find("\"", pos);
            if (end != std::string::npos) {
                std::string link = html.substr(pos, end - pos);
                if (link.find("/index.html") != std::string::npos) {
                    new_urls.push_back(link);
                }
                pos = end;
            }
        }
    }

    // Extract books only if in a category page
    if (url_str.find("/category/") != std::string::npos) {
        size_t pos = 0;
        while ((pos = html.find("<article class=\"product_pod\">", pos)) != std::string::npos) {
            Book b;

            // Rating: <p class="star-rating ...">
            size_t rating_start = html.find("<p class=\"star-rating ", pos);
            if (rating_start != std::string::npos) {
                rating_start = html.find(" ", rating_start + 20); // Skip to class value
                size_t rating_end = html.find("\"", rating_start);
                if (rating_end != std::string::npos) {
                    std::string rating_str = html.substr(rating_start + 1, rating_end - rating_start - 1);
                    if (rating_str == "One") b.rating = 1;
                    else if (rating_str == "Two") b.rating = 2;
                    else if (rating_str == "Three") b.rating = 3;
                    else if (rating_str == "Four") b.rating = 4;
                    else if (rating_str == "Five") b.rating = 5;
                    else b.rating = 0;
                }
            }

            // Title: <h3><a href="..." title="...">
            size_t title_start = html.find("<h3>", pos);
            if (title_start != std::string::npos) {
                title_start = html.find("title=\"", title_start);
                if (title_start != std::string::npos) {
                    title_start += 7;
                    size_t title_end = html.find("\"", title_start);
                    if (title_end != std::string::npos) {
                        b.title = html.substr(title_start, title_end - title_start);
                    }
                }
            }

            // Price: <p class="price_color">£xx.xx</p>
            size_t price_start = html.find("<p class=\"price_color\">", pos);
            if (price_start != std::string::npos) {
                price_start += 23;
                size_t price_end = html.find("</p>", price_start);
                if (price_end != std::string::npos) {
                    std::string price_str = html.substr(price_start, price_end - price_start);
                    size_t digit_start = price_str.find_first_of("0123456789");
                    if (digit_start != std::string::npos) {
                        b.price = std::stod(price_str.substr(digit_start));
                    }
                }
            }

            books.push_back(b);

            // Advance pos to after </article>
            size_t article_end = html.find("</article>", pos);
            if (article_end != std::string::npos) {
                pos = article_end + 10;
            }
            else {
                pos += 1; // Avoid infinite loop
            }
        }
    }
}

std::string make_absolute(const std::string& relative_url, const std::string& base_url) {
    if (relative_url.empty()) return "";
    if (relative_url.find("http") == 0) return relative_url;

    std::string base = base_url;
    if (relative_url[0] == '/') {
        // Absolute path, add domain
        size_t domain_end = base.find("/", 8); // After https://
        if (domain_end != std::string::npos) {
            base = base.substr(0, domain_end);
        }
        return base + relative_url;
    }
    else {
        // Relative, add directory
        size_t last_slash = base.rfind('/');
        if (last_slash != std::string::npos) {
            base = base.substr(0, last_slash + 1);
        }
        return base + relative_url;
    }
}

void update_stats(const std::vector<Book>& local_books, const std::string& cat) {
    for (const auto& b : local_books) {
        total_books.fetch_add(1);
        sum_ratings.fetch_add(static_cast<double>(b.rating));
        total_price.fetch_add(b.price);

        switch (b.rating) {
        case 1: one_star.fetch_add(1); break;
        case 2: two_stars.fetch_add(1); break;
        case 3: three_stars.fetch_add(1); break;
        case 4: four_stars.fetch_add(1); break;
        case 5: five_stars.fetch_add(1); break;
        }

        // Update min/max prices atomically
        double curr_min = min_price.load();
        while (b.price < curr_min && !min_price.compare_exchange_weak(curr_min, b.price)) {}

        double curr_max = max_price.load();
        while (b.price > curr_max && !max_price.compare_exchange_weak(curr_max, b.price)) {}

        // Update min/max book details under mutex
        std::lock_guard<std::mutex> lock(stats_mutex);
        if (b.price < min_book.price) {
            min_book = { b.title, b.rating, b.price };
        }
        if (b.price > max_book.price) {
            max_book = { b.title, b.rating, b.price };
        }
    }
}

void scrape(const std::string& url) {
    auto insert_res = visited.insert(url);
    if (!insert_res.second) return; // Already visited

    std::string html = download(url);
    if (html.empty()) return;

    std::string cat;
    std::vector<Book> local_books;
    std::vector<std::string> new_urls;
    parse(html, url, cat, local_books, new_urls);

    if (!cat.empty()) {
        categories.insert(cat);
    }

    update_stats(local_books, cat);

    tbb::task_group g;
    for (const auto& nu : new_urls) {
        std::string full_url = make_absolute(nu, url);
        g.run([full_url]() { scrape(full_url); });
    }
    g.wait();
}

void serial_scrape(const std::string& start_url) {
    std::queue<std::string> q;
    q.push(start_url);

    while (!q.empty()) {
        std::string url = q.front();
        q.pop();

        auto insert_res = visited.insert(url);
        if (!insert_res.second) continue; // Already visited

        std::string html = download(url);
        if (html.empty()) continue;

        std::string cat;
        std::vector<Book> local_books;
        std::vector<std::string> new_urls;
        parse(html, url, cat, local_books, new_urls);

        if (!cat.empty()) {
            categories.insert(cat);
        }

        update_stats(local_books, cat);

        for (const auto& nu : new_urls) {
            std::string full_url = make_absolute(nu, url);
            q.push(full_url);
        }
    }
}

void reset_globals() {
    visited.clear();
    categories.clear();
    total_books = 0;
    one_star = 0;
    two_stars = 0;
    three_stars = 0;
    four_stars = 0;
    five_stars = 0;
    sum_ratings = 0.0;
    total_price = 0.0;
    max_price = 0.0;
    min_price = std::numeric_limits<double>::max();
    min_book = { "", 0, std::numeric_limits<double>::max() };
    max_book = { "", 0, 0.0 };
}