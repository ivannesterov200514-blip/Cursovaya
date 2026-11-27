#pragma once
#include <cstdint>
#include <istream>

using namespace std;

constexpr uint64_t STEGO_HEADER_SIZE = 8;

struct Bmp {
    uint32_t width;
    uint32_t height;
    bool top_down;
    uint64_t file_size;
    uint64_t pixel_offset;
    uint64_t row_bytes;
    uint64_t row_stride;
};

Bmp read_bmp(istream& file);
uint64_t available_bytes(const Bmp& image);