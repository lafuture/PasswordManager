/**
 * @file test_crypto.cpp
 * @brief Модульные тесты криптографического модуля CryptoEngine.
 */

#include <doctest/doctest.h>

#include <string>

#include "common/Exceptions.h"
#include "crypto/CryptoEngine.h"

TEST_CASE("deriveKey возвращает ключ длиной 32 байта") {
    auto key = CryptoEngine::deriveKey("мой-секретный-файл-ключ");
    CHECK(key.size() == 32);
}

TEST_CASE("deriveKey детерминирован для одинакового входа") {
    auto key1 = CryptoEngine::deriveKey("одинаковый-вход");
    auto key2 = CryptoEngine::deriveKey("одинаковый-вход");
    CHECK(key1 == key2);
}

TEST_CASE("deriveKey даёт разные ключи для разного входа") {
    auto key1 = CryptoEngine::deriveKey("ключ-A");
    auto key2 = CryptoEngine::deriveKey("ключ-B");
    CHECK(key1 != key2);
}

TEST_CASE("encrypt/decrypt: успешный круговой проход") {
    auto key = CryptoEngine::deriveKey("файл-ключ");
    std::string plaintext = "super_secret_password_123";

    auto blob = CryptoEngine::encrypt(plaintext, key);
    CHECK(blob != plaintext);  // данные действительно зашифрованы

    auto decrypted = CryptoEngine::decrypt(blob, key);
    CHECK(decrypted == plaintext);
}

TEST_CASE("encrypt: одинаковый текст даёт разный шифротекст (случайный IV)") {
    auto key = CryptoEngine::deriveKey("файл-ключ");
    auto blob1 = CryptoEngine::encrypt("одно-и-то-же", key);
    auto blob2 = CryptoEngine::encrypt("одно-и-то-же", key);
    CHECK(blob1 != blob2);
}

TEST_CASE("decrypt: неверный ключ приводит к исключению") {
    auto key = CryptoEngine::deriveKey("правильный-ключ");
    auto wrongKey = CryptoEngine::deriveKey("неправильный-ключ");

    auto blob = CryptoEngine::encrypt("секрет", key);
    CHECK_THROWS_AS(CryptoEngine::decrypt(blob, wrongKey), CryptoException);
}

TEST_CASE("decrypt: повреждённые данные приводят к исключению") {
    auto key = CryptoEngine::deriveKey("файл-ключ");
    auto blob = CryptoEngine::encrypt("секрет", key);
    blob[blob.size() / 2] ^= 0x01;  // портим один символ Base64

    CHECK_THROWS_AS(CryptoEngine::decrypt(blob, key), CryptoException);
}

TEST_CASE("decrypt: слишком короткие данные приводят к исключению") {
    auto key = CryptoEngine::deriveKey("файл-ключ");
    CHECK_THROWS_AS(CryptoEngine::decrypt("AAAA", key), CryptoException);
}

TEST_CASE("encrypt/decrypt: корректная работа с пустой строкой") {
    auto key = CryptoEngine::deriveKey("файл-ключ");
    auto blob = CryptoEngine::encrypt("", key);
    CHECK(CryptoEngine::decrypt(blob, key) == "");
}
