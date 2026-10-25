#include "BMPwork.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {
const std::uint32_t BMP_HEADER_SIZE = 54;
const std::uint32_t DIB_HEADER_SIZE = 40;
const std::streamoff COLORS_USED_OFFSET = 46;
const std::uint16_t BITS_PER_PIXEL = 24;
const long double MAX_COLOR_VALUE = 255.0L;
}  // namespace

Image ReadBMP(const std::string& inppath) {
    std::filesystem::path path = inppath;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Не удалось открыть файл");
    }

    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.seekg(0, std::ios::end);
    std::streamoff actualsize = file.tellg();
    if (actualsize < BMP_HEADER_SIZE) {
        throw std::runtime_error("Файл слишком короткий для BMP");
    }
    file.seekg(0, std::ios::beg);
    char signature[2];
    std::uint32_t filesize = 0;
    std::uint32_t reserved = 0;
    std::uint32_t pixeloffset = 0;
    std::uint32_t headersize = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint16_t planes = 0;
    std::uint16_t bitcount = 0;
    std::uint32_t compression = 0;
    std::uint32_t imagesize = 0;
    std::uint32_t colorsused = 0;

    file.read(signature, 2);
    file.read(reinterpret_cast<char*>(&filesize), 4);
    file.read(reinterpret_cast<char*>(&reserved), 4);
    file.read(reinterpret_cast<char*>(&pixeloffset), 4);
    file.read(reinterpret_cast<char*>(&headersize), 4);
    file.read(reinterpret_cast<char*>(&width), 4);
    file.read(reinterpret_cast<char*>(&height), 4);
    file.read(reinterpret_cast<char*>(&planes), 2);
    file.read(reinterpret_cast<char*>(&bitcount), 2);
    file.read(reinterpret_cast<char*>(&compression), 4);
    file.read(reinterpret_cast<char*>(&imagesize), 4);
    file.seekg(COLORS_USED_OFFSET, std::ios::beg);
    file.read(reinterpret_cast<char*>(&colorsused), 4);

    if (signature[0] != 'B' || signature[1] != 'M') {
        throw std::runtime_error("Файл не является BMP");
    }
    if (headersize != DIB_HEADER_SIZE || planes != 1 || bitcount != BITS_PER_PIXEL || compression != 0 ||
        colorsused != 0) {
        throw std::runtime_error("Нужен 24-битный BMP без сжатия и таблицы цветов");
    }
    if (reserved != 0 || filesize != static_cast<std::uint64_t>(actualsize)) {
        throw std::runtime_error("Некорректный заголовок BMP");
    }
    if (width <= 0 || height == 0 || height == std::numeric_limits<std::int32_t>::min()) {
        throw std::runtime_error("Некорректные размеры изображения");
    }

    bool bottomup = height > 0;
    if (height < 0) {
        height = -height;
    }
    std::uint64_t rowbytes = 3ULL * width;
    int padding = static_cast<int>((4 - rowbytes % 4) % 4);
    std::uint64_t datasize = (rowbytes + padding) * height;
    if (pixeloffset < BMP_HEADER_SIZE || pixeloffset + datasize > filesize) {
        throw std::runtime_error("В файле не хватает данных пикселей");
    }
    if (imagesize != 0 && imagesize != datasize) {
        throw std::runtime_error("Некорректный размер данных пикселей");
    }

    Channel channelred = Channel(width, height);
    Channel channelgreen = Channel(width, height);
    Channel channelblue = Channel(width, height);

    file.seekg(pixeloffset, std::ios::beg);
    char paddingbytes[3];
    for (int i = 0; i < height; ++i) {
        int y = bottomup ? height - 1 - i : i;
        for (int j = 0; j < width; ++j) {
            unsigned char pixel[3];
            file.read(reinterpret_cast<char*>(pixel), 3);
            channelblue[j][y] = pixel[0] / MAX_COLOR_VALUE;
            channelgreen[j][y] = pixel[1] / MAX_COLOR_VALUE;
            channelred[j][y] = pixel[2] / MAX_COLOR_VALUE;
        }
        file.read(paddingbytes, padding);
    }

    Image ret = Image(width, height, channelred, channelgreen, channelblue);
    file.close();
    return ret;
}

