#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <regex>
#include <cctype>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <gumbo.h>
#include <iomanip>
// ============================================================================
// UTILITY CLASSES AND FUNCTIONS
// ============================================================================

class Logger {
public:
    enum Level { INFO, WARNING, ERROR, SUCCESS };
    
    static void log(Level level, const std::string& message) {
        std::string prefix;
        std::string color;
        
        switch (level) {
            case INFO:    prefix = "[INFO]"; color = "\033[36m"; break;  // Cyan
            case WARNING: prefix = "[WARN]"; color = "\033[33m"; break;  // Yellow
            case ERROR:   prefix = "[ERROR]"; color = "\033[31m"; break; // Red
            case SUCCESS: prefix = "[SUCCESS]"; color = "\033[32m"; break; // Green
        }
        
        std::cout << color << prefix << "\033[0m " << message << std::endl;
    }
    
    static void info(const std::string& msg) { log(INFO, msg); }
    static void warning(const std::string& msg) { log(WARNING, msg); }
    static void error(const std::string& msg) { log(ERROR, msg); }
    static void success(const std::string& msg) { log(SUCCESS, msg); }
};

class ProgressBar {
private:
    size_t total_;
    size_t current_;
    size_t bar_width_;
    std::string description_;
    
public:
    ProgressBar(size_t total, const std::string& description = "Progress", size_t bar_width = 50)
        : total_(total), current_(0), bar_width_(bar_width), description_(description) {}
    
    void update(size_t current) {
        current_ = current;
        display();
    }
    
    void increment() {
        update(current_ + 1);
    }
    
private:
    void display() {
        float progress = static_cast<float>(current_) / total_;
        size_t filled = static_cast<size_t>(progress * bar_width_);
        
        std::cout << "\r\033[36m" << description_ << "\033[0m [";
        for (size_t i = 0; i < bar_width_; ++i) {
            if (i < filled) {
                std::cout << "\033[32m█\033[0m";  // Green filled
            } else {
                std::cout << "\033[90m░\033[0m";  // Gray empty
            }
        }
        std::cout << "] " << current_ << "/" << total_ 
                  << " (" << static_cast<int>(progress * 100) << "%)";
        std::cout.flush();
        
        if (current_ == total_) {
            std::cout << std::endl;
        }
    }
};

// ============================================================================
// HTTP CLIENT
// ============================================================================

struct HTTPResponse {
    std::string data;
    long status_code = 0;
    bool success = false;
    std::string error_message;
};

class HTTPClient {
private:
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, HTTPResponse* response) {
        size_t totalSize = size * nmemb;
        response->data.append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }
    
public:
    static HTTPResponse get(const std::string& url, int timeout = 30) {
        HTTPResponse response;
        CURL* curl = curl_easy_init();
        
        if (!curl) {
            response.error_message = "Failed to initialize CURL";
            return response;
        }
        
        // Configure CURL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "linkex/2.0 (Advanced Web Scraper)");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        
        // Perform request
        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
        
        if (res == CURLE_OK && response.status_code == 200) {
            response.success = true;
        } else {
            response.error_message = curl_easy_strerror(res);
            if (response.status_code != 200) {
                response.error_message += " (HTTP " + std::to_string(response.status_code) + ")";
            }
        }
        
        curl_easy_cleanup(curl);
        return response;
    }
};

// ============================================================================
// HTML PARSER
// ============================================================================

class HTMLElement {
private:
    GumboNode* node_;
    
public:
    HTMLElement(GumboNode* node) : node_(node) {}
    
    std::string getAttribute(const std::string& name) const {
        if (node_->type != GUMBO_NODE_ELEMENT) return "";
        
        GumboAttribute* attr = gumbo_get_attribute(&node_->v.element.attributes, name.c_str());
        return attr ? std::string(attr->value) : "";
    }
    
    std::string getText() const {
        return getTextContent(node_);
    }
    
private:
    std::string getTextContent(GumboNode* node) const {
        if (node->type == GUMBO_NODE_TEXT) {
            return std::string(node->v.text.text);
        } else if (node->type == GUMBO_NODE_ELEMENT) {
            std::string text;
            GumboVector* children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i) {
                text += getTextContent(static_cast<GumboNode*>(children->data[i]));
            }
            return text;
        }
        return "";
    }
};

class HTMLParser {
private:
    std::unique_ptr<GumboOutput, void(*)(GumboOutput*)> output_;
    
public:
    HTMLParser(const std::string& html) 
        : output_(gumbo_parse(html.c_str()), 
                 [](GumboOutput* output) { gumbo_destroy_output(&kGumboDefaultOptions, output); }) {
        if (!output_) {
            throw std::runtime_error("Failed to parse HTML");
        }
    }
    
