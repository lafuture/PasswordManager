#pragma once

#include <stdexcept>
#include <string>

/**
 * @file Exceptions.h
 * @brief Иерархия пользовательских исключений приложения.
 *
 * Все ошибки между функциональными блоками программы (хранилище, шифрование,
 * сессии) и ошибки, предназначенные для показа пользователю, передаются через
 * этот механизм исключений и обрабатываются на границе обработчиков команд
 * бота.
 */

/**
 * @brief Базовый класс для всех исключений приложения.
 *
 * Наследуется от std::runtime_error, поэтому сообщение об ошибке доступно
 * через метод what().
 */
class AppException : public std::runtime_error {
   public:
    /**
     * @brief Создаёт исключение с текстовым описанием.
     * @param message Человекочитаемое сообщение об ошибке.
     */
    explicit AppException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Ошибка криптографической операции (шифрование, расшифровка, вывод
 * ключа).
 */
class CryptoException : public AppException {
   public:
    /**
     * @brief Создаёт исключение криптографического модуля.
     * @param message Описание криптографической ошибки.
     */
    explicit CryptoException(const std::string& message)
        : AppException(message) {}
};

/**
 * @brief Ошибка работы с базой данных (открытие, подготовка или выполнение
 * запроса).
 */
class DatabaseException : public AppException {
   public:
    /**
     * @brief Создаёт исключение модуля хранилища.
     * @param message Описание ошибки базы данных.
     */
    explicit DatabaseException(const std::string& message)
        : AppException(message) {}
};

/**
 * @brief Запрашиваемый ресурс не найден в хранилище.
 */
class ResourceNotFoundException : public AppException {
   public:
    /**
     * @brief Создаёт исключение «ресурс не найден».
     * @param resource Имя ненайденного ресурса.
     */
    explicit ResourceNotFoundException(const std::string& resource)
        : AppException("Ресурс не найден: " + resource) {}
};

/**
 * @brief Попытка создать ресурс, который уже существует.
 */
class ResourceExistsException : public AppException {
   public:
    /**
     * @brief Создаёт исключение «ресурс уже существует».
     * @param resource Имя дублирующегося ресурса.
     */
    explicit ResourceExistsException(const std::string& resource)
        : AppException("Ресурс уже существует: " + resource) {}
};

/**
 * @brief Сессия пользователя отсутствует или истекла (нет ключа шифрования).
 */
class SessionException : public AppException {
   public:
    /**
     * @brief Создаёт исключение модуля сессий.
     * @param message Описание ошибки сессии.
     */
    explicit SessionException(const std::string& message)
        : AppException(message) {}
};

/**
 * @brief Ошибка разбора аргументов командной строки или конфигурации запуска.
 */
class ConfigException : public AppException {
   public:
    /**
     * @brief Создаёт исключение конфигурации.
     * @param message Описание ошибки конфигурации.
     */
    explicit ConfigException(const std::string& message)
        : AppException(message) {}
};
