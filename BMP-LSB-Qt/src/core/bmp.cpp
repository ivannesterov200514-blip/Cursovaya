#include "bmp.h"

#include <array>
#include <limits>
#include <stdexcept>

using namespace std;

static uint32_t read_le(const array<unsigned char, 54>& bytes, size_t offset, int count) {
    uint32_t value = 0;
    for (int i = 0; i < count; i++) {
        uint32_t byte = bytes[offset + i];
        value |= byte << (8 * i);
    }
    return value;
}

Bmp read_bmp(istream& file) {
    if (!file)
        throw runtime_error("Не удалось открыть BMP");
    file.seekg(0, ios::end);
    streamoff length = file.tellg();
    if (length > static_cast<streamoff>(numeric_limits<uint32_t>::max()))
        throw runtime_error("Размер BMP превышает предел формата: 4 ГиБ минус 1 байт");

    array<unsigned char, 54> bytes{};
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()) ||
        bytes[0] != 'B' || bytes[1] != 'M')
        throw runtime_error("Неверный заголовок BMP");

    uint32_t file_size = read_le(bytes, 2, 4);
    uint32_t offset = read_le(bytes, 10, 4);
    uint32_t dib_size = read_le(bytes, 14, 4);
    if (file_size != length || dib_size < 40 || offset < 14 ||
        offset > file_size || dib_size > offset - 14)
        throw runtime_error("Неверный размер или смещение данных BMP");

    if (read_le(bytes, 26, 2) != 1 || read_le(bytes, 28, 2) != 24 ||
        read_le(bytes, 30, 4) != 0)
        throw runtime_error("Поддерживается только 24-битный BMP без сжатия");

    uint32_t width = read_le(bytes, 18, 4);
    uint32_t raw_height = read_le(bytes, 22, 4);
    if (width == 0 || width > 0x7fffffffu || raw_height == 0)
        throw runtime_error("Недопустимый размер изображения");

    bool top_down = raw_height > 0x7fffffffu;
    uint32_t height = raw_height;
    if (top_down)
        height = 0u - raw_height;
    uint64_t row_bytes = static_cast<uint64_t>(width) * 3;
    uint64_t row_stride = (row_bytes + 3) / 4 * 4;
    if (row_stride > (file_size - offset) / height)
        throw runtime_error("Пиксельные данные BMP обрезаны или размеры неверны");

    return {width, height, top_down, file_size, offset, row_bytes, row_stride};
}

uint64_t available_bytes(const Bmp& image) {
    return image.row_bytes * image.height / 8;
}