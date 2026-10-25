#pragma once

#include <cstddef>
#include <span>
#include <vector>

struct Channel {
    using ld = long double;
    Channel();
    Channel(int w_, int h_);
    explicit Channel(const std::vector<std::vector<ld>>& v);
    std::span<ld> operator[](std::size_t i);
    std::span<const ld> operator[](std::size_t i) const;
    int GetWidth() const;
    int GetHeight() const;
    std::size_t size() const;

private:
    std::vector<std::vector<ld>> v_;
    int width_ = 0;
    int height_ = 0;
};

struct Image {
    const Channel& GetRed() const;
    const Channel& GetGreen() const;
    const Channel& GetBlue() const;
    int GetWidth() const;
    int GetHeight() const;
    Image(std::size_t width, std::size_t height, const Channel& pxl_red, const Channel& pxl_green,
          const Channel& pxl_blue);

private:
    int width_;
    int height_;
    Channel pixels_red_;
    Channel pixels_green_;
    Channel pixels_blue_;
};
