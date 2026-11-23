#pragma once

#include "tool.hpp"

#include <map>
#include <memory>
#include <string>

namespace oopagent {

class ToolRegistry {
public:
    void register_tool(std::unique_ptr<Tool> tool);
    bool contains(const std::string& name) const;
    nlohmann::json definitions() const;
    ToolResult execute(const std::string& name, const nlohmann::json& arguments) const;

private:
    std::map<std::string, std::unique_ptr<Tool>> tools_;
};

} // namespace oopagent