void WriteBMP(const std::string& outpath, const Image& output) {
    std::int32_t width = output.GetWidth();
    std::int32_t height = output.GetHeight();
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Нельзя сохранить изображение с такими размерами");
    }
    std::uint64_t rowbytes = 3ULL * width;
    int padding = static_cast<int>((4 - rowbytes % 4) % 4);
    std::uint64_t datasize = (rowbytes + padding) * height;
    if (datasize > std::numeric_limits<std::uint32_t>::max() - BMP_HEADER_SIZE) {
        throw std::runtime_error("Изображение слишком большое для BMP");
    }

    std::filesystem::path path = outpath;
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Не удалось создать выходной файл");
    }
    file.exceptions(std::ios::failbit | std::ios::badbit);
    char signature[2] = {'B', 'M'};
    std::uint32_t filesize = static_cast<std::uint32_t>(BMP_HEADER_SIZE + datasize);
    std::uint32_t reserved = 0;
    std::uint32_t pixeloffset = BMP_HEADER_SIZE;
    std::uint32_t headersize = DIB_HEADER_SIZE;
    std::uint16_t planes = 1;
    std::uint16_t bitcount = BITS_PER_PIXEL;
    std::uint32_t compression = 0;
    std::uint32_t imagesize = static_cast<std::uint32_t>(datasize);
    std::int32_t resolution = 0;
    std::uint32_t colorsused = 0;
    std::uint32_t importantcolors = 0;

    file.write(signature, 2);
    file.write(reinterpret_cast<const char*>(&filesize), 4);
    file.write(reinterpret_cast<const char*>(&reserved), 4);
    file.write(reinterpret_cast<const char*>(&pixeloffset), 4);
    file.write(reinterpret_cast<const char*>(&headersize), 4);
    file.write(reinterpret_cast<const char*>(&width), 4);
    file.write(reinterpret_cast<const char*>(&height), 4);
    file.write(reinterpret_cast<const char*>(&planes), 2);
    file.write(reinterpret_cast<const char*>(&bitcount), 2);
    file.write(reinterpret_cast<const char*>(&compression), 4);
    file.write(reinterpret_cast<const char*>(&imagesize), 4);
    file.write(reinterpret_cast<const char*>(&resolution), 4);
    file.write(reinterpret_cast<const char*>(&resolution), 4);
    file.write(reinterpret_cast<const char*>(&colorsused), 4);
    file.write(reinterpret_cast<const char*>(&importantcolors), 4);

    const Channel& channelred = output.GetRed();
    const Channel& channelgreen = output.GetGreen();
    const Channel& channelblue = output.GetBlue();
    char paddingbytes[3] = {};

    for (int i = height - 1; i >= 0; --i) {
        for (int j = 0; j < width; ++j) {
            long double red = channelred[j][i];
            long double green = channelgreen[j][i];
            long double blue = channelblue[j][i];
            if (!(0 <= red && red <= 1 && 0 <= green && green <= 1 && 0 <= blue && blue <= 1)) {
                throw std::runtime_error("Некорректный цвет пикселя");
            }
            unsigned char pixel[3] = {static_cast<unsigned char>(std::lround(blue * MAX_COLOR_VALUE)),
                                      static_cast<unsigned char>(std::lround(green * MAX_COLOR_VALUE)),
                                      static_cast<unsigned char>(std::lround(red * MAX_COLOR_VALUE))};
            file.write(reinterpret_cast<const char*>(pixel), 3);
        }
        file.write(paddingbytes, padding);
    }
    file.close();
}
