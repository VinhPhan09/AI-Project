#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace oopagent {

struct ToolCall {
    std::string name;
    nlohmann::json arguments = nlohmann::json::object();
};

struct ChatMessage {
    std::string role;
    std::string content;
    std::string tool_name;
    std::vector<ToolCall> tool_calls;
};

struct ChatResponse {
    std::string content;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    std::vector<ToolCall> tool_calls;
};

class LLMClient {
public:
    virtual ~LLMClient() = default;
    virtual ChatResponse chat(const std::vector<ChatMessage>& messages,
                              const nlohmann::json& tools = nlohmann::json::array()) = 0;
};

} // namespace oopagent
