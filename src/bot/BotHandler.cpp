#include "bot/BotHandler.h"

#include <iostream>
#include <sstream>

#include "common/Exceptions.h"
#include "crypto/CryptoEngine.h"

BotHandler::BotHandler(const std::string& token, Database& db,
                       SessionManager& sm)
    : bot_(token), db_(db), sm_(sm) {
    bot_.getEvents().onCommand(
        "start", [this](TgBot::Message::Ptr msg) { onStart(msg); });
    bot_.getEvents().onCommand(
        "help", [this](TgBot::Message::Ptr msg) { onHelp(msg); });
    bot_.getEvents().onCommand("add",
                               [this](TgBot::Message::Ptr msg) { onAdd(msg); });
    bot_.getEvents().onCommand("get",
                               [this](TgBot::Message::Ptr msg) { onGet(msg); });
    bot_.getEvents().onCommand(
        "change", [this](TgBot::Message::Ptr msg) { onChange(msg); });
    bot_.getEvents().onCommand(
        "delete", [this](TgBot::Message::Ptr msg) { onDelete(msg); });
    bot_.getEvents().onCommand(
        "list", [this](TgBot::Message::Ptr msg) { onList(msg); });
    bot_.getEvents().onNonCommandMessage([this](TgBot::Message::Ptr msg) {
        if (msg->document) {
            onDocument(msg);
        }
    });
    bot_.getEvents().onCallbackQuery(
        [this](TgBot::CallbackQuery::Ptr query) { onCallbackQuery(query); });
}

void BotHandler::run() {
    std::cout << "Бот запущен: @" << bot_.getApi().getMe()->username
              << std::endl;
    TgBot::TgLongPoll longPoll(bot_);
    while (true) {
        try {
            longPoll.start();
        } catch (const std::exception& e) {
            std::cerr << "Ошибка опроса: " << e.what() << std::endl;
        }
    }
}

const std::vector<uint8_t>* BotHandler::requireKey(int64_t chatId) {
    const auto* key = sm_.getKey(chatId);
    if (!key) {
        throw SessionException(
            "Сессия истекла или не начата. Отправьте файл-ключ для "
            "продолжения.");
    }
    return key;
}

void BotHandler::tryDeleteMessage(int64_t chatId, std::int32_t messageId) {
    try {
        bot_.getApi().deleteMessage(chatId, messageId);
    } catch (...) {
        // Удаление может быть недоступно (например, нет прав) — игнорируем.
    }
}

void BotHandler::onStart(TgBot::Message::Ptr msg) {
    try {
        bot_.getApi().sendMessage(
            msg->chat->id,
            "🔐 *Менеджер паролей*\n\n"
            "Отправьте файл-ключ для начала работы.\n"
            "Этот файл используется для шифрования и расшифровки ваших "
            "паролей.\n\n"
            "После загрузки ключа доступны команды:\n"
            "/add <ресурс> <пароль> — сохранить пароль\n"
            "/get <ресурс> — получить пароль\n"
            "/change <ресурс> <новый\\_пароль> — изменить пароль\n"
            "/delete <ресурс> — удалить ресурс\n"
            "/list — все ресурсы и пароли\n"
            "/help — справка",
            nullptr, nullptr, nullptr, "Markdown");
    } catch (const std::exception& e) {
        std::cerr << "onStart: " << e.what() << std::endl;
    }
}

void BotHandler::onHelp(TgBot::Message::Ptr msg) {
    try {
        bot_.getApi().sendMessage(
            msg->chat->id,
            "📋 *Команды:*\n\n"
            "/add <ресурс> <пароль> — сохранить новый пароль\n"
            "/get <ресурс> — получить пароль\n"
            "/change <ресурс> <новый\\_пароль> — изменить пароль\n"
            "/delete <ресурс> — удалить ресурс\n"
            "/list — показать все ресурсы\n\n"
            "⚠️ Для работы необходимо сначала отправить файл-ключ.",
            nullptr, nullptr, nullptr, "Markdown");
    } catch (const std::exception& e) {
        std::cerr << "onHelp: " << e.what() << std::endl;
    }
}

void BotHandler::onDocument(TgBot::Message::Ptr msg) {
    try {
        auto file = bot_.getApi().getFile(msg->document->fileId);
        if (file->fileSize > 1024 * 1024) {
            throw AppException("Файл слишком большой. Максимум — 1 МБ.");
        }

        std::string content = bot_.getApi().downloadFile(file->filePath);
        auto key = CryptoEngine::deriveKey(content);
        sm_.setKey(msg->chat->id, std::move(key));

        bot_.getApi().sendMessage(
            msg->chat->id,
            "✅ Ключ принят. Теперь вы можете управлять паролями.\n"
            "Используйте /help для списка команд.");
    } catch (const std::exception& e) {
        bot_.getApi().sendMessage(msg->chat->id, "❌ Ошибка обработки ключа: " +
                                                     std::string(e.what()));
    }
}

