#include "agent_loop.hpp"

#include <stdexcept>
#include <utility>

namespace oopagent {
namespace {

std::string abbreviated(std::string value, std::size_t max_length = 240) {
    if (value.size() > max_length) value = value.substr(0, max_length) + "...";
    return value;
}

} // namespace

AgentLoop::AgentLoop(LLMClient& client, ToolRegistry& tools, std::size_t max_steps)
    : client_(client), tools_(tools), max_steps_(max_steps) {
    if (max_steps_ == 0) {
        throw std::invalid_argument("Agent max_steps must be greater than zero");
    }
}

AgentRunResult AgentLoop::run(const std::string& task) {
    if (task.empty()) {
        throw std::invalid_argument("Agent task must not be empty");
    }

    std::vector<ChatMessage> history{
        {"system", "You are a helpful AI agent. Use calculator for arithmetic, read_file/write_file "
                   "for workspace files, memory_save/memory_search for persistent notes, and get_time "
                   "when asked for the current local date or time; use web_search when the task needs "
                   "up-to-date web information. Use exec only for safe shell commands in the allowed list. "
                   "After receiving tool results, give the user a concise answer."},
        {"user", task}
    };

    AgentRunResult result;
    const nlohmann::json tool_definitions = tools_.definitions();
    LoopDetector loop_detector;

    for (std::size_t step = 0; step < max_steps_; ++step) {
        ChatResponse response = client_.chat(history, tool_definitions);
        ++result.steps;
        result.prompt_tokens += response.prompt_tokens;
        result.completion_tokens += response.completion_tokens;

        if (response.tool_calls.empty()) {
            result.answer = std::move(response.content);
            if (result.answer.empty()) {
                result.answer = "Model returned an empty answer. Check the model response and token limit.";
            }
            return result;
        }

        history.push_back({"assistant", response.content, {}, response.tool_calls});
        for (const auto& call : response.tool_calls) {
            const std::string signature = call.name + ":" + call.arguments.dump();
            const LoopStatus loop = loop_detector.observe(signature);
            if (loop.warning) {
                const std::string note = std::string(loop_kind_name(loop.kind)) +
                    " loop detected (count=" + std::to_string(loop.repetitions) + ")";
                result.trace.push_back(note);
                if (loop.critical) {
                    result.loop_detected = true;
                    result.answer = "Agent stopped after detecting a critical " + note;
                    return result;
                }
            }

            const ToolResult tool_result = tools_.execute(call.name, call.arguments);
            result.trace.push_back("step=" + std::to_string(result.steps) + " tool=" + call.name +
                " args=" + abbreviated(call.arguments.dump()) + " success=" +
                (tool_result.success ? "true" : "false") + " result=" + abbreviated(tool_result.content));
            history.push_back({"tool", tool_result.content, call.name, {}});
        }
    }

    result.answer = "Agent stopped after reaching max_steps=" + std::to_string(max_steps_);
    return result;
}

} // namespace oopagent
