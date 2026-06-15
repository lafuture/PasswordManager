#include "crypto/CryptoEngine.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <memory>

#include "common/Exceptions.h"

namespace {
/// Фиксированная соль приложения для PBKDF2 (16 байт).
const uint8_t APP_SALT[] = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6, 0x07, 0x18,
                            0x29, 0x3a, 0x4b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90};
}  // namespace

std::vector<uint8_t> CryptoEngine::deriveKey(
    const std::string& keyFileContent) {
    std::vector<uint8_t> key(KEY_SIZE);
    if (PKCS5_PBKDF2_HMAC(keyFileContent.data(),
                          static_cast<int>(keyFileContent.size()), APP_SALT,
                          sizeof(APP_SALT), PBKDF2_ITERATIONS, EVP_sha256(),
                          KEY_SIZE, key.data()) != 1) {
        throw CryptoException("Не удалось вывести ключ (PBKDF2)");
    }
    return key;
}

std::string CryptoEngine::encrypt(const std::string& plaintext,
                                  const std::vector<uint8_t>& key) {
    if (key.size() != KEY_SIZE) {
        throw CryptoException("Неверный размер ключа шифрования");
    }

    std::vector<uint8_t> iv(IV_SIZE);
    if (RAND_bytes(iv.data(), IV_SIZE) != 1) {
        throw CryptoException("Не удалось сгенерировать случайный IV");
    }

    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
        EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) {
        throw CryptoException("Не удалось создать контекст шифрования");
    }

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(),
                           iv.data()) != 1) {
        throw CryptoException("EVP_EncryptInit_ex завершился ошибкой");
    }

    std::vector<uint8_t> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len,
                          reinterpret_cast<const uint8_t*>(plaintext.data()),
                          static_cast<int>(plaintext.size())) != 1) {
        throw CryptoException("EVP_EncryptUpdate завершился ошибкой");
    }
    int ciphertext_len = len;

    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + len, &final_len) !=
        1) {
        throw CryptoException("EVP_EncryptFinal_ex завершился ошибкой");
    }
    ciphertext_len += final_len;

    std::vector<uint8_t> tag(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, TAG_SIZE,
                            tag.data()) != 1) {
        throw CryptoException("Не удалось получить тег GCM");
    }

    std::vector<uint8_t> result;
    result.reserve(IV_SIZE + ciphertext_len + TAG_SIZE);
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(),
                  ciphertext.begin() + ciphertext_len);
    result.insert(result.end(), tag.begin(), tag.end());

    return toBase64(result);
}

std::string CryptoEngine::decrypt(const std::string& blob,
                                  const std::vector<uint8_t>& key) {
    if (key.size() != KEY_SIZE) {
        throw CryptoException("Неверный размер ключа шифрования");
    }

    auto data = fromBase64(blob);
    if (data.size() < static_cast<size_t>(IV_SIZE + TAG_SIZE)) {
        throw CryptoException("Зашифрованные данные слишком короткие");
    }

    const uint8_t* iv = data.data();
    int ciphertext_len = static_cast<int>(data.size()) - IV_SIZE - TAG_SIZE;
    const uint8_t* ciphertext = data.data() + IV_SIZE;
    const uint8_t* tag = data.data() + IV_SIZE + ciphertext_len;

    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(
        EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) {
        throw CryptoException("Не удалось создать контекст шифрования");
    }

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(),
                           iv) != 1) {
        throw CryptoException("EVP_DecryptInit_ex завершился ошибкой");
    }

    std::vector<uint8_t> plaintext(ciphertext_len + EVP_MAX_BLOCK_LENGTH);
    int len = 0;
    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, ciphertext,
                          ciphertext_len) != 1) {
        throw CryptoException("EVP_DecryptUpdate завершился ошибкой");
    }
    int plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, TAG_SIZE,
                            const_cast<uint8_t*>(tag)) != 1) {
        throw CryptoException("Не удалось установить тег GCM");
    }

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &final_len) !=
        1) {
        throw CryptoException(
            "Расшифровка не удалась: неверный ключ или повреждённые данные");
    }
    plaintext_len += final_len;

    return std::string(reinterpret_cast<char*>(plaintext.data()),
                       plaintext_len);
}

// === НАЧАЛО ЗАИМСТВОВАННОГО КОДА ===
// Источник: официальная документация OpenSSL (OpenSSL Wiki, "EVP / BIO Base64
// Encoding/Decoding"), https://wiki.openssl.org/index.php/EVP_Message_Digests
// Идиома кодирования/декодирования Base64 через цепочку BIO с флагом
// BIO_FLAGS_BASE64_NO_NL. Адаптировано под std::string / std::vector.
std::string CryptoEngine::toBase64(const std::vector<uint8_t>& data) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data.data(), static_cast<int>(data.size()));
    BIO_flush(b64);

    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(b64);
    return result;
}

std::vector<uint8_t> CryptoEngine::fromBase64(const std::string& encoded) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem =
        BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> result(encoded.size());
    int len = BIO_read(b64, result.data(), static_cast<int>(result.size()));
    BIO_free_all(b64);

    if (len < 0) {
        throw CryptoException("Ошибка декодирования Base64");
    }
    result.resize(len);
    return result;
}
// === КОНЕЦ ЗАИМСТВОВАННОГО КОДА ===