void BotHandler::onAdd(TgBot::Message::Ptr msg) {
    try {
        const auto* key = requireKey(msg->chat->id);

        std::istringstream iss(msg->text);
        std::string cmd, resource, password;
        iss >> cmd >> resource >> password;
        if (resource.empty() || password.empty()) {
            throw AppException("Использование: /add <ресурс> <пароль>");
        }

        auto encrypted = CryptoEngine::encrypt(password, *key);
        db_.addResource(msg->chat->id, resource, encrypted);
        tryDeleteMessage(msg->chat->id, msg->messageId);

        bot_.getApi().sendMessage(msg->chat->id,
                                  "✅ Пароль для *" + resource + "* сохранён.",
                                  nullptr, nullptr, nullptr, "Markdown");
    } catch (const std::exception& e) {
        bot_.getApi().sendMessage(msg->chat->id, "❌ " + std::string(e.what()));
    }
}

void BotHandler::onGet(TgBot::Message::Ptr msg) {
    try {
        const auto* key = requireKey(msg->chat->id);

        std::istringstream iss(msg->text);
        std::string cmd, resource;
        iss >> cmd >> resource;
        if (resource.empty()) {
            throw AppException("Использование: /get <ресурс>");
        }

        auto encrypted = db_.getPassword(msg->chat->id, resource);
        auto password = CryptoEngine::decrypt(encrypted, *key);

        auto keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
        auto btn = std::make_shared<TgBot::InlineKeyboardButton>();
        btn->text = "📋 Копировать";
        btn->callbackData = "copy:" + resource;
        keyboard->inlineKeyboard.push_back({btn});

        bot_.getApi().sendMessage(msg->chat->id,
                                  "🔑 *" + resource + "*\n`" + password + "`",
                                  nullptr, nullptr, keyboard, "Markdown");
    } catch (const std::exception& e) {
        bot_.getApi().sendMessage(msg->chat->id, "❌ " + std::string(e.what()));
    }
}

void BotHandler::onChange(TgBot::Message::Ptr msg) {
    try {
        const auto* key = requireKey(msg->chat->id);

        std::istringstream iss(msg->text);
        std::string cmd, resource, password;
        iss >> cmd >> resource >> password;
        if (resource.empty() || password.empty()) {
            throw AppException(
                "Использование: /change <ресурс> <новый_пароль>");
        }

        auto encrypted = CryptoEngine::encrypt(password, *key);
        db_.updatePassword(msg->chat->id, resource, encrypted);
        tryDeleteMessage(msg->chat->id, msg->messageId);

        bot_.getApi().sendMessage(msg->chat->id,
                                  "✅ Пароль для *" + resource + "* обновлён.",
                                  nullptr, nullptr, nullptr, "Markdown");
    } catch (const std::exception& e) {
        bot_.getApi().sendMessage(msg->chat->id, "❌ " + std::string(e.what()));
    }
}

void BotHandler::onDelete(TgBot::Message::Ptr msg) {
    try {
        requireKey(msg->chat->id);

        std::istringstream iss(msg->text);
        std::string cmd, resource;
        iss >> cmd >> resource;
        if (resource.empty()) {
            throw AppException("Использование: /delete <ресурс>");
        }

        db_.deleteResource(msg->chat->id, resource);
        bot_.getApi().sendMessage(msg->chat->id,
                                  "✅ Ресурс *" + resource + "* удалён.",
                                  nullptr, nullptr, nullptr, "Markdown");
    } catch (const std::exception& e) {
        bot_.getApi().sendMessage(msg->chat->id, "❌ " + std::string(e.what()));
    }
}

void BotHandler::onList(TgBot::Message::Ptr msg) {
    try {
        const auto* key = requireKey(msg->chat->id);

        auto resources = db_.listResources(msg->chat->id);
        if (resources.empty()) {
            bot_.getApi().sendMessage(msg->chat->id,
                                      "📭 У вас нет сохранённых паролей.");
            return;
        }

        std::string text = "🔐 *Ваши ресурсы:*\n\n";
        auto keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();

        for (const auto& [resource, encrypted] : resources) {
            try {
                auto password = CryptoEngine::decrypt(encrypted, *key);
                text += "▫️ *" + resource + "*: `" + password + "`\n";

                auto btn = std::make_shared<TgBot::InlineKeyboardButton>();
                btn->text = "📋 " + resource;
                btn->callbackData = "copy:" + resource;
                keyboard->inlineKeyboard.push_back({btn});
            } catch (const std::exception&) {
                text += "▫️ *" + resource + "*: ⚠️ ошибка расшифровки\n";
            }
        }

        bot_.getApi().sendMessage(msg->chat->id, text, nullptr, nullptr,
                                  keyboard, "Markdown");
    } catch (const std::exception& e) {
        bot_.getApi().sendMessage(msg->chat->id, "❌ " + std::string(e.what()));
    }
}

void BotHandler::onCallbackQuery(TgBot::CallbackQuery::Ptr query) {
    try {
        const auto& data = query->data;
        if (data.rfind("copy:", 0) != 0) {
            return;
        }

        std::string resource = data.substr(5);
        int64_t chatId = query->message->chat->id;

        const auto* key = sm_.getKey(chatId);
        if (!key) {
            bot_.getApi().answerCallbackQuery(query->id,
                                              "⚠️ Загрузите файл-ключ");
            return;
        }

        auto encrypted = db_.getPassword(chatId, resource);
        auto password = CryptoEngine::decrypt(encrypted, *key);
        bot_.getApi().answerCallbackQuery(query->id, password, true);
    } catch (const std::exception& e) {
        bot_.getApi().answerCallbackQuery(query->id,
                                          "❌ " + std::string(e.what()));
    }
}
