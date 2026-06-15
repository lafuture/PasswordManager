#pragma once

#include <tgbot/tgbot.h>

#include <string>

#include "session/SessionManager.h"
#include "storage/Database.h"

/**
 * @file BotHandler.h
 * @brief Обработчик команд и callback-запросов Telegram-бота.
 */

/**
 * @class BotHandler
 * @brief Связывает Telegram Bot API с модулями хранилища, шифрования и сессий.
 *
 * Регистрирует обработчики команд (/start, /add, /get, /change, /delete,
 * /list), обработку загрузки файла-ключа и нажатий inline-кнопки «Копировать».
 * Каждый обработчик заключён в try/catch: любые исключения нижних слоёв
 * перехватываются и преобразуются в понятное пользователю сообщение.
 */
class BotHandler {
   public:
    /**
     * @brief Создаёт обработчик и регистрирует все события бота.
     * @param token Токен Telegram-бота, полученный от \@BotFather.
     * @param db Ссылка на инициализированную базу данных.
     * @param sm Ссылка на менеджер сессий.
     */
    BotHandler(const std::string& token, Database& db, SessionManager& sm);

    /**
     * @brief Запускает бесконечный цикл длинного опроса (long polling).
     *
     * Сетевые ошибки опроса перехватываются и логируются, после чего опрос
     * продолжается.
     */
    void run();

   private:
    /**
     * @brief Обработчик команды /start — приветствие и запрос файла-ключа.
     * @param msg Входящее сообщение.
     */
    void onStart(TgBot::Message::Ptr msg);

    /**
     * @brief Обработчик команды /help — справка по командам.
     * @param msg Входящее сообщение.
     */
    void onHelp(TgBot::Message::Ptr msg);

    /**
     * @brief Обработчик команды /add — сохранение нового пароля.
     * @param msg Входящее сообщение вида "/add <ресурс> <пароль>".
     */
    void onAdd(TgBot::Message::Ptr msg);

    /**
     * @brief Обработчик команды /get — получение пароля с кнопкой «Копировать».
     * @param msg Входящее сообщение вида "/get <ресурс>".
     */
    void onGet(TgBot::Message::Ptr msg);

    /**
     * @brief Обработчик команды /change — изменение существующего пароля.
     * @param msg Входящее сообщение вида "/change <ресурс> <новый_пароль>".
     */
    void onChange(TgBot::Message::Ptr msg);

    /**
     * @brief Обработчик команды /delete — удаление ресурса.
     * @param msg Входящее сообщение вида "/delete <ресурс>".
     */
    void onDelete(TgBot::Message::Ptr msg);

    /**
     * @brief Обработчик команды /list — список всех ресурсов с паролями.
     * @param msg Входящее сообщение.
     */
    void onList(TgBot::Message::Ptr msg);

    /**
     * @brief Обработчик загрузки документа — приём файла-ключа.
     * @param msg Входящее сообщение с прикреплённым документом.
     */
    void onDocument(TgBot::Message::Ptr msg);

    /**
     * @brief Обработчик нажатия inline-кнопки «Копировать».
     * @param query Callback-запрос с данными вида "copy:<ресурс>".
     */
    void onCallbackQuery(TgBot::CallbackQuery::Ptr query);

    /**
     * @brief Проверяет наличие активной сессии пользователя.
     * @param chatId Идентификатор чата.
     * @return Указатель на ключ шифрования.
     * @throws SessionException Если сессия отсутствует или истекла.
     */
    const std::vector<uint8_t>* requireKey(int64_t chatId);

    /**
     * @brief Безопасно удаляет сообщение, игнорируя ошибки API.
     * @param chatId Идентификатор чата.
     * @param messageId Идентификатор сообщения.
     */
    void tryDeleteMessage(int64_t chatId, std::int32_t messageId);

    TgBot::Bot bot_;      ///< Экземпляр Telegram-бота.
    Database& db_;        ///< Хранилище паролей.
    SessionManager& sm_;  ///< Менеджер сессий.
};
