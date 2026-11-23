#pragma once

#include "tool.hpp"

#include <filesystem>

namespace oopagent {

class WorkspaceFileTool : public Tool {
public:
    explicit WorkspaceFileTool(std::filesystem::path root = "agent_workspace");

protected:
    std::filesystem::path resolve_path(const std::string& relative_path) const;
    std::filesystem::path root_;
};

class ReadFileTool final : public WorkspaceFileTool {
public:
    using WorkspaceFileTool::WorkspaceFileTool;
    std::string name() const override;
    std::string description() const override;
    nlohmann::json parameters() const override;
    ToolResult execute(const nlohmann::json& arguments) override;
};

class WriteFileTool final : public WorkspaceFileTool {
public:
    using WorkspaceFileTool::WorkspaceFileTool;
    std::string name() const override;
    std::string description() const override;
    nlohmann::json parameters() const override;
    ToolResult execute(const nlohmann::json& arguments) override;
};

} // namespace oopagent
