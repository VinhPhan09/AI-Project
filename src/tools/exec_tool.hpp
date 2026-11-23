#pragma once

#include "tool.hpp"

namespace oopagent {

class ExecTool final : public Tool {
public:
    std::string name() const override;
    std::string description() const override;
    nlohmann::json parameters() const override;
    ToolResult execute(const nlohmann::json& arguments) override;
};

} // namespace oopagent
