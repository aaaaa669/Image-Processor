#include "BMPwork.h"
#include "Filters.h"

#include <catch.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
using Color = std::array<long double, 3>;
using Colors = std::vector<std::vector<Color>>;

const std::size_t BMP_HEADER_SIZE = 54;
const std::uint32_t DIB_HEADER_SIZE = 40;
const std::size_t BITS_PER_BYTE = 8;
const long double COLOR_MAX = 255.0L;

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        std::random_device generator;
        do {
            path_ = std::filesystem::temp_directory_path() / ("image_processor_test_" + std::to_string(generator()));
        } while (!std::filesystem::create_directory(path_));
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;

    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    std::filesystem::path File(const std::string& name) const {
        return path_ / name;
    }

private:
    std::filesystem::path path_;
};

Image MakeImage(const Colors& colors) {
    REQUIRE(!colors.empty());
    REQUIRE(!colors.front().empty());
    int width = static_cast<int>(colors.front().size());
    int height = static_cast<int>(colors.size());
    Channel red(width, height);
    Channel green(width, height);
    Channel blue(width, height);
    for (int y = 0; y < height; ++y) {
        REQUIRE(colors[y].size() == static_cast<std::size_t>(width));
        for (int x = 0; x < width; ++x) {
            red[x][y] = colors[y][x][0];
            green[x][y] = colors[y][x][1];
            blue[x][y] = colors[y][x][2];
        }
    }
    return Image(width, height, red, green, blue);
}

Image MakeSolidImage(int width, int height, const Color& color) {
    return MakeImage(Colors(height, std::vector<Color>(width, color)));
}

void CheckColor(const Image& image, int x, int y, const Color& color, double tolerance = 1e-12) {
    CHECK(image.GetRed()[x][y] == Approx(static_cast<double>(color[0])).epsilon(1e-12).margin(tolerance));
    CHECK(image.GetGreen()[x][y] == Approx(static_cast<double>(color[1])).epsilon(1e-12).margin(tolerance));
    CHECK(image.GetBlue()[x][y] == Approx(static_cast<double>(color[2])).epsilon(1e-12).margin(tolerance));
}

void CheckImage(const Image& image, const Colors& colors, double tolerance = 1e-12) {
    REQUIRE(image.GetHeight() == static_cast<int>(colors.size()));
    REQUIRE(image.GetWidth() == static_cast<int>(colors.front().size()));
    for (int y = 0; y < image.GetHeight(); ++y) {
        for (int x = 0; x < image.GetWidth(); ++x) {
            CAPTURE(x, y);
            CheckColor(image, x, y, colors[y][x], tolerance);
        }
    }
}

void SetUInt32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < sizeof(value); ++i) {
        bytes.at(offset + i) = static_cast<unsigned char>(value >> (BITS_PER_BYTE * i));
    }
}

std::uint32_t GetUInt32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < sizeof(value); ++i) {
        value |= static_cast<std::uint32_t>(bytes.at(offset + i)) << (BITS_PER_BYTE * i);
    }
    return value;
}

std::vector<unsigned char> MakeBmp(const Colors& colors, bool top_down = false, std::size_t gap = 0) {
    REQUIRE(!colors.empty());
    REQUIRE(!colors.front().empty());
    std::size_t width = colors.front().size();
    std::size_t height = colors.size();
    std::size_t stride = (width * 3 + 3) / 4 * 4;
    std::size_t offset = BMP_HEADER_SIZE + gap;
    std::vector<unsigned char> bytes(offset + stride * height);
    bytes[0] = 'B';
    bytes[1] = 'M';
    SetUInt32(bytes, 2, static_cast<std::uint32_t>(bytes.size()));
    SetUInt32(bytes, 10, static_cast<std::uint32_t>(offset));
    SetUInt32(bytes, 14, DIB_HEADER_SIZE);
    SetUInt32(bytes, 18, static_cast<std::uint32_t>(width));
    std::int32_t signed_height = static_cast<std::int32_t>(height);
    if (top_down) {
        signed_height = -signed_height;
    }
    SetUInt32(bytes, 22, static_cast<std::uint32_t>(signed_height));
    bytes[26] = 1;
    bytes[28] = 24;
    SetUInt32(bytes, 34, static_cast<std::uint32_t>(stride * height));
    for (std::size_t y = 0; y < height; ++y) {
        std::size_t source_y = top_down ? y : height - 1 - y;
        for (std::size_t x = 0; x < width; ++x) {
            for (std::size_t channel = 0; channel < 3; ++channel) {
                bytes[offset + y * stride + 3 * x + channel] =
                    static_cast<unsigned char>(std::lround(colors[source_y][x][2 - channel] * COLOR_MAX));
            }
        }
        for (std::size_t padding = 3 * width; padding < stride; ++padding) {
            bytes[offset + y * stride + padding] = 0xa5;
        }
    }
    return bytes;
}

void WriteBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary);
    REQUIRE(file.is_open());
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
}

std::vector<unsigned char> ReadBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.is_open());
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
}  // namespace

TEST_CASE("Channel initializes dimensions and values", "[image]") {
    const Channel empty;
    CHECK(empty.GetWidth() == 0);
    CHECK(empty.GetHeight() == 0);
    CHECK(empty.size() == 0);
    const Channel channel(2, 3);
    CHECK(channel.GetWidth() == 2);
    CHECK(channel.GetHeight() == 3);
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 3; ++y) {
            CHECK(channel[x][y] == 0);
        }
    }
    CHECK_THROWS_AS(Channel(-1, 1), std::invalid_argument);
    CHECK_THROWS_AS(Channel(1, -1), std::invalid_argument);
}

TEST_CASE("Channel validates rectangular storage and owns its values", "[image]") {
    std::vector<std::vector<Channel::ld>> values = {{0.25L, 0.5L}, {0.75L, 1.0L}};
    const Channel channel(values);
    CHECK(channel.GetWidth() == 2);
    CHECK(channel.GetHeight() == 2);
    CHECK(channel[1][0] == 0.75L);
    values[1][0] = 0;
    CHECK(channel[1][0] == 0.75L);
    CHECK_THROWS_AS(Channel(std::vector<std::vector<Channel::ld>>{{0}, {0, 1}}), std::invalid_argument);
    const Channel empty(std::vector<std::vector<Channel::ld>>{});
    CHECK(empty.size() == 0);
    CHECK(empty.GetHeight() == 0);
}

TEST_CASE("Image keeps dimensions and distinct color channels", "[image]") {
    CheckImage(MakeImage({{{1, 0, 0}, {0, 1, 0}}, {{0, 0, 1}, {0.25L, 0.5L, 0.75L}}}),
               {{{1, 0, 0}, {0, 1, 0}}, {{0, 0, 1}, {0.25L, 0.5L, 0.75L}}});
}

TEST_CASE("Image rejects empty and inconsistent dimensions", "[image]") {
    Channel channel(1, 1);
    CHECK_THROWS_AS(Image(0, 1, channel, channel, channel), std::invalid_argument);
    CHECK_THROWS_AS(Image(1, 0, channel, channel, channel), std::invalid_argument);
    CHECK_THROWS_AS(Image(2, 1, channel, channel, channel), std::invalid_argument);
    CHECK_THROWS_AS(Image(1, 2, channel, channel, channel), std::invalid_argument);
    CHECK_THROWS_AS(Image(1, 1, Channel(2, 1), channel, channel), std::invalid_argument);
    CHECK_THROWS_AS(Image(1, 1, channel, Channel(1, 2), channel), std::invalid_argument);
    CHECK_THROWS_AS(Image(1, 1, channel, channel, Channel(0, 0)), std::invalid_argument);
}

TEST_CASE("Image rejects dimensions before narrowing them to int", "[image][regression]") {
    Channel channel(1, 1);
    std::vector<std::size_t> dimensions = {
        static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1,
        std::numeric_limits<std::size_t>::max(),
    };
    if (std::numeric_limits<std::size_t>::max() > std::numeric_limits<std::uint32_t>::max()) {
        dimensions.push_back(static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 2);
    }
    for (std::size_t dimension : dimensions) {
        CAPTURE(dimension);
        CHECK_THROWS_AS(Image(dimension, 1, channel, channel, channel), std::out_of_range);
        CHECK_THROWS_AS(Image(1, dimension, channel, channel, channel), std::out_of_range);
    }
}

TEST_CASE("Negative inverts all channels without changing the input", "[filters]") {
    const Colors colors = {{{0, 0.25L, 1}, {0.5L, 0.75L, 0.25L}}};
    const Image image = MakeImage(colors);
    Negative filter;
    CheckImage(filter.filter(image), {{{1, 0.75L, 0}, {0.5L, 0.25L, 0.75L}}});
    CheckImage(image, colors);
}