    std::vector<HTMLElement> select(const std::string& selector) {
        std::vector<GumboNode*> nodes;
        findElementsBySelector(output_->root, selector, nodes);
        
        std::vector<HTMLElement> elements;
        elements.reserve(nodes.size());
        for (auto* node : nodes) {
            elements.emplace_back(node);
        }
        return elements;
    }
    
private:
    void findElementsBySelector(GumboNode* node, const std::string& selector, std::vector<GumboNode*>& results) {
        if (node->type != GUMBO_NODE_ELEMENT) return;
        
        // Enhanced selector matching
        if (selector == "#chapters-list li > a" || selector == "#chapters-list a") {
            findChapterLinks(node, results);
        } else if (selector == "#manga-info-rightColumn h1" || 
                   selector == "#manga-info-rightColumn > div:nth-child(1) > h1") {
            findMangaTitle(node, results);
        }
        
        // Recursively search children
        if (node->type == GUMBO_NODE_ELEMENT) {
            GumboVector* children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i) {
                findElementsBySelector(static_cast<GumboNode*>(children->data[i]), selector, results);
            }
        }
    }
    
    void findChapterLinks(GumboNode* node, std::vector<GumboNode*>& results) {
        if (node->v.element.tag == GUMBO_TAG_DIV || node->v.element.tag == GUMBO_TAG_UL) {
            GumboAttribute* id_attr = gumbo_get_attribute(&node->v.element.attributes, "id");
            if (id_attr && std::string(id_attr->value) == "chapters-list") {
                collectAllLinks(node, results);
            }
        }
    }
    
    void findMangaTitle(GumboNode* node, std::vector<GumboNode*>& results) {
        if (node->v.element.tag == GUMBO_TAG_DIV) {
            GumboAttribute* id_attr = gumbo_get_attribute(&node->v.element.attributes, "id");
            if (id_attr && std::string(id_attr->value) == "manga-info-rightColumn") {
                collectAllH1(node, results);
            }
        }
    }
    
    void collectAllLinks(GumboNode* node, std::vector<GumboNode*>& results) {
        if (node->type == GUMBO_NODE_ELEMENT) {
            if (node->v.element.tag == GUMBO_TAG_A) {
                results.push_back(node);
            }
            
            GumboVector* children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i) {
                collectAllLinks(static_cast<GumboNode*>(children->data[i]), results);
            }
        }
    }
    
    void collectAllH1(GumboNode* node, std::vector<GumboNode*>& results) {
        if (node->type == GUMBO_NODE_ELEMENT) {
            if (node->v.element.tag == GUMBO_TAG_H1) {
                results.push_back(node);
                return; // Take first H1 found
            }
            
            GumboVector* children = &node->v.element.children;
            for (unsigned int i = 0; i < children->length; ++i) {
                collectAllH1(static_cast<GumboNode*>(children->data[i]), results);
                if (!results.empty()) return; // Stop after first H1
            }
        }
    }
};

// ============================================================================
// URL UTILITIES
// ============================================================================

class URLUtils {
public:
    static std::string join(const std::string& baseUrl, const std::string& relativeUrl) {
        if (relativeUrl.empty()) return baseUrl;
        
        // If relative URL is already absolute, return it
        if (relativeUrl.find("http://") == 0 || relativeUrl.find("https://") == 0) {
            return relativeUrl;
        }
        
        std::string result = baseUrl;
        
        // Remove trailing slash from base URL
        if (!result.empty() && result.back() == '/') {
            result.pop_back();
        }
        
        // Add leading slash to relative URL if needed
        std::string relative = relativeUrl;
        if (!relative.empty() && relative.front() != '/') {
            relative = "/" + relative;
        }
        
        return result + relative;
    }
    
    static bool isValid(const std::string& url) {
        return url.find("http://") == 0 || url.find("https://") == 0;
    }
};

// ============================================================================
// STRING UTILITIES
// ============================================================================

class StringUtils {
public:
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
    
    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    static std::string createSafeFilename(const std::string& title) {
        std::string filename = toLower(trim(title));
        
        // Replace spaces and special characters
        std::regex specialChars(R"([^\w\-_\.])", std::regex_constants::icase);
        filename = std::regex_replace(filename, specialChars, "_");
        
        // Replace multiple underscores with single
        std::regex multipleUnderscores("_{2,}");
        filename = std::regex_replace(filename, multipleUnderscores, "_");
        
        // Remove leading/trailing underscores
        if (!filename.empty() && filename.front() == '_') filename.erase(0, 1);
        if (!filename.empty() && filename.back() == '_') filename.pop_back();
        
        return filename.empty() ? "chapters" : filename;
    }
    
