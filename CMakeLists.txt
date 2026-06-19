cmake_minimum_required(VERSION 3.20)

# Boost 1.90 в данном окружении использует устаревший модуль FindBoost,
# к которому обращается tgbot-cpp. Включаем совместимый режим политики.
cmake_policy(SET CMP0167 OLD)

project(aaip_bot LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Собирать модульные тесты" ON)

# --- Поиск установленных зависимостей -----------------------------------
set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED)

# libpq (C-библиотека PostgreSQL) ищется через стандартный модуль CMake.
set(PostgreSQL_ROOT "/opt/homebrew/opt/postgresql@14")
find_package(PostgreSQL REQUIRED)

# --- Сторонние зависимости через CMake/FetchContent ----------------------
# Некоторые зависимости объявляют очень старый минимум CMake; разрешаем сборку.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
include(FetchContent)

FetchContent_Declare(
    tgbot-cpp
    GIT_REPOSITORY https://github.com/reo7sp/tgbot-cpp.git
    GIT_TAG master
)

# libpqxx — C++ обёртка над libpq, собирается средствами CMake (FetchContent).
FetchContent_Declare(
    libpqxx
    GIT_REPOSITORY https://github.com/jtv/libpqxx.git
    GIT_TAG 7.9.2
)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)

# Отключаем тесты и документацию самой libpqxx при сборке.
set(SKIP_BUILD_TEST ON CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(tgbot-cpp libpqxx doctest)

# --- Базовая библиотека (используется и ботом, и тестами) ----------------
add_library(aaip_core STATIC
    src/crypto/CryptoEngine.cpp
    src/storage/Database.cpp
    src/session/SessionManager.cpp
)
target_include_directories(aaip_core PUBLIC src)
target_link_libraries(aaip_core
    PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    libpqxx::pqxx
)

# --- Исполняемый файл бота ----------------------------------------------
add_executable(aaip_bot
    src/main.cpp
    src/bot/BotHandler.cpp
)
target_link_libraries(aaip_bot
    PRIVATE
    aaip_core
    TgBot
    Boost::boost
)

# --- Тесты ---------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()
    add_executable(aaip_tests
        tests/test_main.cpp
        tests/test_crypto.cpp
        tests/test_database.cpp
        tests/test_session.cpp
    )
    target_link_libraries(aaip_tests
        PRIVATE
        aaip_core
        doctest::doctest
    )
    add_test(NAME unit_tests COMMAND aaip_tests)
endif()
