/**
 * @file test_database.cpp
 * @brief Модульные тесты модуля хранилища Database.
 *
 * Используется база данных в оперативной памяти (":memory:"), чтобы тесты не
 * затрагивали файловую систему.
 */

#include <doctest/doctest.h>

#include <memory>

#include "common/Exceptions.h"
#include "storage/Database.h"

namespace {
/// Создаёт инициализированную базу в памяти для теста.
std::unique_ptr<Database> makeDb() {
    auto db = std::make_unique<Database>(":memory:");
    db->initialize();
    return db;
}
constexpr int64_t kChat = 42;
}  // namespace

TEST_CASE("addResource + getPassword: успешное сохранение и чтение") {
    auto db = makeDb();
    db->addResource(kChat, "github", "enc_pass");
    CHECK(db->getPassword(kChat, "github") == "enc_pass");
}

TEST_CASE("addResource: дубликат приводит к ResourceExistsException") {
    auto db = makeDb();
    db->addResource(kChat, "github", "enc1");
    CHECK_THROWS_AS(db->addResource(kChat, "github", "enc2"),
                    ResourceExistsException);
}

TEST_CASE("getPassword: отсутствующий ресурс приводит к исключению") {
    auto db = makeDb();
    CHECK_THROWS_AS(db->getPassword(kChat, "нет-такого"),
                    ResourceNotFoundException);
}

TEST_CASE("updatePassword: успешное обновление существующего ресурса") {
    auto db = makeDb();
    db->addResource(kChat, "mail", "old");
    db->updatePassword(kChat, "mail", "new");
    CHECK(db->getPassword(kChat, "mail") == "new");
}

TEST_CASE("updatePassword: отсутствующий ресурс приводит к исключению") {
    auto db = makeDb();
    CHECK_THROWS_AS(db->updatePassword(kChat, "нет", "x"),
                    ResourceNotFoundException);
}

TEST_CASE("deleteResource: успешное удаление") {
    auto db = makeDb();
    db->addResource(kChat, "vk", "enc");
    db->deleteResource(kChat, "vk");
    CHECK_THROWS_AS(db->getPassword(kChat, "vk"), ResourceNotFoundException);
}

TEST_CASE("deleteResource: отсутствующий ресурс приводит к исключению") {
    auto db = makeDb();
    CHECK_THROWS_AS(db->deleteResource(kChat, "нет"),
                    ResourceNotFoundException);
}

TEST_CASE("listResources: возвращает все записи в алфавитном порядке") {
    auto db = makeDb();
    db->addResource(kChat, "b_resource", "e2");
    db->addResource(kChat, "a_resource", "e1");
    auto list = db->listResources(kChat);

    REQUIRE(list.size() == 2);
    CHECK(list[0].first == "a_resource");
    CHECK(list[1].first == "b_resource");
}

TEST_CASE("listResources: пустой список для нового пользователя") {
    auto db = makeDb();
    CHECK(db->listResources(kChat).empty());
}

TEST_CASE("Данные разных пользователей изолированы") {
    auto db = makeDb();
    db->addResource(1, "shared", "enc1");
    db->addResource(2, "shared", "enc2");

    CHECK(db->getPassword(1, "shared") == "enc1");
    CHECK(db->getPassword(2, "shared") == "enc2");
}