    static std::vector<std::string> naturalSortTokens(const std::string& str) {
        std::vector<std::string> tokens;
        std::regex tokenRegex(R"((\d+|\D+))");
        auto begin = std::sregex_iterator(str.begin(), str.end(), tokenRegex);
        auto end = std::sregex_iterator();
        
        for (auto i = begin; i != end; ++i) {
            tokens.push_back(i->str());
        }
        return tokens;
    }
    
    static bool naturalSort(const std::string& a, const std::string& b) {
        auto aTokens = naturalSortTokens(a);
        auto bTokens = naturalSortTokens(b);
        
        size_t minSize = std::min(aTokens.size(), bTokens.size());
        
        for (size_t i = 0; i < minSize; ++i) {
            const std::string& aToken = aTokens[i];
            const std::string& bToken = bTokens[i];
            
            bool aIsNumber = std::all_of(aToken.begin(), aToken.end(), ::isdigit);
            bool bIsNumber = std::all_of(bToken.begin(), bToken.end(), ::isdigit);
            
            if (aIsNumber && bIsNumber) {
                int aNum = std::stoi(aToken);
                int bNum = std::stoi(bToken);
                if (aNum != bNum) {
                    return aNum < bNum;
                }
            } else {
                std::string aLower = toLower(aToken);
                std::string bLower = toLower(bToken);
                if (aLower != bLower) {
                    return aLower < bLower;
                }
            }
        }
        
        return aTokens.size() < bTokens.size();
    }
};

// ============================================================================
// MAIN SCRAPER CLASS
// ============================================================================

class MangaScraper {
private:
    std::string baseUrl_;
    std::vector<std::string> chapterLinks_;
    std::string mangaTitle_;
    
public:
    explicit MangaScraper(const std::string& url) : baseUrl_(url) {
        if (!URLUtils::isValid(url)) {
            throw std::invalid_argument("Invalid URL provided");
        }
    }
    
    bool scrape() {
        try {
            Logger::info("Starting scrape for: " + baseUrl_);
            
            // Fetch webpage
            Logger::info("Fetching webpage...");
            HTTPResponse response = HTTPClient::get(baseUrl_);
            
            if (!response.success) {
                Logger::error("Failed to fetch webpage: " + response.error_message);
                return false;
            }
            
            Logger::success("Webpage fetched successfully (" + std::to_string(response.data.length()) + " bytes)");
            
            // Parse HTML
            Logger::info("Parsing HTML content...");
            HTMLParser parser(response.data);
            
            // Extract chapter links
            Logger::info("Extracting chapter links...");
            auto chapterElements = parser.select("#chapters-list li > a");
            
            if (chapterElements.empty()) {
                // Try alternative selector
                chapterElements = parser.select("#chapters-list a");
            }
            
            if (chapterElements.empty()) {
                Logger::warning("No chapter links found with standard selectors");
                return false;
            }
            
            // Process chapter links with progress bar
            ProgressBar progress(chapterElements.size(), "Processing chapters");
            
            for (size_t i = 0; i < chapterElements.size(); ++i) {
                const auto& element = chapterElements[i];
                
                std::string href = element.getAttribute("href");
                if (href.empty()) {
                    href = element.getAttribute("src");
                }
                
                if (!href.empty()) {
                    std::string fullUrl = URLUtils::join("https://demonicscans.org", href);
                    chapterLinks_.push_back(fullUrl);
                }
                
                progress.update(i + 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay for visual effect
            }
            
            // Sort chapters naturally
            Logger::info("Sorting chapters naturally...");
            std::sort(chapterLinks_.begin(), chapterLinks_.end(), StringUtils::naturalSort);
            
            // Extract manga title
            Logger::info("Extracting manga title...");
            auto titleElements = parser.select("#manga-info-rightColumn h1");
            
            if (!titleElements.empty()) {
                mangaTitle_ = StringUtils::trim(titleElements[0].getText());
            }
            
            if (mangaTitle_.empty()) {
                Logger::warning("Could not extract manga title, using default");
                mangaTitle_ = "unknown_manga";
            } else {
                Logger::success("Found manga: " + mangaTitle_);
            }
            
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Scraping failed: " + std::string(e.what()));
            return false;
        }
    }
    
    bool saveToFile() {
        if (chapterLinks_.empty()) {
            Logger::error("No chapter links to save");
            return false;
        }
        
        std::string filename = StringUtils::createSafeFilename(mangaTitle_) + ".txt";
        
        Logger::info("Saving to file: " + filename);
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            Logger::error("Could not create file: " + filename);
            return false;
        }
        
        // Write header with metadata
        file << "# Manga Chapter Links\n";
        file << "# Title: " << mangaTitle_ << "\n";
        file << "# Source: " << baseUrl_ << "\n";
        file << "# Total Chapters: " << chapterLinks_.size() << "\n";
        file << "# Generated: " << getCurrentTimestamp() << "\n";
        file << "# ==========================================\n\n";
        
        // Write chapter links
        for (size_t i = 0; i < chapterLinks_.size(); ++i) {
            file << "# Chapter " << (i + 1) << "\n";
            file << chapterLinks_[i] << "\n\n";
        }
        
        file.close();
        
        Logger::success("Successfully saved " + std::to_string(chapterLinks_.size()) + 
                       " chapter links to " + filename);
        return true;
    }
    
