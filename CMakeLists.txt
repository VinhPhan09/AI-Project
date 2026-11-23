cmake_minimum_required(VERSION 3.20)

project(OopAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(oop_agent
    src/main.cpp
    src/client/ollama_client.cpp
    src/agent/agent_loop.cpp
    src/agent/loop_detector.cpp
    src/tools/tool_registry.cpp
    src/tools/calculator_tool.cpp
    src/tools/file_tools.cpp
    src/tools/time_tool.cpp
    src/tools/memory_store.cpp
    src/tools/memory_tools.cpp
    src/tools/web_search_tool.cpp
    src/tools/exec_tool.cpp
)

find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

target_include_directories(oop_agent PRIVATE src)
target_link_libraries(oop_agent PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    SQLite3::SQLite3
)

if(WIN32)
    target_link_libraries(oop_agent PRIVATE shell32)
endif()