TEST_CASE("Grayscale uses each color weight and returns equal channels", "[filters]") {
    const Image image = MakeImage({{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}}});
    Grayscale filter;
    CheckImage(filter.filter(image),
               {{{0.299L, 0.299L, 0.299L}, {0.587L, 0.587L, 0.587L}, {0.114L, 0.114L, 0.114L}, {1, 1, 1}}});
}

TEST_CASE("Crop takes the upper left part and limits oversized requests", "[filters]") {
    const Colors colors = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {{0, 1, 1}, {1, 0, 1}, {1, 1, 0}}};
    const Image image = MakeImage(colors);
    CheckImage(Crop(2, 1).filter(image), {{{1, 0, 0}, {0, 1, 0}}});
    CheckImage(Crop(1, 100).filter(image), {{{1, 0, 0}}, {{0, 1, 1}}});
    CheckImage(Crop(100, 100).filter(image), colors);
    CheckImage(image, colors);
}

TEST_CASE("Crop rejects nonpositive dimensions", "[filters][errors]") {
    CHECK_THROWS_AS(Crop(0, 1), std::invalid_argument);
    CHECK_THROWS_AS(Crop(1, 0), std::invalid_argument);
    CHECK_THROWS_AS(Crop(-1, 1), std::invalid_argument);
    CHECK_THROWS_AS(Crop(1, -1), std::invalid_argument);
}

TEST_CASE("Sharpening clamps colors and repeats the nearest border pixel", "[filters]") {
    const Image image = MakeImage({{{0.1L, 0.3L, 0.7L}, {0.2L, 0.3L, 0.7L}, {0.3L, 0.3L, 0.7L}},
                                   {{0.4L, 0.3L, 0.7L}, {0.8L, 0.3L, 0.7L}, {0.5L, 0.3L, 0.7L}},
                                   {{0.6L, 0.3L, 0.7L}, {0.7L, 0.3L, 0.7L}, {0.9L, 0.3L, 0.7L}}});
    Sharpening filter;
    const Image result = filter.filter(image);
    CheckColor(result, 0, 0, {0, 0.3L, 0.7L});
    CheckColor(result, 1, 1, {1, 0.3L, 0.7L});
    CheckColor(result, 2, 0, {0.2L, 0.3L, 0.7L});
    CheckColor(result, 1, 2, {0.5L, 0.3L, 0.7L});
}

TEST_CASE("Sharpening keeps a single pixel unchanged", "[filters][small]") {
    const Image image = MakeSolidImage(1, 1, {0.25L, 0.5L, 0.75L});
    Sharpening filter;
    CheckImage(filter.filter(image), {{{0.25L, 0.5L, 0.75L}}});
}

TEST_CASE("Edge detection converts color to grayscale before thresholding", "[filters]") {
    const Image image = MakeImage(
        {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, {{0, 0, 0}, {0, 0, 1}, {0, 0, 0}}, {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}});
    const Image below = EdgeDetection(0.5L).filter(image);
    CheckImage(below, Colors(3, std::vector<Color>(3, {0, 0, 0})));
    const Image above = EdgeDetection(0.4L).filter(image);
    CheckColor(above, 1, 1, {1, 1, 1});
    CheckColor(above, 0, 1, {0, 0, 0});
    CheckColor(above, 1, 0, {0, 0, 0});
}

TEST_CASE("Edge threshold comparison is strict", "[filters]") {
    const Image image = MakeImage({{{0, 0, 0}, {1, 1, 1}, {0, 0, 0}}});
    CheckImage(EdgeDetection(1).filter(image), {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}});
    CheckImage(EdgeDetection(0).filter(MakeSolidImage(1, 1, {0, 0, 0})), {{{0, 0, 0}}});
}

TEST_CASE("Edge detection rejects nonfinite thresholds", "[filters][errors]") {
    CHECK_THROWS_AS(EdgeDetection(std::numeric_limits<long double>::quiet_NaN()), std::invalid_argument);
    CHECK_THROWS_AS(EdgeDetection(std::numeric_limits<long double>::infinity()), std::invalid_argument);
    CHECK_THROWS_AS(EdgeDetection(-std::numeric_limits<long double>::infinity()), std::invalid_argument);
}

