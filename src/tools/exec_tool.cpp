#include "exec_tool.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace oopagent {
namespace {

std::string required_command(const nlohmann::json& arguments) {
    if (!arguments.is_object() || !arguments.contains("command") || !arguments["command"].is_string()) {
        throw std::invalid_argument("exec requires a string argument named 'command'");
    }
    std::string command = arguments["command"].get<std::string>();
    const auto first = command.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) throw std::invalid_argument("Command must not be empty");
    const auto last = command.find_last_not_of(" \t\r\n");
    command = command.substr(first, last - first + 1);
    if (command.size() > 1000) throw std::invalid_argument("Command exceeds the 1000 character limit");
    return command;
}

#ifdef _WIN32

class Handle {
public:
    explicit Handle(HANDLE value = nullptr) : value_(value) {}
    ~Handle() { if (value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const { return value_; }
    HANDLE release() { HANDLE value = value_; value_ = nullptr; return value; }
    void reset() { if (value_ && value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); value_ = nullptr; }
private:
    HANDLE value_;
};

std::wstring utf8_to_wide(const std::string& text) {
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) throw std::invalid_argument("Command must be valid UTF-8");
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), wide.data(), size) <= 0) {
        throw std::runtime_error("Could not convert command to UTF-16");
    }
    return wide;
}

std::string base64_utf16le(const std::wstring& text) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    const std::size_t length = text.size() * sizeof(wchar_t);
    std::string output;
    output.reserve(((length + 2) / 3) * 4);
    for (std::size_t i = 0; i < length; i += 3) {
        const unsigned int a = bytes[i];
        const unsigned int b = i + 1 < length ? bytes[i + 1] : 0;
        const unsigned int c = i + 2 < length ? bytes[i + 2] : 0;
        const unsigned int value = (a << 16) | (b << 8) | c;
        output.push_back(alphabet[(value >> 18) & 0x3f]);
        output.push_back(alphabet[(value >> 12) & 0x3f]);
        output.push_back(i + 1 < length ? alphabet[(value >> 6) & 0x3f] : '=');
        output.push_back(i + 2 < length ? alphabet[value & 0x3f] : '=');
    }
    return output;
}

void read_pipe(HANDLE pipe, std::string& output) {
    char buffer[2048];
    DWORD bytes_read = 0;
    constexpr std::size_t max_output = 16 * 1024;
    while (ReadFile(pipe, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0) {
        if (output.size() < max_output) {
            const auto available = max_output - output.size();
            output.append(buffer, std::min<std::size_t>(bytes_read, available));
        }
    }
}

bool safe_command(const std::string& command) {
    for (unsigned char ch : command) {
        if (ch == ';' || ch == '|' || ch == '&' || ch == '<' || ch == '>' || ch == '$' ||
            ch == '`' || ch == '\'' || ch == '"' || ch == '\n' || ch == '\r' || ch == '(' || ch == ')') {
            return false;
        }
    }
    if (command.find("..") != std::string::npos || command.find(':') != std::string::npos) return false;

    const auto end = command.find_first_of(" \t");
    std::string verb = command.substr(0, end);
    std::transform(verb.begin(), verb.end(), verb.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    static const std::vector<std::string> allowed{
        "get-date", "get-location", "get-childitem", "write-output", "echo", "dir", "pwd"
    };
    return std::find(allowed.begin(), allowed.end(), verb) != allowed.end();
}

ToolResult run_powershell(const std::string& command) {
    if (!safe_command(command)) {
        throw std::invalid_argument(
            "Command is outside the safe allowlist. Allowed commands: Get-Date, Get-Location, "
            "Get-ChildItem, Write-Output, echo, dir, pwd. Shell operators and absolute paths are blocked.");
    }

    const auto working_directory = std::filesystem::absolute("agent_workspace").wstring();
    std::error_code error;
    std::filesystem::create_directories(working_directory, error);
    if (error) throw std::runtime_error("Could not prepare exec workspace: " + error.message());

    const std::wstring script = L"[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new(); " + utf8_to_wide(command);
    const std::string encoded = base64_utf16le(script);
    std::wstring command_line = L"powershell.exe -NoLogo -NoProfile -NonInteractive -EncodedCommand " +
                                std::wstring(encoded.begin(), encoded.end());

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE stdout_read_raw = nullptr, stdout_write_raw = nullptr;
    HANDLE stderr_read_raw = nullptr, stderr_write_raw = nullptr;
    if (!CreatePipe(&stdout_read_raw, &stdout_write_raw, &security, 0) ||
        !CreatePipe(&stderr_read_raw, &stderr_write_raw, &security, 0)) {
        if (stdout_read_raw) CloseHandle(stdout_read_raw);
        if (stdout_write_raw) CloseHandle(stdout_write_raw);
        if (stderr_read_raw) CloseHandle(stderr_read_raw);
        if (stderr_write_raw) CloseHandle(stderr_write_raw);
        throw std::runtime_error("Could not create output pipes for exec");
    }
    Handle stdout_read(stdout_read_raw), stdout_write(stdout_write_raw);
    Handle stderr_read(stderr_read_raw), stderr_write(stderr_write_raw);
    SetHandleInformation(stdout_read.get(), HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read.get(), HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = stdout_write.get();
    startup.hStdError = stderr_write.get();
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, working_directory.c_str(), &startup, &process)) {
        throw std::runtime_error("Could not start PowerShell for exec (Windows error " +
                                 std::to_string(GetLastError()) + ")");
    }
    Handle process_handle(process.hProcess), thread_handle(process.hThread);
    stdout_write.reset();
    stderr_write.reset();

    std::string stdout_text, stderr_text;
    std::thread stdout_reader(read_pipe, stdout_read.get(), std::ref(stdout_text));
    std::thread stderr_reader(read_pipe, stderr_read.get(), std::ref(stderr_text));
    const DWORD wait_result = WaitForSingleObject(process_handle.get(), 15000);
    const bool timed_out = wait_result == WAIT_TIMEOUT;
    if (timed_out) {
        TerminateProcess(process_handle.get(), 124);
        WaitForSingleObject(process_handle.get(), INFINITE);
    }
    DWORD exit_code = 1;
    GetExitCodeProcess(process_handle.get(), &exit_code);
    stdout_reader.join();
    stderr_reader.join();

    std::string output = "exit_code=" + std::to_string(timed_out ? 124 : exit_code) +
                         (timed_out ? " (timed out after 15 seconds)\n" : "\n") +
                         "stdout:\n" + (stdout_text.empty() ? "(empty)\n" : stdout_text) +
                         "\nstderr:\n" + (stderr_text.empty() ? "(empty)" : stderr_text);
    return {!timed_out && exit_code == 0, std::move(output)};
}

#endif

} // namespace

std::string ExecTool::name() const { return "exec"; }

std::string ExecTool::description() const {
    return "Run a safe, allowlisted PowerShell command in agent_workspace and return stdout and stderr.";
}

nlohmann::json ExecTool::parameters() const {
    return {
        {"type", "object"},
        {"properties", {{"command", {{"type", "string"}, {"description", "A single allowlisted command, without shell operators"}}}}},
        {"required", {"command"}},
        {"additionalProperties", false}
    };
}

ToolResult ExecTool::execute(const nlohmann::json& arguments) {
    const auto command = required_command(arguments);
#ifdef _WIN32
    return run_powershell(command);
#else
    (void)command;
    throw std::runtime_error("ExecTool currently supports Windows only");
#endif
}

} // namespace oopagent
