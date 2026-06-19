#pragma once

#include <cstdint>
#include <memory>
#include <pqxx/pqxx>
#include <string>
#include <utility>
#include <vector>

/**
 * @file Database.h
 * @brief Модуль хранилища зашифрованных паролей поверх PostgreSQL.
 */

/**
 * @class Database
 * @brief Обёртка над PostgreSQL (libpqxx) для хранения зашифрованных паролей.
 *
 * Каждая запись привязана к идентификатору чата Telegram (chat_id) и хранит
 * имя ресурса вместе с зашифрованным паролем. Для каждой пары
 * (chat_id, resource) гарантируется уникальность. Все запросы выполняются
 * через подготовленные выражения (prepared statements) для защиты от инъекций.
 * Ошибки уровня SQL и нарушения бизнес-правил передаются через исключения.
 */
class Database {
   public:
    /**
     * @brief Открывает соединение с базой данных PostgreSQL.
     * @param connStr Строка подключения в формате
     *                "postgresql://user:pass\@host:port/dbname" или
     *                "host=... dbname=... user=...".
     * @throws DatabaseException Если установить соединение не удалось.
     */
    explicit Database(
        const std::string& connStr = "postgresql://localhost/aaip");

    /**
     * @brief Закрывает соединение с базой данных.
     */
    ~Database() = default;

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /**
     * @brief Создаёт таблицу и индексы, если они ещё не существуют.
     * @throws DatabaseException При ошибке выполнения DDL-запроса.
     */
    void initialize();

    /**
     * @brief Добавляет новый ресурс с зашифрованным паролем.
     * @param chatId Идентификатор чата владельца.
     * @param resource Имя ресурса.
     * @param encryptedPassword Зашифрованный пароль (Base64).
     * @throws ResourceExistsException Если ресурс уже существует.
     * @throws DatabaseException При иной ошибке базы данных.
     */
    void addResource(int64_t chatId, const std::string& resource,
                     const std::string& encryptedPassword);

    /**
     * @brief Возвращает зашифрованный пароль для ресурса.
     * @param chatId Идентификатор чата владельца.
     * @param resource Имя ресурса.
     * @return Зашифрованный пароль (Base64).
     * @throws ResourceNotFoundException Если ресурс не найден.
     * @throws DatabaseException При ошибке базы данных.
     */
    std::string getPassword(int64_t chatId, const std::string& resource);

    /**
     * @brief Обновляет пароль существующего ресурса.
     * @param chatId Идентификатор чата владельца.
     * @param resource Имя ресурса.
     * @param encryptedPassword Новый зашифрованный пароль (Base64).
     * @throws ResourceNotFoundException Если ресурс не найден.
     * @throws DatabaseException При ошибке базы данных.
     */
    void updatePassword(int64_t chatId, const std::string& resource,
                        const std::string& encryptedPassword);

    /**
     * @brief Удаляет ресурс пользователя.
     * @param chatId Идентификатор чата владельца.
     * @param resource Имя ресурса.
     * @throws ResourceNotFoundException Если ресурс не найден.
     * @throws DatabaseException При ошибке базы данных.
     */
    void deleteResource(int64_t chatId, const std::string& resource);

    /**
     * @brief Возвращает все ресурсы пользователя с зашифрованными паролями.
     * @param chatId Идентификатор чата владельца.
     * @return Вектор пар (имя ресурса, зашифрованный пароль), отсортированный
     *         по имени ресурса. Пустой, если записей нет.
     * @throws DatabaseException При ошибке базы данных.
     */
    std::vector<std::pair<std::string, std::string>> listResources(
        int64_t chatId);

   private:
    /// Соединение с PostgreSQL (libpqxx). Хранится через указатель, чтобы
    /// избежать дефолтной конструкции pqxx::connection до инициализации строки.
    std::unique_ptr<pqxx::connection> conn_;
};