TEST_CASE("Gaussian blur preserves constant images including narrow ones", "[filters][small]") {
    for (int width : {1, 3}) {
        for (int height : {1, 2}) {
            for (long double sigma : {0.1L, 0.5L, 3.0L, 10.0L}) {
                CAPTURE(width, height, sigma);
                const Color color = {0.25L, 0.5L, 0.75L};
                const Image image = MakeSolidImage(width, height, color);
                CheckImage(GaussianBlur(sigma).filter(image), Colors(height, std::vector<Color>(width, color)));
            }
        }
    }
}

TEST_CASE("Gaussian blur agrees with Gaussian impulse weights in both directions", "[filters]") {
    // Infinite Gaussian weights at sigma=1; the finite kernel introduces a small approximation error.
    const long double CENTER_WEIGHT = 0.3989422804014327L;
    const long double NEIGHBOR_WEIGHT = 0.2419707245191434L;
    const Image horizontal = MakeImage({{{0, 0, 0}, {1, 0.5L, 0.25L}, {0, 0, 0}}});
    const Image vertical = MakeImage({{{0, 0, 0}}, {{1, 0.5L, 0.25L}}, {{0, 0, 0}}});
    GaussianBlur filter(1);
    const Image x_result = filter.filter(horizontal);
    const Image y_result = filter.filter(vertical);
    CheckColor(x_result, 1, 0, {CENTER_WEIGHT, CENTER_WEIGHT / 2, CENTER_WEIGHT / 4}, 1e-5);
    CheckColor(x_result, 0, 0, {NEIGHBOR_WEIGHT, NEIGHBOR_WEIGHT / 2, NEIGHBOR_WEIGHT / 4}, 1e-5);
    CheckColor(x_result, 2, 0, {NEIGHBOR_WEIGHT, NEIGHBOR_WEIGHT / 2, NEIGHBOR_WEIGHT / 4}, 1e-5);
    CheckColor(y_result, 0, 1, {CENTER_WEIGHT, CENTER_WEIGHT / 2, CENTER_WEIGHT / 4}, 1e-5);
    CheckColor(y_result, 0, 0, {NEIGHBOR_WEIGHT, NEIGHBOR_WEIGHT / 2, NEIGHBOR_WEIGHT / 4}, 1e-5);
    CheckColor(y_result, 0, 2, {NEIGHBOR_WEIGHT, NEIGHBOR_WEIGHT / 2, NEIGHBOR_WEIGHT / 4}, 1e-5);
}

TEST_CASE("Gaussian blur applies a two dimensional kernel", "[filters]") {
    const Image image = MakeImage(
        {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, {{0, 0, 0}, {1, 1, 1}, {0, 0, 0}}, {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}});
    const Image result = GaussianBlur(1).filter(image);
    CheckColor(result, 1, 1, {0.1591549431L, 0.1591549431L, 0.1591549431L}, 1e-5);
    CheckColor(result, 0, 1, {0.0965323526L, 0.0965323526L, 0.0965323526L}, 1e-5);
    CheckColor(result, 0, 0, {0.0585498315L, 0.0585498315L, 0.0585498315L}, 1e-5);
}

TEST_CASE("Gaussian blur rejects invalid sigma and an unrepresentable radius", "[filters][errors]") {
    CHECK_THROWS_AS(GaussianBlur(0), std::invalid_argument);
    CHECK_THROWS_AS(GaussianBlur(-1), std::invalid_argument);
    CHECK_THROWS_AS(GaussianBlur(std::numeric_limits<long double>::quiet_NaN()), std::invalid_argument);
    CHECK_THROWS_AS(GaussianBlur(std::numeric_limits<long double>::infinity()), std::invalid_argument);
    const Image image = MakeSolidImage(1, 1, {0, 0, 0});
    CHECK_THROWS_AS(GaussianBlur(1e100L).filter(image), std::invalid_argument);
}

TEST_CASE("Crystallization preserves tiny images with one pixel cells", "[filters][small]") {
    for (int width : {1, 10}) {
        for (int height : {1, 10}) {
            CAPTURE(width, height);
            Colors colors(height, std::vector<Color>(width));
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    colors[y][x] = {x / 10.0L, y / 10.0L, (x + y) / 20.0L};
                }
            }
            const Image image = MakeImage(colors);
            Crystalize filter;
            CheckImage(filter.filter(image), colors);
        }
    }
}

