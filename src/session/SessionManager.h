#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

/**
 * @file SessionManager.h
 * @brief Управление пользовательскими сессиями (ключами шифрования в памяти).
 */

/**
 * @class SessionManager
 * @brief Хранит производные ключи шифрования пользователей в оперативной
 * памяти.
 *
 * Ключ выводится из загруженного пользователем файла-ключа и никогда не
 * сохраняется на диск. Каждая сессия имеет тайм-аут бездействия: если в течение
 * заданного времени не было обращений, ключ автоматически удаляется. Класс
 * потокобезопасен, так как обращения происходят из потока опроса Telegram.
 */
class SessionManager {
   public:
    /**
     * @brief Создаёт менеджер сессий с заданным тайм-аутом бездействия.
     * @param timeout Время бездействия, после которого сессия истекает
     *                (по умолчанию 1800 секунд = 30 минут).
     */
    explicit SessionManager(
        std::chrono::seconds timeout = std::chrono::seconds(1800));

    /**
     * @brief Сохраняет (или заменяет) ключ шифрования для чата.
     * @param chatId Идентификатор чата Telegram.
     * @param key Производный ключ шифрования (32 байта).
     */
    void setKey(int64_t chatId, std::vector<uint8_t> key);

    /**
     * @brief Возвращает ключ шифрования активной сессии и продлевает её.
     * @param chatId Идентификатор чата Telegram.
     * @return Указатель на ключ или nullptr, если сессия отсутствует/истекла.
     */
    const std::vector<uint8_t>* getKey(int64_t chatId);

    /**
     * @brief Проверяет наличие активной (не истёкшей) сессии.
     * @param chatId Идентификатор чата Telegram.
     * @return true, если сессия активна; иначе false.
     */
    bool hasSession(int64_t chatId);

    /**
     * @brief Удаляет сессию пользователя (немедленный «выход»).
     * @param chatId Идентификатор чата Telegram.
     */
    void removeSession(int64_t chatId);

   private:
    /// Тип часов для измерения времени бездействия.
    using Clock = std::chrono::steady_clock;

    /**
     * @struct Session
     * @brief Внутреннее представление сессии: ключ и время последнего доступа.
     */
    struct Session {
        std::vector<uint8_t> key;      ///< Производный ключ шифрования.
        Clock::time_point lastAccess;  ///< Момент последнего обращения.
    };

    /**
     * @brief Проверяет, истекла ли сессия по тайм-ауту бездействия.
     * @param s Сессия для проверки.
     * @return true, если сессия истекла.
     */
    bool isExpired(const Session& s) const;

    mutable std::mutex mutex_;  ///< Мьютекс для потокобезопасного доступа.
    std::unordered_map<int64_t, Session> sessions_;  ///< Активные сессии.
    std::chrono::seconds timeout_;                   ///< Тайм-аут бездействия.
};
