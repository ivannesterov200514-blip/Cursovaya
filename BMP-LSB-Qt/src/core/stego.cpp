#include "stego.h"
#include "bmp.h"
#include "files.h"
#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

static constexpr array<uint8_t, 4> SIGNATURE = {'S', 'T', 'G', '1'};
static constexpr size_t BLOCK_SIZE = 64 * 1024;
using Block = array<char, BLOCK_SIZE>;

static void read_block(istream& input, char* data, size_t count) {
    if (!input.read(data, static_cast<streamsize>(count)))
        throw runtime_error("Ошибка чтения BMP");
}

static void write_block(ostream& output, const char* data, size_t count) {
    if (!output.write(data, static_cast<streamsize>(count)))
        throw runtime_error("Ошибка записи результата. Проверьте доступное место на диске");
}

static void copy_bytes(istream& input, ostream& output, uint64_t length) {
    Block buffer;
    while (length != 0) {
        size_t count = static_cast<size_t>(min<uint64_t>(length, buffer.size()));
        read_block(input, buffer.data(), count);
        write_block(output, buffer.data(), count);
        length -= count;
    }
}

class PixelStream {
public:
    PixelStream(istream& input, const Bmp& bmp, ostream* output = nullptr)
        : input(input), output(output), remaining(bmp.file_size - bmp.pixel_offset), row_bytes(bmp.row_bytes), padding(bmp.row_stride - bmp.row_bytes),
          channels_left(bmp.row_bytes) {}

    char& next_channel() {
        if (channels_left == 0) {
            for (uint64_t i = 0; i < padding; i++)
                next_byte();
            channels_left = row_bytes;
        }
        channels_left--;
        return next_byte();
    }

    void copy_remaining() {
        write_block(*output, buffer.data(), loaded);
        copy_bytes(input, *output, remaining);
    }

private:
    char& next_byte() {
        if (position == loaded) {
            if (output && loaded != 0)
                write_block(*output, buffer.data(), loaded);
            loaded = static_cast<size_t>(min<uint64_t>(remaining, buffer.size()));
            if (loaded == 0)
                throw runtime_error("Пиксельные данные BMP обрезаны");
            read_block(input, buffer.data(), loaded);
            remaining -= loaded;
            position = 0;
        }
        return buffer[position++];
    }

    istream& input;
    ostream* output;
    uint64_t remaining;
    uint64_t row_bytes;
    uint64_t padding;
    uint64_t channels_left;
    Block buffer;
    size_t position = 0;
    size_t loaded = 0;
};

static void write_byte(PixelStream& pixels, uint8_t value) {
    for (int shift = 7; shift >= 0; shift--) {
        char& channel = pixels.next_channel();
        unsigned char color = static_cast<unsigned char>(channel);
        unsigned char bit = (value >> shift) & 1u;
        color = (color & 0xfeu) | bit;
        channel = static_cast<char>(color);
    }
}

static uint8_t read_byte(PixelStream& pixels) {
    uint8_t value = 0;
    for (int i = 0; i < 8; i++) {
        unsigned char color = static_cast<unsigned char>(pixels.next_channel());
        unsigned char bit = color & 1u;
        value = static_cast<uint8_t>((value << 1) | bit);
    }
    return value;
}

static uint64_t embed_data(const fs::path& image, istream& payload, uint64_t length, const fs::path& output) {
    ifstream input(image, ios::binary);
    const Bmp bmp = read_bmp(input);
    uint64_t available = available_bytes(bmp);
    if (available < STEGO_HEADER_SIZE)
        throw runtime_error("В BMP не помещается даже заголовок сообщения");
    if (length > available - STEGO_HEADER_SIZE)
        throw runtime_error("Данные не помещаются в этот BMP");

    FileWriter result(output);
    input.seekg(0);
    copy_bytes(input, result.stream(), bmp.pixel_offset);
    PixelStream pixels(input, bmp, &result.stream());
    for (uint8_t byte : SIGNATURE)
        write_byte(pixels, byte);
    for (int i = 0; i < 4; i++)
        write_byte(pixels, static_cast<uint8_t>(length >> (i * 8)));
    for (uint64_t i = 0; i < length; i++) {
        int byte = payload.get();
        if (byte == char_traits<char>::eof())
            throw runtime_error("Ошибка чтения файла данных");
        write_byte(pixels, static_cast<uint8_t>(byte));
    }
    if (payload.peek() != char_traits<char>::eof() || payload.bad())
        throw runtime_error("Файл данных изменился или недоступен для чтения");

    pixels.copy_remaining();
    if (input.peek() != char_traits<char>::eof() || input.bad())
        throw runtime_error("BMP изменился или недоступен для чтения");
    result.save(false);
    return length;
}

uint64_t hide_data(const fs::path& image, const fs::path& data_file, const fs::path& output) {
    ifstream payload(data_file, ios::binary | ios::ate);
    if (!payload)
        throw runtime_error("Не удалось открыть файл данных");
    streamoff length = payload.tellg();
    if (length < 0)
        throw runtime_error("Не удалось определить размер файла данных");
    payload.seekg(0);
    if (!payload)
        throw runtime_error("Ошибка чтения файла данных");
    return embed_data(image, payload, static_cast<uint64_t>(length), output);
}

uint64_t hide_text(const fs::path& image, const string& text, const fs::path& output) {
    istringstream payload(text);
    return embed_data(image, payload, text.size(), output);
}

ExtractedData extract_data(const fs::path& image, const fs::path& output, size_t preview_limit) {
    ifstream input(image, ios::binary);
    const Bmp bmp = read_bmp(input);
    uint64_t available = available_bytes(bmp);
    if (available < STEGO_HEADER_SIZE)
        throw runtime_error("BMP слишком мал для заголовка сообщения");

    input.seekg(static_cast<streamoff>(bmp.pixel_offset));
    PixelStream pixels(input, bmp);
    for (uint8_t byte : SIGNATURE) {
        if (read_byte(pixels) != byte)
            throw runtime_error("В BMP не найдена метка скрытых данных STG1");
    }

    uint32_t length = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t byte = read_byte(pixels);
        length |= byte << (i * 8);
    }
    if (length > available - STEGO_HEADER_SIZE)
        throw runtime_error("Длина скрытых данных превышает вместимость BMP");

    FileWriter result(output);
    ExtractedData data{length, {}};
    uint64_t preview_size = min<uint64_t>(length, preview_limit);
    data.preview.reserve(static_cast<size_t>(preview_size));
    Block buffer;
    uint64_t written = 0;
    while (written < length) {
        size_t count = static_cast<size_t>(min<uint64_t>(length - written, buffer.size()));
        for (size_t i = 0; i < count; i++)
            buffer[i] = static_cast<char>(read_byte(pixels));
        write_block(result.stream(), buffer.data(), count);
        if (written < preview_size) {
            size_t preview_count = static_cast<size_t>(min<uint64_t>(preview_size - written, count));
            data.preview.append(buffer.data(), preview_count);
        }
        written += count;
    }
    result.save(false);
    return data;
}