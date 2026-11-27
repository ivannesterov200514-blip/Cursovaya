#include "auth.h"
#include "files.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <array>
#include <stdexcept>

using namespace std;

static const char ACCOUNT_FILE[] = "account.bin";
static const string SIGNATURE = "STGAUTH1";
static const uint32_t ITERATIONS = 600000;
using Salt = array<unsigned char, 16>;
using Hash = array<unsigned char, 32>;

bool account_exists() {
    return file_exists(ACCOUNT_FILE);
}

bool valid_password(const string& password) {
    return password.size() >= 8 && password.size() <= 128 && password.find_first_not_of(' ') != string::npos;
}

static Hash password_hash(const string& password, const Salt& salt, uint32_t iterations) {
    Hash hash{};
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt.data(),
                          static_cast<int>(salt.size()), static_cast<int>(iterations),
                          EVP_sha256(), static_cast<int>(hash.size()), hash.data()) != 1)
        throw runtime_error("Не удалось выполнить проверку пароля");
    return hash;
}

void register_account(const string& password) {
    if (account_exists())
        throw runtime_error("Пароль уже настроен");
    if (!valid_password(password))
        throw runtime_error("Введите пароль длиной от 8 до 128 байт");

    Salt salt{};
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
        throw runtime_error("Не удалось настроить учётные данные");

    const Hash hash = password_hash(password, salt, ITERATIONS);
    FileWriter output(ACCOUNT_FILE);
    ofstream& file = output.stream();
    file << SIGNATURE;
    write_number(file, ITERATIONS);
    file.write(reinterpret_cast<const char*>(salt.data()), salt.size());
    file.write(reinterpret_cast<const char*>(hash.data()), hash.size());
    output.save();
}

bool check_password(const string& password) {
    ifstream file(ACCOUNT_FILE, ios::binary);
    string signature(SIGNATURE.size(), '\0');
    file.read(signature.data(), signature.size());
    if (!file || signature != SIGNATURE)
        throw runtime_error("Не удалось прочитать учётные данные");
    uint32_t iterations;
    try {
        iterations = read_number(file);
    } catch (const runtime_error&) {
        throw runtime_error("Не удалось прочитать учётные данные");
    }

    Salt salt{};
    Hash saved{};
    file.read(reinterpret_cast<char*>(salt.data()), salt.size());
    file.read(reinterpret_cast<char*>(saved.data()), saved.size());
    if (!file || file.peek() != char_traits<char>::eof() || file.bad() || iterations != ITERATIONS)
        throw runtime_error("Не удалось прочитать учётные данные");

    if (!valid_password(password))
        return false;
    const Hash current = password_hash(password, salt, iterations);
    return CRYPTO_memcmp(current.data(), saved.data(), current.size()) == 0;
}