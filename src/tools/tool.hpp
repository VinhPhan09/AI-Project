#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace oopagent {

struct ToolResult {
    bool success = true;
    std::string content;
};

class Tool {
public:
    virtual ~Tool() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual nlohmann::json parameters() const = 0;
    virtual ToolResult execute(const nlohmann::json& arguments) = 0;
};

} // namespace oopagent
