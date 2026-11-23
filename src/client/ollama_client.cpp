#include "ollama_client.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace oopagent {
namespace {

using Json = nlohmann::json;

size_t append_response(char* data, size_t size, size_t count, void* destination) {
    const size_t bytes = size * count;
    static_cast<std::string*>(destination)->append(data, bytes);
    return bytes;
}

void initialize_curl() {
    static const CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK) {
        throw std::runtime_error("Could not initialize libcurl");
    }
}

} // namespace

OllamaClient::OllamaClient(OllamaConfig config) : config_(std::move(config)) {
    if (config_.base_url.empty() || config_.model.empty()) {
        throw std::invalid_argument("Ollama base URL and model must not be empty");
    }
    if (config_.max_tokens <= 0 || config_.timeout_seconds <= 0) {
        throw std::invalid_argument("max_tokens and timeout_seconds must be positive");
    }
}

ChatResponse OllamaClient::chat(const std::vector<ChatMessage>& messages,
                                const nlohmann::json& tools) {
    initialize_curl();

    CURL* handle = curl_easy_init();
    if (handle == nullptr) {
        throw std::runtime_error("Could not create an HTTP request");
    }

    struct CurlCleanup {
        CURL* handle;
        ~CurlCleanup() { curl_easy_cleanup(handle); }
    } cleanup{handle};

    Json request;
    request["model"] = config_.model;
    request["stream"] = false;
    request["think"] = config_.think;
    request["messages"] = Json::array();
    request["options"] = {
        {"temperature", config_.temperature},
        {"num_predict", config_.max_tokens}
    };
    if (!tools.empty()) {
        request["tools"] = tools;
    }
    for (const auto& message : messages) {
        Json serialized_message = {{"role", message.role}, {"content", message.content}};
        if (!message.tool_name.empty()) {
            serialized_message["tool_name"] = message.tool_name;
        }
        if (!message.tool_calls.empty()) {
            serialized_message["tool_calls"] = Json::array();
            for (const auto& call : message.tool_calls) {
                serialized_message["tool_calls"].push_back({
                    {"type", "function"},
                    {"function", {{"name", call.name}, {"arguments", call.arguments}}}
                });
            }
        }
        request["messages"].push_back(std::move(serialized_message));
    }

    const std::string request_body = request.dump();
    std::string response_body;
    std::string url = config_.base_url;
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/api/chat";

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    struct HeaderCleanup {
        curl_slist* headers;
        ~HeaderCleanup() { curl_slist_free_all(headers); }
    } header_cleanup{headers};

    char error_buffer[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POST, 1L);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(request_body.size()));
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, append_response);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, config_.timeout_seconds);
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, error_buffer);

    const CURLcode result = curl_easy_perform(handle);
    if (result != CURLE_OK) {
        const std::string detail = error_buffer[0] != '\0' ? error_buffer : curl_easy_strerror(result);
        throw std::runtime_error("Ollama request failed: " + detail);
    }

    long status_code = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code < 200 || status_code >= 300) {
        throw std::runtime_error("Ollama returned HTTP " + std::to_string(status_code) + ": " + response_body);
    }

    try {
        const Json response = Json::parse(response_body);
        const Json& message = response.at("message");
        ChatResponse result_message;
        result_message.content = message.at("content").get<std::string>();
        result_message.prompt_tokens = response.value("prompt_eval_count", 0);
        result_message.completion_tokens = response.value("eval_count", 0);
        if (message.contains("tool_calls") && message.at("tool_calls").is_array()) {
            for (const auto& call : message.at("tool_calls")) {
                const Json& function = call.at("function");
                ToolCall tool_call;
                tool_call.name = function.at("name").get<std::string>();
                const Json& arguments = function.at("arguments");
                tool_call.arguments = arguments.is_string()
                    ? Json::parse(arguments.get<std::string>())
                    : arguments;
                result_message.tool_calls.push_back(std::move(tool_call));
            }
        }
        return result_message;
    } catch (const Json::exception& error) {
        throw std::runtime_error(std::string("Ollama returned malformed JSON: ") + error.what());
    }
}

} // namespace oopagent
