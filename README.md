# OOP AI Agent with Ollama

C++17 project for an object-oriented AI agent framework connected to Ollama.

## Current milestone

The project now has an Ollama chat client, runtime tool registry, calculator, workspace-scoped file read/write tools, a local time tool, DuckDuckGo web search, SQLite memory save/search, a restricted PowerShell exec tool, a bounded agent loop, and generic-repeat/ping-pong loop detection with failure traces.

File tools are restricted to the `agent_workspace/` directory under the current working directory.

## Build

```powershell
cmake -S . -B build
cmake --build build
```

## Ollama

The client defaults to `http://localhost:11434` and `qwen3-vl:4b-instruct`. Keep the Ollama app/service running, then launch the program. You can pass a prompt as command-line arguments:

```powershell
.\build\oop_agent.exe "Hãy tính 15 nhân 17"
```

## Dependencies

The current CMake target expects the MSYS2 UCRT64 packages for libcurl, nlohmann/json, and SQLite3.
