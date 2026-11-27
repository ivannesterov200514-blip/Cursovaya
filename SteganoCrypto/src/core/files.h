#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

using namespace std;

class WorkspaceLock {
public:
    WorkspaceLock();
    ~WorkspaceLock();
    WorkspaceLock(const WorkspaceLock&) = delete;
    WorkspaceLock& operator=(const WorkspaceLock&) = delete;

private:
    int handle;
};

class FileWriter {
public:
    explicit FileWriter(const filesystem::path& path);
    ~FileWriter();
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;

    ofstream& stream();
    void save(bool replace = true);

private:
    filesystem::path destination;
    filesystem::path temp_dir;
    ofstream file;
};

bool file_exists(const filesystem::path& path);
bool valid_file_name(const string& name);
filesystem::path user_file(const string& name, bool output);
void write_number(ostream& file, uint32_t value);
uint32_t read_number(istream& file);