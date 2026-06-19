/**
 * @file main.cpp
 * @brief Точка входа Telegram-бота менеджера паролей.
 *
 * Разбирает аргументы командной строки, инициализирует базу данных, менеджер
 * сессий и обработчик бота, после чего запускает цикл опроса. Все ошибки
 * верхнего уровня перехватываются и выводятся пользователю.
 */

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "bot/BotHandler.h"
#include "common/Exceptions.h"
#include "session/SessionManager.h"
#include "storage/Database.h"

/**
 * @struct Config
 * @brief Параметры запуска, задаваемые пользователем.
 */
struct Config {
    std::string token;  ///< Токен Telegram-бота.
    std::string dbPath =
        "postgresql://localhost/aaip";   ///< Строка подключения к PostgreSQL.
    std::chrono::seconds timeout{1800};  ///< Тайм-аут сессии в секундах.
};

/**
 * @brief Выводит краткую справку по использованию программы.
 * @param prog Имя исполняемого файла (argv[0]).
 */
static void printUsage(const std::string& prog) {
    std::cout
        << "Использование: " << prog << " [опции]\n\n"
        << "Опции:\n"
        << "  --token <токен>      Токен Telegram-бота (или переменная\n"
        << "                       окружения TELEGRAM_BOT_TOKEN).\n"
        << "  --db <url>           Строка подключения к PostgreSQL\n"
        << "                       (по умолчанию: "
           "postgresql://localhost/aaip).\n"
        << "  --timeout <секунды>  Тайм-аут сессии (по умолчанию: 1800).\n"
        << "  --help               Показать эту справку.\n";
}

/**
 * @brief Разбирает аргументы командной строки в структуру Config.
 * @param argc Количество аргументов.
 * @param argv Массив аргументов.
 * @return Заполненная конфигурация запуска.
 * @throws ConfigException При отсутствии значения опции или неизвестной опции.
 */
static Config parseArgs(int argc, char* argv[]) {
    Config cfg;
    if (const char* envToken = std::getenv("TELEGRAM_BOT_TOKEN")) {
        cfg.token = envToken;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto needValue = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw ConfigException("Опция " + name + " требует значения");
            }
            return argv[++i];
        };

        if (arg == "--token") {
            cfg.token = needValue(arg);
        } else if (arg == "--db") {
            cfg.dbPath = needValue(arg);
        } else if (arg == "--timeout") {
            cfg.timeout = std::chrono::seconds(std::stoi(needValue(arg)));
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else {
            throw ConfigException("Неизвестная опция: " + arg);
        }
    }

    if (cfg.token.empty()) {
        throw ConfigException(
            "Не задан токен бота. Используйте --token или переменную "
            "окружения TELEGRAM_BOT_TOKEN.");
    }
    return cfg;
}

/**
 * @brief Главная функция программы.
 * @param argc Количество аргументов.
 * @param argv Массив аргументов.
 * @return 0 при успехе, 1 при ошибке.
 */
int main(int argc, char* argv[]) {
    try {
        Config cfg = parseArgs(argc, argv);

        Database db(cfg.dbPath);
        db.initialize();

        SessionManager sm(cfg.timeout);
        BotHandler handler(cfg.token, db, sm);
        handler.run();
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
