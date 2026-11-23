#include "client/ollama_client.hpp"
#include "agent/agent_loop.hpp"
#include "tools/calculator_tool.hpp"
#include "tools/file_tools.hpp"
#include "tools/exec_tool.hpp"
#include "tools/memory_tools.hpp"
#include "tools/time_tool.hpp"
#include "tools/tool_registry.hpp"
#include "tools/web_search_tool.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

#ifdef _WIN32
std::string to_utf8(const wchar_t* text) {
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
                                             nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        throw std::runtime_error("Could not convert command-line text to UTF-8");
    }
    std::string result(static_cast<std::size_t>(required - 1), '\0');
    if (required > 1 && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
                                            result.data(), required, nullptr, nullptr) <= 0) {
        throw std::runtime_error("Could not convert command-line text to UTF-8");
    }
    return result;
}

std::vector<std::string> command_arguments() {
    int wide_argc = 0;
    wchar_t** wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (wide_argv == nullptr) {
        throw std::runtime_error("Could not read the Windows command line");
    }

    std::vector<std::string> arguments;
    try {
        for (int i = 1; i < wide_argc; ++i) {
            arguments.push_back(to_utf8(wide_argv[i]));
        }
    } catch (...) {
        LocalFree(wide_argv);
        throw;
    }
    LocalFree(wide_argv);
    return arguments;
}
#else
std::vector<std::string> command_arguments(int argc, char* argv[]) {
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }
    return arguments;
}
#endif

} // namespace

int main(int argc, char* argv[]) {
    std::string prompt;
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    const auto arguments = command_arguments();
#else
    const auto arguments = command_arguments(argc, argv);
#endif
    for (const auto& argument : arguments) {
        if (!prompt.empty()) {
            prompt += ' ';
        }
        prompt += argument;
    }
    if (prompt.empty()) {
        prompt = "Xin chào! Hãy giới thiệu ngắn gọn về bản thân bằng tiếng Việt.";
    }

    try {
        oopagent::OllamaClient client;
        oopagent::ToolRegistry tools;
        tools.register_tool(std::make_unique<oopagent::CalculatorTool>());
        tools.register_tool(std::make_unique<oopagent::ReadFileTool>());
        tools.register_tool(std::make_unique<oopagent::WriteFileTool>());
        tools.register_tool(std::make_unique<oopagent::TimeTool>());
        tools.register_tool(std::make_unique<oopagent::WebSearchTool>());
        tools.register_tool(std::make_unique<oopagent::ExecTool>());
        auto memory_store = std::make_shared<oopagent::SQLiteMemoryStore>();
        tools.register_tool(std::make_unique<oopagent::MemorySaveTool>(memory_store));
        tools.register_tool(std::make_unique<oopagent::MemorySearchTool>(memory_store));
        oopagent::AgentLoop agent(client, tools);
        const auto result = agent.run(prompt);
        std::cout << result.answer << "\n\n"
                  << "Agent steps: " << result.steps << "\n"
                  << "Tokens: prompt=" << result.prompt_tokens
                  << ", completion=" << result.completion_tokens << '\n';
        if (result.loop_detected || result.answer.find("max_steps=") != std::string::npos) {
            for (const auto& event : result.trace) {
                std::cout << "Trace: " << event << '\n';
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
