#include "web_search_tool.hpp"

#include <curl/curl.h>

#include <cctype>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace oopagent {
namespace {

size_t append_body(char* data, size_t size, size_t count, void* destination) {
    const size_t bytes = size * count;
    static_cast<std::string*>(destination)->append(data, bytes);
    return bytes;
}

std::string html_decode(std::string value) {
    const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&#x27;", "'"},
        {"&lt;", "<"}, {"&gt;", ">"}, {"&nbsp;", " "}
    };
    for (const auto& entity : entities) {
        std::size_t position = 0;
        while ((position = value.find(entity.first, position)) != std::string::npos) {
            value.replace(position, std::char_traits<char>::length(entity.first), entity.second);
            position += std::char_traits<char>::length(entity.second);
        }
    }
    return value;
}

std::string strip_tags(const std::string& html) {
    std::string text;
    bool inside_tag = false;
    for (char ch : html) {
        if (ch == '<') {
            inside_tag = true;
            continue;
        }
        if (ch == '>') {
            inside_tag = false;
            text.push_back(' ');
            continue;
        }
        if (!inside_tag) text.push_back(ch);
    }
    text = html_decode(std::move(text));
    std::string compact;
    bool previous_space = true;
    for (unsigned char ch : text) {
        if (std::isspace(ch)) {
            if (!previous_space) compact.push_back(' ');
            previous_space = true;
        } else {
            compact.push_back(static_cast<char>(ch));
            previous_space = false;
        }
    }
    if (!compact.empty() && compact.back() == ' ') compact.pop_back();
    return compact;
}

std::string attribute_value(const std::string& attributes, const std::string& name) {
    std::size_t position = attributes.find(name + "=");
    if (position == std::string::npos) return {};
    position += name.size() + 1;
    if (position >= attributes.size()) return {};
    const char quote = attributes[position];
    if (quote != '\'' && quote != '"') return {};
    const auto end = attributes.find(quote, position + 1);
    if (end == std::string::npos) return {};
    return attributes.substr(position + 1, end - position - 1);
}

std::string real_result_url(CURL* handle, std::string url) {
    const std::string parameter = "uddg=";
    const auto redirect = url.find(parameter);
    if (redirect != std::string::npos) {
        const auto start = redirect + parameter.size();
        const auto end = url.find('&', start);
        const std::string encoded = url.substr(start, end == std::string::npos ? end : end - start);
        int decoded_length = 0;
        char* decoded = curl_easy_unescape(handle, encoded.c_str(), static_cast<int>(encoded.size()), &decoded_length);
        if (decoded) {
            std::string result(decoded, static_cast<std::size_t>(decoded_length));
            curl_free(decoded);
            return html_decode(std::move(result));
        }
    }
    return html_decode(std::move(url));
}

struct SearchResult {
    std::string title;
    std::string url;
    std::string snippet;
};

std::vector<SearchResult> parse_results(CURL* handle, const std::string& html, int max_results) {
    std::vector<SearchResult> results;
    std::vector<std::string> snippets;
    std::size_t position = 0;
    while ((position = html.find("<a", position)) != std::string::npos) {
        const auto open_end = html.find('>', position + 2);
        if (open_end == std::string::npos) break;
        const auto close = html.find("</a>", open_end + 1);
        if (close == std::string::npos) break;

        const std::string attributes = html.substr(position + 2, open_end - position - 2);
        const std::string content = strip_tags(html.substr(open_end + 1, close - open_end - 1));
        if (attributes.find("result__a") != std::string::npos) {
            const std::string url = attribute_value(attributes, "href");
            if (!url.empty() && !content.empty()) {
                results.push_back({content, real_result_url(handle, url), {}});
            }
        } else if (attributes.find("result__snippet") != std::string::npos && !content.empty()) {
            snippets.push_back(content);
        }
        position = close + 4;
    }

    for (std::size_t i = 0; i < results.size() && i < snippets.size(); ++i) {
        results[i].snippet = snippets[i];
    }
    if (results.size() > static_cast<std::size_t>(max_results)) {
        results.resize(static_cast<std::size_t>(max_results));
    }
    return results;
}

std::string required_query(const nlohmann::json& arguments) {
    if (!arguments.is_object() || !arguments.contains("query") || !arguments["query"].is_string()) {
        throw std::invalid_argument("web_search requires a string argument named 'query'");
    }
    const auto query = arguments["query"].get<std::string>();
    if (query.empty()) throw std::invalid_argument("Search query must not be empty");
    return query;
}

} // namespace

std::string WebSearchTool::name() const { return "web_search"; }

std::string WebSearchTool::description() const {
    return "Search the web using DuckDuckGo and return result titles, URLs, and snippets.";
}

nlohmann::json WebSearchTool::parameters() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {{"type", "string"}, {"description", "The phrase or question to search for"}}},
            {"max_results", {{"type", "integer"}, {"description", "Maximum results to return (1-8)"}}}
        }},
        {"required", {"query"}},
        {"additionalProperties", false}
    };
}

ToolResult WebSearchTool::execute(const nlohmann::json& arguments) {
    static const CURLcode curl_init_result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curl_init_result != CURLE_OK) {
        throw std::runtime_error("Could not initialize libcurl for web search");
    }

    const std::string query = required_query(arguments);
    const int max_results = arguments.value("max_results", 5);
    if (max_results < 1 || max_results > 8) {
        throw std::invalid_argument("max_results must be between 1 and 8");
    }

    CURL* handle = curl_easy_init();
    if (!handle) throw std::runtime_error("Could not create a web search request");
    struct Cleanup {
        CURL* handle;
        char* escaped = nullptr;
        curl_slist* headers = nullptr;
        ~Cleanup() {
            if (escaped) curl_free(escaped);
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(handle);
        }
    } cleanup{handle};

    char* escaped = curl_easy_escape(handle, query.c_str(), static_cast<int>(query.size()));
    if (!escaped) throw std::runtime_error("Could not encode web search query");
    cleanup.escaped = escaped;
    cleanup.headers = curl_slist_append(cleanup.headers, "Accept: text/html,application/xhtml+xml");
    cleanup.headers = curl_slist_append(cleanup.headers, "Accept-Language: en-US,en;q=0.9");

    const std::string url = std::string("https://html.duckduckgo.com/html/?q=") + escaped;
    std::string response_body;
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, cleanup.headers);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, append_body);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) OopAgent/1.0");
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 25L);

    const CURLcode result = curl_easy_perform(handle);
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("DuckDuckGo request failed: ") + curl_easy_strerror(result));
    }
    long status_code = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code < 200 || status_code >= 300) {
        throw std::runtime_error("DuckDuckGo returned HTTP " + std::to_string(status_code));
    }

    const auto results = parse_results(handle, response_body, max_results);
    if (results.empty()) {
        if (response_body.find("anomaly-modal") != std::string::npos ||
            response_body.find("challenge") != std::string::npos) {
            return {false, "DuckDuckGo blocked this automated search request."};
        }
        return {true, "DuckDuckGo returned no web results for: " + query};
    }

    std::string output;
    for (std::size_t index = 0; index < results.size(); ++index) {
        output += std::to_string(index + 1) + ". " + results[index].title + "\n" +
                  results[index].url + "\n" + results[index].snippet + "\n";
    }
    return {true, output};
}

} // namespace oopagent
