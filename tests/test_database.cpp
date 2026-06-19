/**
 * @file test_database.cpp
 * @brief Модульные тесты модуля хранилища Database (PostgreSQL).
 *
 * Подключается к тестовой БД через переменную окружения TEST_DATABASE_URL
 * (по умолчанию "postgresql://localhost/aaip_test"). Если PostgreSQL
 * недоступен, тесты пропускаются. Каждый тест использует уникальный chat_id и
 * очищает свои данные, чтобы тест-кейсы не зависели друг от друга.
 */

#include <doctest/doctest.h>

#include <cstdlib>
#include <memory>

#include "common/Exceptions.h"
#include "storage/Database.h"

namespace {

/// Возвращает строку подключения для тестовой БД.
std::string testConnStr() {
    const char* env = std::getenv("TEST_DATABASE_URL");
    return env ? env : "postgresql://localhost/aaip_test";
}

/// Создаёт инициализированное соединение с тестовой БД.
/// Возвращает nullptr, если PostgreSQL недоступен.
std::unique_ptr<Database> makeDb() {
    try {
        auto db = std::make_unique<Database>(testConnStr());
        db->initialize();
        return db;
    } catch (...) {
        return nullptr;
    }
}

/// Уникальные chat_id для каждого теста (избегаем пересечений).
constexpr int64_t kChatAdd = 1001;
constexpr int64_t kChatGet = 1002;
constexpr int64_t kChatUpdate = 1003;
constexpr int64_t kChatDelete = 1004;
constexpr int64_t kChatList = 1005;
constexpr int64_t kChatIsolation1 = 1006;
constexpr int64_t kChatIsolation2 = 1007;
constexpr int64_t kChatDup = 1008;

}  // namespace

// Макрос: пропустить тест если PostgreSQL недоступен.
#define SKIP_IF_NO_DB(db)                                 \
    if (!(db)) {                                          \
        MESSAGE("PostgreSQL недоступен — тест пропущен"); \
        return;                                           \
    }

TEST_CASE("addResource + getPassword: успешное сохранение и чтение") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    try {
        db->deleteResource(kChatAdd, "github");
    } catch (...) {
    }  // cleanup — ресурса может не быть
    db->addResource(kChatAdd, "github", "enc_pass");
    CHECK(db->getPassword(kChatAdd, "github") == "enc_pass");
    db->deleteResource(kChatAdd, "github");
}

TEST_CASE("addResource: дубликат приводит к ResourceExistsException") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    try {
        db->deleteResource(kChatDup, "dup");
    } catch (...) {
    }
    db->addResource(kChatDup, "dup", "enc1");
    CHECK_THROWS_AS(db->addResource(kChatDup, "dup", "enc2"),
                    ResourceExistsException);
    db->deleteResource(kChatDup, "dup");
}

TEST_CASE("getPassword: отсутствующий ресурс приводит к исключению") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    CHECK_THROWS_AS(db->getPassword(kChatGet, "нет-такого"),
                    ResourceNotFoundException);
}

TEST_CASE("updatePassword: успешное обновление существующего ресурса") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    try {
        db->deleteResource(kChatUpdate, "mail");
    } catch (...) {
    }
    db->addResource(kChatUpdate, "mail", "old");
    db->updatePassword(kChatUpdate, "mail", "new");
    CHECK(db->getPassword(kChatUpdate, "mail") == "new");
    db->deleteResource(kChatUpdate, "mail");
}

TEST_CASE("updatePassword: отсутствующий ресурс приводит к исключению") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    CHECK_THROWS_AS(db->updatePassword(kChatUpdate, "нет", "x"),
                    ResourceNotFoundException);
}

TEST_CASE("deleteResource: успешное удаление") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    try {
        db->deleteResource(kChatDelete, "vk");
    } catch (...) {
    }
    db->addResource(kChatDelete, "vk", "enc");
    db->deleteResource(kChatDelete, "vk");
    CHECK_THROWS_AS(db->getPassword(kChatDelete, "vk"),
                    ResourceNotFoundException);
}

TEST_CASE("deleteResource: отсутствующий ресурс приводит к исключению") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    CHECK_THROWS_AS(db->deleteResource(kChatDelete, "нет"),
                    ResourceNotFoundException);
}

TEST_CASE("listResources: возвращает все записи в алфавитном порядке") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    try {
        db->deleteResource(kChatList, "a_resource");
    } catch (...) {
    }
    try {
        db->deleteResource(kChatList, "b_resource");
    } catch (...) {
    }
    db->addResource(kChatList, "b_resource", "e2");
    db->addResource(kChatList, "a_resource", "e1");
    auto list = db->listResources(kChatList);
    REQUIRE(list.size() == 2);
    CHECK(list[0].first == "a_resource");
    CHECK(list[1].first == "b_resource");
    db->deleteResource(kChatList, "a_resource");
    db->deleteResource(kChatList, "b_resource");
}

TEST_CASE("listResources: пустой список для нового пользователя") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    CHECK(db->listResources(99999).empty());
}

TEST_CASE("Данные разных пользователей изолированы") {
    auto db = makeDb();
    SKIP_IF_NO_DB(db);
    try {
        db->deleteResource(kChatIsolation1, "shared");
    } catch (...) {
    }
    try {
        db->deleteResource(kChatIsolation2, "shared");
    } catch (...) {
    }
    db->addResource(kChatIsolation1, "shared", "enc1");
    db->addResource(kChatIsolation2, "shared", "enc2");
    CHECK(db->getPassword(kChatIsolation1, "shared") == "enc1");
    CHECK(db->getPassword(kChatIsolation2, "shared") == "enc2");
    db->deleteResource(kChatIsolation1, "shared");
    db->deleteResource(kChatIsolation2, "shared");
}
