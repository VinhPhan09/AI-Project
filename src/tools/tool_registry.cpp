#include "tool_registry.hpp"

#include <stdexcept>
#include <utility>

namespace oopagent {

void ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        throw std::invalid_argument("Cannot register a null tool");
    }

    const std::string tool_name = tool->name();
    if (tool_name.empty()) {
        throw std::invalid_argument("A tool must have a name");
    }
    if (tools_.count(tool_name) != 0) {
        throw std::invalid_argument("A tool named '" + tool_name + "' is already registered");
    }

    tools_.emplace(tool_name, std::move(tool));
}

bool ToolRegistry::contains(const std::string& name) const {
    return tools_.count(name) != 0;
}

nlohmann::json ToolRegistry::definitions() const {
    auto result = nlohmann::json::array();
    for (const auto& entry : tools_) {
        const Tool& tool = *entry.second;
        result.push_back({
            {"type", "function"},
            {"function", {
                {"name", tool.name()},
                {"description", tool.description()},
                {"parameters", tool.parameters()}
            }}
        });
    }
    return result;
}

ToolResult ToolRegistry::execute(const std::string& name, const nlohmann::json& arguments) const {
    const auto found = tools_.find(name);
    if (found == tools_.end()) {
        return {false, "Tool not found: " + name};
    }

    try {
        return found->second->execute(arguments);
    } catch (const std::exception& error) {
        return {false, error.what()};
    }
}

} // namespace oopagent