    void printSummary() const {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "\033[1;36m SCRAPING SUMMARY \033[0m" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "\033[1mManga Title:\033[0m " << mangaTitle_ << std::endl;
        std::cout << "\033[1mSource URL:\033[0m " << baseUrl_ << std::endl;
        std::cout << "\033[1mChapters Found:\033[0m " << chapterLinks_.size() << std::endl;
        std::cout << "\033[1mFilename:\033[0m " << StringUtils::createSafeFilename(mangaTitle_) << ".txt" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
    }
    
private:
    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

// ============================================================================
// MAIN FUNCTION
// ============================================================================

void printBanner() {
    std::cout << "\033[1;36m" << std::endl;
    std::cout << "██╗     ██╗███╗   ██╗██╗  ██╗███████╗██╗  ██╗" << std::endl;
    std::cout << "██║     ██║████╗  ██║██║ ██╔╝██╔════╝╚██╗██╔╝" << std::endl;
    std::cout << "██║     ██║██╔██╗ ██║█████╔╝ █████╗   ╚███╔╝ " << std::endl;
    std::cout << "██║     ██║██║╚██╗██║██╔═██╗ ██╔══╝   ██╔██╗ " << std::endl;
    std::cout << "███████╗██║██║ ╚████║██║  ██╗███████╗██╔╝ ██╗" << std::endl;
    std::cout << "╚══════╝╚═╝╚═╝  ╚═══╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝" << std::endl;
    std::cout << "\033[0m" << std::endl;
    std::cout << "\033[1;33mAdvanced Manga Chapter Link Extractor v2.0\033[0m" << std::endl;
    std::cout << "\033[90mBuilt with C++17 | Powered by libcurl & gumbo-parser\033[0m" << std::endl;
    std::cout << std::string(60, '-') << std::endl << std::endl;
}

void printUsage() {
    std::cout << "\033[1mUsage:\033[0m linkex <URL>" << std::endl;
    std::cout << "\033[1mExample:\033[0m linkex https://demonicscans.org/manga/The-Beginning-After-the-End" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mFeatures:\033[0m" << std::endl;
    std::cout << "  • Natural chapter sorting (1, 2, 10 instead of 1, 10, 2)" << std::endl;
    std::cout << "  • Intelligent URL handling (relative & absolute)" << std::endl;
    std::cout << "  • Comprehensive error handling & logging" << std::endl;
    std::cout << "  • Progress tracking with visual indicators" << std::endl;
    std::cout << "  • Safe filename generation" << std::endl;
    std::cout << "  • Metadata-rich output files" << std::endl;
}

int main(int argc, char* argv[]) {
    printBanner();
    
    if (argc != 2) {
        printUsage();
        return 1;
    }
    
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    try {
        MangaScraper scraper(argv[1]);
        
        if (scraper.scrape()) {
            scraper.printSummary();
            
            if (scraper.saveToFile()) {
                Logger::success("Operation completed successfully!");
            } else {
                Logger::error("Failed to save results to file");
                curl_global_cleanup();
                return 1;
            }
        } else {
            Logger::error("Scraping operation failed");
            curl_global_cleanup();
            return 1;
        }
        
    } catch (const std::exception& e) {
        Logger::error("Fatal error: " + std::string(e.what()));
        curl_global_cleanup();
        return 1;
    }
    
    // Cleanup curl
    curl_global_cleanup();
    
    std::cout << "\n\033[1;32mThank you for using linkex! 🚀\033[0m" << std::endl;
    return 0;
}