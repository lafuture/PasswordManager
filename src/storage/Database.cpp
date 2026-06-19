#include "storage/Database.h"

#include "common/Exceptions.h"

Database::Database(const std::string& connStr) {
    try {
        conn_ = std::make_unique<pqxx::connection>(connStr);
    } catch (const std::exception& e) {
        throw DatabaseException("Не удалось подключиться к PostgreSQL: " +
                                std::string(e.what()));
    }
}

void Database::initialize() {
    try {
        pqxx::work tx(*conn_);
        tx.exec(R"(
            CREATE TABLE IF NOT EXISTS credentials (
                id BIGSERIAL PRIMARY KEY,
                chat_id BIGINT NOT NULL,
                resource TEXT NOT NULL,
                encrypted_password TEXT NOT NULL,
                UNIQUE(chat_id, resource)
            )
        )");
        tx.exec(
            "CREATE INDEX IF NOT EXISTS idx_chat_id ON credentials(chat_id)");
        tx.commit();
    } catch (const std::exception& e) {
        throw DatabaseException("Не удалось инициализировать базу: " +
                                std::string(e.what()));
    }
}

void Database::addResource(int64_t chatId, const std::string& resource,
                           const std::string& encryptedPassword) {
    try {
        pqxx::work tx(*conn_);
        tx.exec_params(
            "INSERT INTO credentials (chat_id, resource, encrypted_password) "
            "VALUES ($1, $2, $3)",
            chatId, resource, encryptedPassword);
        tx.commit();
    } catch (const pqxx::unique_violation&) {
        throw ResourceExistsException(resource);
    } catch (const std::exception& e) {
        throw DatabaseException("Не удалось сохранить ресурс: " +
                                std::string(e.what()));
    }
}

std::string Database::getPassword(int64_t chatId,
                                  const std::string& resource) {
    try {
        pqxx::work tx(*conn_);
        auto row = tx.exec_params1(
            "SELECT encrypted_password FROM credentials "
            "WHERE chat_id = $1 AND resource = $2",
            chatId, resource);
        tx.commit();
        return row[0].as<std::string>();
    } catch (const pqxx::unexpected_rows&) {
        throw ResourceNotFoundException(resource);
    } catch (const ResourceNotFoundException&) {
        throw;
    } catch (const std::exception& e) {
        throw DatabaseException("Не удалось получить пароль: " +
                                std::string(e.what()));
    }
}

void Database::updatePassword(int64_t chatId, const std::string& resource,
                              const std::string& encryptedPassword) {
    try {
        pqxx::work tx(*conn_);
        auto result = tx.exec_params(
            "UPDATE credentials SET encrypted_password = $1 "
            "WHERE chat_id = $2 AND resource = $3",
            encryptedPassword, chatId, resource);
        tx.commit();
        if (result.affected_rows() == 0) {
            throw ResourceNotFoundException(resource);
        }
    } catch (const ResourceNotFoundException&) {
        throw;
    } catch (const std::exception& e) {
        throw DatabaseException("Не удалось обновить пароль: " +
                                std::string(e.what()));
    }
}

void Database::deleteResource(int64_t chatId, const std::string& resource) {
    try {
        pqxx::work tx(*conn_);
        auto result = tx.exec_params(
            "DELETE FROM credentials WHERE chat_id = $1 AND resource = $2",
            chatId, resource);
        tx.commit();
        if (result.affected_rows() == 0) {
            throw ResourceNotFoundException(resource);
        }
    } catch (const ResourceNotFoundException&) {
        throw;
    } catch (const std::exception& e) {
        throw DatabaseException("Не удалось удалить ресурс: " +
                                std::string(e.what()));
    }
}

std::vector<std::pair<std::string, std::string>> Database::listResources(
    int64_t chatId) {
    try {
        pqxx::work tx(*conn_);
        auto rows = tx.exec_params(
            "SELECT resource, encrypted_password FROM credentials "
            "WHERE chat_id = $1 ORDER BY resource",
            chatId);
        tx.commit();

        std::vector<std::pair<std::string, std::string>> result;
        result.reserve(rows.size());
        for (const auto& row : rows) {
            result.emplace_back(row[0].as<std::string>(),
                                row[1].as<std::string>());
        }
        return result;
    } catch (const std::exception& e) {
        throw DatabaseException("Не удалось получить список ресурсов: " +
                                std::string(e.what()));
    }
}
