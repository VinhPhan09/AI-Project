#include "file_tools.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace oopagent {
namespace {

constexpr std::uintmax_t max_read_bytes = 1024 * 1024;

std::string required_string(const nlohmann::json& arguments, const char* key) {
    if (!arguments.is_object() || !arguments.contains(key) || !arguments.at(key).is_string()) {
        throw std::invalid_argument(std::string("A string argument named '") + key + "' is required");
    }
    return arguments.at(key).get<std::string>();
}

nlohmann::json path_parameters() {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Relative path inside agent_workspace"}}}
        }},
        {"required", {"path"}},
        {"additionalProperties", false}
    };
}

} // namespace

WorkspaceFileTool::WorkspaceFileTool(std::filesystem::path root) {
    std::error_code error;
    root_ = std::filesystem::absolute(std::move(root), error).lexically_normal();
    if (error) {
        throw std::runtime_error("Could not resolve agent workspace: " + error.message());
    }
    std::filesystem::create_directories(root_, error);
    if (error) {
        throw std::runtime_error("Could not create agent workspace: " + error.message());
    }
    root_ = std::filesystem::weakly_canonical(root_, error);
    if (error) {
        throw std::runtime_error("Could not canonicalize agent workspace: " + error.message());
    }
}

std::filesystem::path WorkspaceFileTool::resolve_path(const std::string& relative_path) const {
    const std::filesystem::path input = std::filesystem::u8path(relative_path);
    if (input.empty() || input.is_absolute() || input.has_root_name()) {
        throw std::invalid_argument("File tool path must be relative to agent_workspace");
    }

    const auto candidate = (root_ / input).lexically_normal();
    const auto canonical_candidate = std::filesystem::weakly_canonical(candidate);
    const auto relative = canonical_candidate.lexically_relative(root_);
    if (relative.empty() || *relative.begin() == "..") {
        throw std::invalid_argument("File tool path must stay inside agent_workspace");
    }
    return canonical_candidate;
}

std::string ReadFileTool::name() const { return "read_file"; }

std::string ReadFileTool::description() const {
    return "Read a UTF-8 text file inside the agent workspace (maximum 1 MiB).";
}

nlohmann::json ReadFileTool::parameters() const { return path_parameters(); }

ToolResult ReadFileTool::execute(const nlohmann::json& arguments) {
    const auto path = resolve_path(required_string(arguments, "path"));
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        throw std::invalid_argument("File does not exist or is not a regular file");
    }
    if (std::filesystem::file_size(path) > max_read_bytes) {
        throw std::invalid_argument("File exceeds the 1 MiB read limit");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open file for reading");
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    return {true, contents.str()};
}

std::string WriteFileTool::name() const { return "write_file"; }

std::string WriteFileTool::description() const {
    return "Write UTF-8 text to a file inside the agent workspace, creating parent directories if needed.";
}

nlohmann::json WriteFileTool::parameters() const {
    auto schema = path_parameters();
    schema["properties"]["content"] = {
        {"type", "string"}, {"description", "Text content to write to the file"}
    };
    schema["required"].push_back("content");
    return schema;
}

ToolResult WriteFileTool::execute(const nlohmann::json& arguments) {
    const auto path = resolve_path(required_string(arguments, "path"));
    const std::string content = required_string(arguments, "content");
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        throw std::runtime_error("Could not create parent directory: " + error.message());
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Could not open file for writing");
    }
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!output) {
        throw std::runtime_error("Failed while writing file");
    }
    return {true, "Wrote " + std::to_string(content.size()) + " bytes to " + path.filename().u8string()};
}

} // namespace oopagent
