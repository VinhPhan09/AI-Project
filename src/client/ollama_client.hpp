#pragma once

#include "llm_client.hpp"

#include <string>

namespace oopagent {

struct OllamaConfig {
    std::string base_url = "http://localhost:11434";
    std::string model = "qwen3-vl:4b-instruct";
    double temperature = 0.2;
    int max_tokens = 1024;
    long timeout_seconds = 120;
    bool think = false;
};

class OllamaClient final : public LLMClient {
public:
    explicit OllamaClient(OllamaConfig config = {});
    ChatResponse chat(const std::vector<ChatMessage>& messages,
                      const nlohmann::json& tools = nlohmann::json::array()) override;

private:
    OllamaConfig config_;
};

} // namespace oopagent
