#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace std;

const char PRIMARY_FILE[] = "records.bin";
const char SELECTED_FILE[] = "selected.bin";
const size_t MAX_TITLE = 80;

struct Record {
    uint32_t id = 0;
    string title;
    string image;
    string payload;
};

bool valid_title(const string& title);
void create_records();
bool find_record(uint32_t id, Record& result);
void add_record(const Record& record);
void edit_record(const Record& record);
void delete_record(uint32_t id);
size_t select_records(const string& image);
vector<Record> load_records(const filesystem::path& path = PRIMARY_FILE);