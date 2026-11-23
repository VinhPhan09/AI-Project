#pragma once

#include "../client/llm_client.hpp"
#include "../tools/tool_registry.hpp"
#include "loop_detector.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace oopagent {

struct AgentRunResult {
    std::string answer;
    std::size_t steps = 0;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    bool loop_detected = false;
    std::vector<std::string> trace;
};

class AgentLoop {
public:
    AgentLoop(LLMClient& client, ToolRegistry& tools, std::size_t max_steps = 8);
    AgentRunResult run(const std::string& task);

private:
    LLMClient& client_;
    ToolRegistry& tools_;
    std::size_t max_steps_;
};

} // namespace oopagent
