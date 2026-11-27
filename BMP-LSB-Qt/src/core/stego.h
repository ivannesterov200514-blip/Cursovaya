#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

using namespace std;

struct ExtractedData {
    uint64_t size;
    string preview;
};

uint64_t hide_data(const filesystem::path& image, const filesystem::path& data_file, const filesystem::path& output);
uint64_t hide_text(const filesystem::path& image, const string& text, const filesystem::path& output);
ExtractedData extract_data(const filesystem::path& image, const filesystem::path& output, size_t preview_limit = 0);