/**
 * @file test_session.cpp
 * @brief Модульные тесты менеджера сессий SessionManager.
 */

#include <doctest/doctest.h>

#include <chrono>
#include <thread>
#include <vector>

#include "session/SessionManager.h"

namespace {
const std::vector<uint8_t> kKey = {1, 2, 3, 4, 5};
constexpr int64_t kChat = 7;
}  // namespace

TEST_CASE("setKey + getKey: успешное сохранение и получение ключа") {
    SessionManager sm;
    sm.setKey(kChat, kKey);

    const auto* key = sm.getKey(kChat);
    REQUIRE(key != nullptr);
    CHECK(*key == kKey);
}

TEST_CASE("getKey: для отсутствующей сессии возвращает nullptr") {
    SessionManager sm;
    CHECK(sm.getKey(999) == nullptr);
}

TEST_CASE("hasSession: положительный и отрицательный случаи") {
    SessionManager sm;
    CHECK_FALSE(sm.hasSession(kChat));
    sm.setKey(kChat, kKey);
    CHECK(sm.hasSession(kChat));
}

TEST_CASE("removeSession: сессия удаляется") {
    SessionManager sm;
    sm.setKey(kChat, kKey);
    sm.removeSession(kChat);
    CHECK_FALSE(sm.hasSession(kChat));
}

TEST_CASE("Сессия истекает по тайм-ауту бездействия") {
    SessionManager sm(std::chrono::seconds(0));  // мгновенное истечение
    sm.setKey(kChat, kKey);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK_FALSE(sm.hasSession(kChat));
    CHECK(sm.getKey(kChat) == nullptr);
}

TEST_CASE("Активная сессия не истекает при большом тайм-ауте") {
    SessionManager sm(std::chrono::seconds(3600));
    sm.setKey(kChat, kKey);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CHECK(sm.hasSession(kChat));
}
