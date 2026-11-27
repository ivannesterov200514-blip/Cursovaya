#include "files.h"

#include <chrono>
#include <stdexcept>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

using namespace std;
namespace fs = filesystem;

WorkspaceLock::WorkspaceLock() {
    handle = open(".stego.lock", O_WRONLY | O_CREAT | O_NOFOLLOW, 0600);
    if (handle == -1)
        throw runtime_error("Рабочий каталог недоступен");
    if (flock(handle, LOCK_EX | LOCK_NB) != 0) {
        close(handle);
        throw runtime_error("Рабочий каталог занят или недоступен");
    }
}

WorkspaceLock::~WorkspaceLock() {
    close(handle);
}

FileWriter::FileWriter(const fs::path& path) : destination(path) {
    const auto stamp = chrono::steady_clock::now().time_since_epoch().count();
    const fs::path parent = path.parent_path();
    for (int i = 0; i < 100; i++) {
        fs::path candidate = parent / (".stego-tmp-" + to_string(stamp) + "-" + to_string(i));
        if (fs::create_directory(candidate)) {
            temp_dir = candidate;
            break;
        }
    }
    if (temp_dir.empty())
        throw runtime_error("Не удалось создать временный каталог");

    file.open(temp_dir / "data", ios::binary);
    if (!file) {
        error_code error;
        fs::remove_all(temp_dir, error);
        throw runtime_error("Не удалось создать временный файл");
    }
}

FileWriter::~FileWriter() {
    if (file.is_open())
        file.close();
    error_code error;
    fs::remove_all(temp_dir, error);
}

ofstream& FileWriter::stream() {
    return file;
}

void FileWriter::save(bool replace) {
    file.close();
    if (!file)
        throw runtime_error("Ошибка записи файла " + destination.u8string());
    if (!replace && file_exists(destination))
        throw runtime_error("Файл результата уже существует. Укажите другое имя");
    fs::rename(temp_dir / "data", destination);
}

bool file_exists(const fs::path& path) {
    const auto status = fs::symlink_status(path);
    if (!fs::exists(status))
        return false;
    if (!fs::is_regular_file(status))
        throw runtime_error("Ожидался обычный файл: " + path.u8string());
    return true;
}

bool valid_file_name(const string& name) {
    if (name.empty() || name.size() > 240 || name.front() == '.' || name.front() == ' ' || name.back() == ' ' || name.back() == '.')
        return false;
    for (unsigned char ch : name) {
        if (ch < 32 || ch == 127 || ch == '/')
            return false;
    }
    return true;
}

fs::path user_file(const string& name, bool output) {
    if (!valid_file_name(name))
        throw runtime_error("Укажите только имя файла из каталога files");

    const fs::path folder = "files";
    if (!fs::is_directory(fs::symlink_status(folder)))
        throw runtime_error("Каталог files отсутствует или недоступен");
    const fs::path path = folder / fs::u8path(name);
    const bool exists = file_exists(path);
    if (output && exists)
        throw runtime_error("Файл результата уже существует. Укажите другое имя");
    if (!output && !exists)
        throw runtime_error("Файл не найден: " + name);
    return path;
}

void write_number(ostream& file, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        file.put(static_cast<char>(value & 255u));
        value >>= 8;
    }
}

uint32_t read_number(istream& file) {
    uint32_t value = 0;
    for (int i = 0; i < 4; i++) {
        const int byte = file.get();
        if (byte == char_traits<char>::eof())
            throw runtime_error("Ошибка чтения бинарного файла или неверный формат");
        value |= static_cast<uint32_t>(byte) << (i * 8);
    }
    return value;
}