TEST_CASE("Crystallization reduces colors using colors from the original image", "[filters]") {
    const int SIZE = 80;
    Colors colors(SIZE, std::vector<Color>(SIZE));
    std::set<Color> original_colors;
    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            colors[y][x] = {x / 80.0L, y / 80.0L, 0.5L};
            original_colors.insert(colors[y][x]);
        }
    }
    const Image image = MakeImage(colors);
    Crystalize filter;
    const Image result = filter.filter(image);
    REQUIRE(result.GetWidth() == SIZE);
    REQUIRE(result.GetHeight() == SIZE);
    std::set<Color> result_colors;
    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            Color color = {result.GetRed()[x][y], result.GetGreen()[x][y], result.GetBlue()[x][y]};
            REQUIRE(original_colors.count(color) == 1);
            result_colors.insert(color);
        }
    }
    CHECK(result_colors.size() > 1);
    CHECK(result_colors.size() < original_colors.size());
    CheckImage(image, colors);
}

TEST_CASE("Filter order changes the result of cropping and blurring", "[filters]") {
    const Image image = MakeImage({{{0, 0, 0}, {1, 1, 1}, {0, 0, 0}}});
    Crop crop(1, 1);
    GaussianBlur blur(1);
    const Image crop_first = blur.filter(crop.filter(image));
    const Image blur_first = crop.filter(blur.filter(image));
    CheckColor(crop_first, 0, 0, {0, 0, 0});
    CHECK(blur_first.GetRed()[0][0] > 0);
}

TEST_CASE("BMP reader supports all padding sizes, both orientations, and a pixel offset", "[bmp]") {
    TemporaryDirectory directory;
    for (int width = 1; width <= 4; ++width) {
        for (bool top_down : {false, true}) {
            CAPTURE(width, top_down);
            Colors colors(2, std::vector<Color>(width));
            for (int x = 0; x < width; ++x) {
                colors[0][x] = {1, x % 2 == 0 ? 0.0L : 1.0L, 0};
                colors[1][x] = {0, x % 2 == 0 ? 1.0L : 0.0L, 1};
            }
            auto bytes = MakeBmp(colors, top_down, 11);
            if (top_down) {
                SetUInt32(bytes, 34, 0);
            }
            WriteBytes(directory.File("input.bmp"), bytes);
            CheckImage(ReadBMP(directory.File("input.bmp").string()), colors);
        }
    }
}

TEST_CASE("BMP reader accepts the minimal one pixel file", "[bmp][small]") {
    TemporaryDirectory directory;
    auto bytes = MakeBmp({{{1, 0, 0}}});
    REQUIRE(bytes.size() == BMP_HEADER_SIZE + 4);
    WriteBytes(directory.File("input.bmp"), bytes);
    CheckImage(ReadBMP(directory.File("input.bmp").string()), {{{1, 0, 0}}});
}

TEST_CASE("BMP reader rejects malformed or unsupported files", "[bmp][errors]") {
    TemporaryDirectory directory;
    auto bytes = MakeBmp({{{1, 0, 0}}});
    SECTION("empty file") {
        bytes.clear();
    }
    SECTION("truncated header") {
        bytes.resize(BMP_HEADER_SIZE - 1);
    }
    SECTION("wrong signature") {
        bytes[0] = 'Z';
    }
    SECTION("wrong declared file size") {
        SetUInt32(bytes, 2, 0);
    }
    SECTION("reserved field") {
        SetUInt32(bytes, 6, 1);
    }
    SECTION("unsupported DIB header") {
        SetUInt32(bytes, 14, 108);
    }
    SECTION("zero width") {
        SetUInt32(bytes, 18, 0);
    }
    SECTION("negative width") {
        SetUInt32(bytes, 18, std::numeric_limits<std::uint32_t>::max());
    }
    SECTION("zero height") {
        SetUInt32(bytes, 22, 0);
    }
    SECTION("minimum signed height") {
        SetUInt32(bytes, 22, static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::min()));
    }
    SECTION("multiple planes") {
        bytes[26] = 2;
    }
    SECTION("wrong pixel depth") {
        bytes[28] = 32;
    }
    SECTION("compression") {
        SetUInt32(bytes, 30, 1);
    }
    SECTION("wrong pixel data size") {
        SetUInt32(bytes, 34, 1);
    }
    SECTION("color table") {
        SetUInt32(bytes, 46, 1);
    }
    SECTION("pixel offset overlaps header") {
        SetUInt32(bytes, 10, BMP_HEADER_SIZE - 1);
    }
    SECTION("pixel offset outside file") {
        SetUInt32(bytes, 10, static_cast<std::uint32_t>(bytes.size() + 1));
    }
    SECTION("missing pixel padding even with an adjusted file size") {
        bytes.pop_back();
        SetUInt32(bytes, 2, static_cast<std::uint32_t>(bytes.size()));
    }
    WriteBytes(directory.File("input.bmp"), bytes);
    CHECK_THROWS_AS(ReadBMP(directory.File("input.bmp").string()), std::runtime_error);
}

