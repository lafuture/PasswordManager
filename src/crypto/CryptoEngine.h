#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * @file CryptoEngine.h
 * @brief Криптографический модуль: вывод ключа и аутентифицированное
 * шифрование.
 */

/**
 * @class CryptoEngine
 * @brief Шифрование и расшифровка паролей алгоритмом AES-256-GCM.
 *
 * Класс предоставляет статические функции для:
 *  - вывода 256-битного ключа из содержимого файла-ключа (PBKDF2-HMAC-SHA256);
 *  - аутентифицированного шифрования открытого текста (AES-256-GCM);
 *  - расшифровки с проверкой целостности и подлинности данных.
 *
 * Для каждой операции шифрования генерируется случайный вектор инициализации
 * (IV), а результат кодируется в Base64 в формате `IV || ciphertext || tag`.
 * Все ошибки выбрасываются как CryptoException.
 */
class CryptoEngine {
   public:
    /**
     * @brief Выводит симметричный ключ из содержимого файла-ключа.
     *
     * Применяется алгоритм PBKDF2-HMAC-SHA256 с фиксированной солью приложения
     * и большим числом итераций для защиты от перебора.
     *
     * @param keyFileContent Сырое содержимое загруженного пользователем
     * файла-ключа.
     * @return Вектор из 32 байт — производный ключ шифрования.
     * @throws CryptoException Если вывод ключа завершился неудачей.
     */
    static std::vector<uint8_t> deriveKey(const std::string& keyFileContent);

    /**
     * @brief Шифрует открытый текст алгоритмом AES-256-GCM.
     *
     * @param plaintext Открытый текст (пароль) для шифрования.
     * @param key Ключ шифрования длиной 32 байта, полученный из deriveKey().
     * @return Строка Base64, содержащая `IV || ciphertext || tag`.
     * @throws CryptoException При ошибке генерации IV или работы шифратора.
     */
    static std::string encrypt(const std::string& plaintext,
                               const std::vector<uint8_t>& key);

    /**
     * @brief Расшифровывает данные, ранее зашифрованные encrypt().
     *
     * Проверяет тег аутентификации GCM: при неверном ключе или повреждённых
     * данных расшифровка завершается исключением, а не возвратом мусора.
     *
     * @param blob Строка Base64 в формате `IV || ciphertext || tag`.
     * @param key Ключ шифрования длиной 32 байта.
     * @return Расшифрованный открытый текст.
     * @throws CryptoException Если данные повреждены, слишком короткие или ключ
     * неверный.
     */
    static std::string decrypt(const std::string& blob,
                               const std::vector<uint8_t>& key);

   private:
    /**
     * @brief Кодирует двоичные данные в строку Base64.
     * @param data Двоичные данные.
     * @return Строка Base64 без переводов строк.
     */
    static std::string toBase64(const std::vector<uint8_t>& data);

    /**
     * @brief Декодирует строку Base64 в двоичные данные.
     * @param encoded Строка Base64.
     * @return Декодированные двоичные данные.
     * @throws CryptoException При ошибке декодирования.
     */
    static std::vector<uint8_t> fromBase64(const std::string& encoded);

    /// Размер ключа AES-256 в байтах.
    static constexpr int KEY_SIZE = 32;
    /// Размер вектора инициализации (IV) для GCM в байтах.
    static constexpr int IV_SIZE = 12;
    /// Размер тега аутентификации GCM в байтах.
    static constexpr int TAG_SIZE = 16;
    /// Число итераций PBKDF2 (рекомендация OWASP).
    static constexpr int PBKDF2_ITERATIONS = 600000;
};
