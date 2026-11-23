#pragma once

#include "memory_store.hpp"
#include "tool.hpp"

#include <memory>

namespace oopagent {

class MemorySaveTool final : public Tool {
public:
    explicit MemorySaveTool(std::shared_ptr<SQLiteMemoryStore> store);
    std::string name() const override;
    std::string description() const override;
    nlohmann::json parameters() const override;
    ToolResult execute(const nlohmann::json& arguments) override;

private:
    std::shared_ptr<SQLiteMemoryStore> store_;
};

class MemorySearchTool final : public Tool {
public:
    explicit MemorySearchTool(std::shared_ptr<SQLiteMemoryStore> store);
    std::string name() const override;
    std::string description() const override;
    nlohmann::json parameters() const override;
    ToolResult execute(const nlohmann::json& arguments) override;

private:
    std::shared_ptr<SQLiteMemoryStore> store_;
};

} // namespace oopagent
