#include "records.h"
#include "files.h"
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

static const string SIGNATURE = "STGREC01";

bool valid_title(const string& title) {
    if (title.size() > MAX_TITLE || title.find_first_not_of(' ') == string::npos)
        return false;
    for (unsigned char ch : title) {
        if (ch < 32 || ch == 127)
            return false;
    }
    return true;
}

static ifstream open_records(const fs::path& path) {
    if (!file_exists(path))
        throw runtime_error("Файл записей не найден: " + path.u8string());
    ifstream file(path, ios::binary);
    if (!file)
        throw runtime_error("Не удалось открыть файл " + path.u8string());

    string header(SIGNATURE.size(), '\0');
    file.read(header.data(), header.size());
    if (!file || header != SIGNATURE)
        throw runtime_error("Неверный формат файла " + path.u8string());
    return file;
}

static string read_string(istream& file, size_t max_size) {
    const uint32_t length = read_number(file);
    if (length == 0 || length > max_size)
        throw runtime_error("Неверная длина поля в файле записей");

    string value(length, '\0');
    file.read(value.data(), length);
    if (!file)
        throw runtime_error("Ошибка чтения поля записи");
    return value;
}

static bool read_record(istream& file, Record& record) {
    if (file.peek() == char_traits<char>::eof()) {
        if (file.bad() || !file.eof())
            throw runtime_error("Ошибка чтения файла записей");
        return false;
    }
    record.id = read_number(file);
    record.title = read_string(file, MAX_TITLE);
    record.image = read_string(file, 240);
    record.payload = read_string(file, 240);
    if (record.id == 0 || !valid_title(record.title) ||
        !valid_file_name(record.image) || !valid_file_name(record.payload))
        throw runtime_error("Недопустимое поле в файле записей");
    return true;
}

static void write_string(ostream& file, const string& value) {
    write_number(file, static_cast<uint32_t>(value.size()));
    file.write(value.data(), value.size());
}

static void write_record(ostream& file, const Record& record) {
    write_number(file, record.id);
    write_string(file, record.title);
    write_string(file, record.image);
    write_string(file, record.payload);
    if (!file)
        throw runtime_error("Ошибка записи каталога заданий");
}

void create_records() {
    if (file_exists(PRIMARY_FILE))
        throw runtime_error("Исходный файл уже существует");

    FileWriter output(PRIMARY_FILE);
    output.stream() << SIGNATURE;
    output.save();
}

bool find_record(uint32_t id, Record& result) {
    auto file = open_records(PRIMARY_FILE);
    Record current;
    bool found = false;
    while (read_record(file, current)) {
        if (current.id != id)
            continue;
        if (found)
            throw runtime_error("В каталоге повторяется номер записи");
        result = current;
        found = true;
    }
    return found;
}

// Замена и удаление проходят за один проход по файлу, в памяти одна текущая запись.
static void rewrite_records(uint32_t id, const Record* replacement, bool adding) {
    auto file = open_records(PRIMARY_FILE);
    FileWriter output(PRIMARY_FILE);
    output.stream() << SIGNATURE;

    bool found = false;
    Record current;
    while (read_record(file, current)) {
        if (current.id != id) {
            write_record(output.stream(), current);
            continue;
        }
        if (adding)
            throw runtime_error("Запись с таким номером уже есть");
        if (found)
            throw runtime_error("В каталоге повторяется номер записи");
        found = true;
        if (replacement)
            write_record(output.stream(), *replacement);
    }

    if (adding)
        write_record(output.stream(), *replacement);
    else if (!found)
        throw runtime_error("Запись с таким номером не найдена");
    file.close();
    output.save();
}

void add_record(const Record& record) {
    rewrite_records(record.id, &record, true);
}

void edit_record(const Record& record) {
    rewrite_records(record.id, &record, false);
}

void delete_record(uint32_t id) {
    rewrite_records(id, nullptr, false);
}

size_t select_records(const string& image) {
    auto file = open_records(PRIMARY_FILE);
    file_exists(SELECTED_FILE);
    FileWriter output(SELECTED_FILE);
    output.stream() << SIGNATURE;

    size_t count = 0;
    Record record;
    while (read_record(file, record)) {
        if (record.image == image) {
            write_record(output.stream(), record);
            count++;
        }
    }
    output.save();
    return count;
}

vector<Record> load_records(const fs::path& path) {
    auto file = open_records(path);
    vector<Record> records;
    Record record;
    while (read_record(file, record))
        records.push_back(record);
    return records;
}