#include "Image.h"

#include <limits>
#include <stdexcept>

Channel::Channel() = default;

Channel::Channel(int w_, int h_) {
    if (w_ < 0 || h_ < 0) {
        throw std::invalid_argument("Размеры цветового канала не могут быть отрицательными");
    }
    v_.assign(w_, std::vector<ld>(h_));
    width_ = w_;
    height_ = h_;
}

Channel::Channel(const std::vector<std::vector<ld>>& v) {
    if (v.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::out_of_range("Ширина цветового канала превышает допустимый диапазон int");
    }
    width_ = static_cast<int>(v.size());
    if (width_ > 0) {
        if (v[0].size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::out_of_range("Высота цветового канала превышает допустимый диапазон int");
        }
        for (int i = 1; i < width_; ++i) {
            if (v[i].size() != v[0].size()) {
                throw std::invalid_argument("Некорректный цветовой канал");
            }
        }
        height_ = static_cast<int>(v[0].size());
    }
    v_ = v;
}

std::span<Channel::ld> Channel::operator[](std::size_t i) {
    return v_[i];
}

std::span<const Channel::ld> Channel::operator[](std::size_t i) const {
    return v_[i];
}

int Channel::GetWidth() const {
    return width_;
}

int Channel::GetHeight() const {
    return height_;
}

std::size_t Channel::size() const {
    return v_.size();
}

const Channel& Image::GetRed() const {
    return pixels_red_;
}

const Channel& Image::GetGreen() const {
    return pixels_green_;
}

const Channel& Image::GetBlue() const {
    return pixels_blue_;
}

int Image::GetWidth() const {
    return width_;
}

int Image::GetHeight() const {
    return height_;
}

Image::Image(std::size_t width, std::size_t height, const Channel& pxl_red, const Channel& pxl_green,
             const Channel& pxl_blue) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("Ширина и высота изображения должны быть положительными");
    }
    if (width > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::out_of_range("Размеры изображения превышают допустимый диапазон int");
    }
    width_ = static_cast<int>(width);
    height_ = static_cast<int>(height);
    bool hcorrect =
        (pxl_red.GetHeight() == height_ && pxl_green.GetHeight() == height_ && pxl_blue.GetHeight() == height_);
    bool wcorrect = (pxl_red.GetWidth() == width_ && pxl_green.GetWidth() == width_ && pxl_blue.GetWidth() == width_);
    if (!hcorrect || !wcorrect) {
        throw std::invalid_argument("Размеры цветовых каналов не совпадают с размерами изображения");
    }
    pixels_red_ = pxl_red;
    pixels_blue_ = pxl_blue;
    pixels_green_ = pxl_green;
}
