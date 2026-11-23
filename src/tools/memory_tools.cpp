#include "memory_tools.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace oopagent {
namespace {

std::string required_text(const nlohmann::json& arguments, const char* field) {
    if (!arguments.is_object() || !arguments.contains(field) || !arguments.at(field).is_string()) {
        throw std::invalid_argument(std::string("A string argument named '") + field + "' is required");
    }
    return arguments.at(field).get<std::string>();
}

nlohmann::json search_schema() {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {{"type", "string"}, {"description", "Text to find in saved memories"}}},
            {"limit", {{"type", "integer"}, {"description", "Maximum results (1-20)"}}}
        }},
        {"required", {"query"}},
        {"additionalProperties", false}
    };
}

} // namespace

MemorySaveTool::MemorySaveTool(std::shared_ptr<SQLiteMemoryStore> store) : store_(std::move(store)) {
    if (!store_) throw std::invalid_argument("MemorySaveTool requires a memory store");
}
std::string MemorySaveTool::name() const { return "memory_save"; }
std::string MemorySaveTool::description() const { return "Save a text fact to persistent SQLite memory."; }
nlohmann::json MemorySaveTool::parameters() const {
    return {
        {"type", "object"},
        {"properties", {{"content", {{"type", "string"}, {"description", "Fact or note to remember"}}}}},
        {"required", {"content"}},
        {"additionalProperties", false}
    };
}
ToolResult MemorySaveTool::execute(const nlohmann::json& arguments) {
    const auto id = store_->save(required_text(arguments, "content"));
    return {true, "Saved memory with id " + std::to_string(id)};
}

MemorySearchTool::MemorySearchTool(std::shared_ptr<SQLiteMemoryStore> store) : store_(std::move(store)) {
    if (!store_) throw std::invalid_argument("MemorySearchTool requires a memory store");
}
std::string MemorySearchTool::name() const { return "memory_search"; }
std::string MemorySearchTool::description() const { return "Search persistent SQLite memories by a text phrase."; }
nlohmann::json MemorySearchTool::parameters() const { return search_schema(); }
ToolResult MemorySearchTool::execute(const nlohmann::json& arguments) {
    const auto query = required_text(arguments, "query");
    const int limit = arguments.value("limit", 5);
    const auto results = store_->search(query, limit);
    if (results.empty()) {
        return {true, "No matching memories found."};
    }

    std::string output;
    for (const auto& entry : results) {
        if (!output.empty()) output += '\n';
        output += "#" + std::to_string(entry.id) + " [" + entry.created_at + "] " + entry.content;
    }
    return {true, output};
}

} // namespace oopagent
