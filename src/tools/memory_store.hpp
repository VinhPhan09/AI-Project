#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;

namespace oopagent {

struct MemoryEntry {
    std::int64_t id = 0;
    std::string content;
    std::string created_at;
};

class SQLiteMemoryStore {
public:
    explicit SQLiteMemoryStore(std::filesystem::path database_path = "agent_workspace/memory.db");
    ~SQLiteMemoryStore();

    SQLiteMemoryStore(const SQLiteMemoryStore&) = delete;
    SQLiteMemoryStore& operator=(const SQLiteMemoryStore&) = delete;

    std::int64_t save(const std::string& content);
    std::vector<MemoryEntry> search(const std::string& query, int limit = 5) const;

private:
    sqlite3* database_ = nullptr;
};

} // namespace oopagent
