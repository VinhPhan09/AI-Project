#include "memory_store.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string>
#include <system_error>

namespace oopagent {
namespace {

class Statement {
public:
    Statement(sqlite3* database, const char* sql) : database_(database) {
        if (sqlite3_prepare_v2(database_, sql, -1, &statement_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("SQLite prepare failed: ") + sqlite3_errmsg(database_));
        }
    }
    ~Statement() { sqlite3_finalize(statement_); }
    sqlite3_stmt* get() const { return statement_; }

private:
    sqlite3* database_;
    sqlite3_stmt* statement_ = nullptr;
};

} // namespace

SQLiteMemoryStore::SQLiteMemoryStore(std::filesystem::path database_path) {
    std::error_code error;
    const auto absolute_path = std::filesystem::absolute(std::move(database_path), error);
    if (error) {
        throw std::runtime_error("Could not resolve memory database path: " + error.message());
    }
    std::filesystem::create_directories(absolute_path.parent_path(), error);
    if (error) {
        throw std::runtime_error("Could not create memory database directory: " + error.message());
    }

    if (sqlite3_open_v2(absolute_path.string().c_str(), &database_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        const std::string message = database_ ? sqlite3_errmsg(database_) : "unknown SQLite error";
        if (database_) {
            sqlite3_close(database_);
            database_ = nullptr;
        }
        throw std::runtime_error("Could not open memory database: " + message);
    }

    char* error_message = nullptr;
    const char* schema =
        "CREATE TABLE IF NOT EXISTS memories ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "content TEXT NOT NULL, "
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");";
    if (sqlite3_exec(database_, schema, nullptr, nullptr, &error_message) != SQLITE_OK) {
        const std::string message = error_message ? error_message : sqlite3_errmsg(database_);
        sqlite3_free(error_message);
        sqlite3_close(database_);
        database_ = nullptr;
        throw std::runtime_error("Could not initialize memory database: " + message);
    }
}

SQLiteMemoryStore::~SQLiteMemoryStore() {
    if (database_) {
        sqlite3_close(database_);
    }
}

std::int64_t SQLiteMemoryStore::save(const std::string& content) {
    if (content.empty()) {
        throw std::invalid_argument("Memory content must not be empty");
    }
    Statement statement(database_, "INSERT INTO memories(content) VALUES (?1)");
    if (sqlite3_bind_text(statement.get(), 1, content.c_str(), static_cast<int>(content.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
        throw std::runtime_error(std::string("SQLite bind failed: ") + sqlite3_errmsg(database_));
    }
    if (sqlite3_step(statement.get()) != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQLite insert failed: ") + sqlite3_errmsg(database_));
    }
    return sqlite3_last_insert_rowid(database_);
}

std::vector<MemoryEntry> SQLiteMemoryStore::search(const std::string& query, int limit) const {
    if (query.empty()) {
        throw std::invalid_argument("Memory search query must not be empty");
    }
    if (limit < 1 || limit > 20) {
        throw std::invalid_argument("Memory search limit must be between 1 and 20");
    }

    Statement statement(database_,
        "SELECT id, content, created_at FROM memories "
        "WHERE instr(lower(content), lower(?1)) > 0 "
        "ORDER BY id DESC LIMIT ?2");
    if (sqlite3_bind_text(statement.get(), 1, query.c_str(), static_cast<int>(query.size()), SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int(statement.get(), 2, limit) != SQLITE_OK) {
        throw std::runtime_error(std::string("SQLite bind failed: ") + sqlite3_errmsg(database_));
    }

    std::vector<MemoryEntry> results;
    int result = SQLITE_ROW;
    while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
        const auto* content = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1));
        const auto* created_at = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2));
        results.push_back({
            sqlite3_column_int64(statement.get(), 0),
            content ? content : "",
            created_at ? created_at : ""
        });
    }
    if (result != SQLITE_DONE) {
        throw std::runtime_error(std::string("SQLite search failed: ") + sqlite3_errmsg(database_));
    }
    return results;
}

} // namespace oopagent
