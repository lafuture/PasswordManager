#include "storage/Database.h"

#include "common/Exceptions.h"

Database::Database(const std::string& dbPath) {
    int rc = sqlite3_open_v2(
        dbPath.c_str(), &db_,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "неизвестная ошибка";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseException("Не удалось открыть базу данных: " + err);
    }
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void Database::initialize() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS credentials (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            chat_id INTEGER NOT NULL,
            resource TEXT NOT NULL,
            encrypted_password TEXT NOT NULL,
            UNIQUE(chat_id, resource)
        );
        CREATE INDEX IF NOT EXISTS idx_chat_id ON credentials(chat_id);
    )";
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "неизвестная ошибка";
        sqlite3_free(errMsg);
        throw DatabaseException("Не удалось инициализировать базу: " + err);
    }
}

void Database::addResource(int64_t chatId, const std::string& resource,
                           const std::string& encryptedPassword) {
    const char* sql =
        "INSERT INTO credentials (chat_id, resource, encrypted_password) "
        "VALUES (?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Ошибка подготовки запроса addResource");
    }

    sqlite3_bind_int64(stmt, 1, chatId);
    sqlite3_bind_text(stmt, 2, resource.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, encryptedPassword.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_CONSTRAINT) {
        throw ResourceExistsException(resource);
    }
    if (rc != SQLITE_DONE) {
        throw DatabaseException("Не удалось сохранить ресурс: " +
                                std::string(sqlite3_errmsg(db_)));
    }
}

std::string Database::getPassword(int64_t chatId, const std::string& resource) {
    const char* sql =
        "SELECT encrypted_password FROM credentials "
        "WHERE chat_id = ? AND resource = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Ошибка подготовки запроса getPassword");
    }

    sqlite3_bind_int64(stmt, 1, chatId);
    sqlite3_bind_text(stmt, 2, resource.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw ResourceNotFoundException(resource);
    }
    std::string result =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return result;
}

void Database::updatePassword(int64_t chatId, const std::string& resource,
                              const std::string& encryptedPassword) {
    const char* sql =
        "UPDATE credentials SET encrypted_password = ? "
        "WHERE chat_id = ? AND resource = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Ошибка подготовки запроса updatePassword");
    }

    sqlite3_bind_text(stmt, 1, encryptedPassword.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, chatId);
    sqlite3_bind_text(stmt, 3, resource.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw DatabaseException("Не удалось обновить пароль: " +
                                std::string(sqlite3_errmsg(db_)));
    }
    if (sqlite3_changes(db_) == 0) {
        throw ResourceNotFoundException(resource);
    }
}

void Database::deleteResource(int64_t chatId, const std::string& resource) {
    const char* sql =
        "DELETE FROM credentials WHERE chat_id = ? AND resource = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Ошибка подготовки запроса deleteResource");
    }

    sqlite3_bind_int64(stmt, 1, chatId);
    sqlite3_bind_text(stmt, 2, resource.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw DatabaseException("Не удалось удалить ресурс: " +
                                std::string(sqlite3_errmsg(db_)));
    }
    if (sqlite3_changes(db_) == 0) {
        throw ResourceNotFoundException(resource);
    }
}

std::vector<std::pair<std::string, std::string>> Database::listResources(
    int64_t chatId) {
    const char* sql =
        "SELECT resource, encrypted_password FROM credentials "
        "WHERE chat_id = ? ORDER BY resource";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Ошибка подготовки запроса listResources");
    }

    sqlite3_bind_int64(stmt, 1, chatId);

    std::vector<std::pair<std::string, std::string>> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string resource =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string encPwd =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        results.emplace_back(std::move(resource), std::move(encPwd));
    }
    sqlite3_finalize(stmt);
    return results;
}
