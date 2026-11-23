#include "time_tool.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace oopagent {

std::string TimeTool::name() const { return "get_time"; }

std::string TimeTool::description() const {
    return "Get the current local date and time from the computer running the agent.";
}

nlohmann::json TimeTool::parameters() const {
    return {{"type", "object"}, {"properties", nlohmann::json::object()}, {"additionalProperties", false}};
}

ToolResult TimeTool::execute(const nlohmann::json&) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t current_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &current_time);
#else
    localtime_r(&current_time, &local_time);
#endif
    std::ostringstream formatted;
    formatted << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return {true, formatted.str()};
}

} // namespace oopagent