TEST_CASE("BMP reader reports a missing input file", "[bmp][errors]") {
    TemporaryDirectory directory;
    CHECK_THROWS_AS(ReadBMP(directory.File("missing.bmp").string()), std::runtime_error);
}

TEST_CASE("BMP writer produces the specified header, BGR order, and zero padding", "[bmp]") {
    TemporaryDirectory directory;
    const Image image = MakeImage({{{1, 0, 0}, {0, 1, 0}}, {{0, 0, 1}, {1, 1, 1}}});
    WriteBMP(directory.File("output.bmp").string(), image);
    const auto bytes = ReadBytes(directory.File("output.bmp"));
    REQUIRE(bytes.size() == BMP_HEADER_SIZE + 16);
    CHECK(bytes[0] == 'B');
    CHECK(bytes[1] == 'M');
    CHECK(GetUInt32(bytes, 2) == bytes.size());
    CHECK(GetUInt32(bytes, 10) == BMP_HEADER_SIZE);
    CHECK(GetUInt32(bytes, 14) == DIB_HEADER_SIZE);
    CHECK(GetUInt32(bytes, 18) == 2);
    CHECK(GetUInt32(bytes, 22) == 2);
    CHECK(bytes[26] == 1);
    CHECK(bytes[28] == 24);
    CHECK(GetUInt32(bytes, 30) == 0);
    CHECK(GetUInt32(bytes, 34) == 16);
    const std::vector<unsigned char> expected = {255, 0, 0, 255, 255, 255, 0, 0, 0, 0, 255, 0, 255, 0, 0, 0};
    CHECK(std::vector<unsigned char>(bytes.begin() + BMP_HEADER_SIZE, bytes.end()) == expected);
}

TEST_CASE("BMP writer rounds normalized colors to bytes", "[bmp]") {
    TemporaryDirectory directory;
    WriteBMP(directory.File("output.bmp").string(), MakeSolidImage(1, 1, {0.25L, 0.5L, 0.75L}));
    const auto bytes = ReadBytes(directory.File("output.bmp"));
    REQUIRE(bytes.size() == BMP_HEADER_SIZE + 4);
    CHECK(bytes[BMP_HEADER_SIZE] == 191);
    CHECK(bytes[BMP_HEADER_SIZE + 1] == 128);
    CHECK(bytes[BMP_HEADER_SIZE + 2] == 64);
    CHECK(bytes[BMP_HEADER_SIZE + 3] == 0);
}

TEST_CASE("BMP pixels survive reading and rewriting nonstandard layouts", "[bmp]") {
    TemporaryDirectory directory;
    const Colors colors = {{{17 / COLOR_MAX, 128 / COLOR_MAX, 241 / COLOR_MAX}}, {{1, 0, 1}}};
    WriteBytes(directory.File("input.bmp"), MakeBmp(colors, true, 7));
    const Image image = ReadBMP(directory.File("input.bmp").string());
    WriteBMP(directory.File("output.bmp").string(), image);
    CheckImage(ReadBMP(directory.File("output.bmp").string()), colors);
}

TEST_CASE("BMP writer rejects invalid colors and an unavailable destination", "[bmp][errors]") {
    TemporaryDirectory directory;
    for (long double value : {-0.1L, 1.1L, std::numeric_limits<long double>::quiet_NaN()}) {
        CHECK_THROWS_AS(WriteBMP(directory.File("output.bmp").string(), MakeSolidImage(1, 1, {value, 0, 0})),
                        std::runtime_error);
        CHECK_THROWS_AS(WriteBMP(directory.File("output.bmp").string(), MakeSolidImage(1, 1, {0, value, 0})),
                        std::runtime_error);
        CHECK_THROWS_AS(WriteBMP(directory.File("output.bmp").string(), MakeSolidImage(1, 1, {0, 0, value})),
                        std::runtime_error);
    }
    CHECK_THROWS_AS(WriteBMP(directory.File("missing/output.bmp").string(), MakeSolidImage(1, 1, {0, 0, 0})),
                    std::runtime_error);
}
