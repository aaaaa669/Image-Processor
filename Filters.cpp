#include "Filters.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
const int RANDOM_SEED = 67;
const int SHARPENING_CENTER_WEIGHT = 5;
const long double GAUSSIAN_EXPONENT_FACTOR = 0.5L;
const double CRYSTAL_TARGET_COUNT = 1000.0;
const std::int64_t INITIAL_DISTANCE = 1'000'000'000'000LL;
}

Filter::~Filter() = default;

long double Filter::ClampColor(long double x) {
    return std::max(static_cast<long double>(0), std::min(static_cast<long double>(1), x));
}

int Filter::ClampIndex(int x, int upbound, int lowbound) {
    return std::max(lowbound, std::min(upbound - 1, x));
}

Image Grayscale::filter(const Image& a) {
    const long double RED_WEIGHT = 0.299;
    const long double Cgreen = 0.587;
    const long double Cblue = 0.114;
    Channel nw = a.GetRed();
    const Channel& nwred = a.GetRed();
    const Channel& nwgreen = a.GetGreen();
    const Channel& nwblue = a.GetBlue();
    for (int i = 0; i < a.GetWidth(); ++i) {
        for (int j = 0; j < a.GetHeight(); ++j) {
            nw[i][j] = nwred[i][j] * RED_WEIGHT + nwgreen[i][j] * Cgreen + nwblue[i][j] * Cblue;
        }
    }
    return Image(a.GetWidth(), a.GetHeight(), nw, nw, nw);
}

Image Negative::filter(const Image& a) {
    Channel nwred = a.GetRed();
    Channel nwgreen = a.GetGreen();
    Channel nwblue = a.GetBlue();
    for (int i = 0; i < a.GetWidth(); ++i) {
        for (int j = 0; j < a.GetHeight(); ++j) {
            nwred[i][j] = 1 - nwred[i][j];
            nwgreen[i][j] = 1 - nwgreen[i][j];
            nwblue[i][j] = 1 - nwblue[i][j];
        }
    }
    return Image(a.GetWidth(), a.GetHeight(), nwred, nwgreen, nwblue);
}

Crop::Crop(int a, int b) {
    if (a <= 0 || b <= 0) {
        throw std::invalid_argument("Фильтр -crop: ширина и высота должны быть положительными целыми числами");
    }
    nwidth = a;
    nheight = b;
}

Image Crop::filter(const Image& a) {
    int nw = std::min(nwidth, a.GetWidth());
    int nh = std::min(nheight, a.GetHeight());
    const Channel& oldred = a.GetRed();
    const Channel& oldgreen = a.GetGreen();
    const Channel& oldblue = a.GetBlue();
    Channel nwred = Channel(nw, nh);
    Channel nwgreen = Channel(nw, nh);
    Channel nwblue = Channel(nw, nh);
    for (int i = 0; i < nw; ++i) {
        for (int j = 0; j < nh; ++j) {
            nwred[i][j] = oldred[i][j];
            nwgreen[i][j] = oldgreen[i][j];
            nwblue[i][j] = oldblue[i][j];
        }
    }
    return Image(nw, nh, nwred, nwgreen, nwblue);
}

Image Sharpening::filter(const Image& a) {
    const Channel& oldred = a.GetRed();
    const Channel& oldgreen = a.GetGreen();
    const Channel& oldblue = a.GetBlue();
    Channel nwred = a.GetRed();
    Channel nwgreen = a.GetGreen();
    Channel nwblue = a.GetBlue();
    for (int i = 0; i < a.GetWidth(); ++i) {
        for (int j = 0; j < a.GetHeight(); ++j) {
            nwred[i][j] = ApplySharpeningKernel(oldred, i, j, a.GetWidth(), a.GetHeight());
            nwgreen[i][j] = ApplySharpeningKernel(oldgreen, i, j, a.GetWidth(), a.GetHeight());
            nwblue[i][j] = ApplySharpeningKernel(oldblue, i, j, a.GetWidth(), a.GetHeight());
        }
    }
    return Image(nwred.GetWidth(), nwred.GetHeight(), nwred, nwgreen, nwblue);
}

long double Sharpening::ApplySharpeningKernel(const Channel& matrix, int i, int j, int bw, int bh) {
    long double ret = SHARPENING_CENTER_WEIGHT * matrix[i][j];
    for (auto [mw, mh] : offsets_) {
        ret -= matrix[ClampIndex(i + mw, bw)][ClampIndex(j + mh, bh)];
    }
    return ClampColor(ret);
}

EdgeDetection::EdgeDetection(long double threshold_) {
    if (!std::isfinite(threshold_)) {
        throw std::invalid_argument("Фильтр -edge: threshold должен быть конечным действительным числом");
    }
    threshold = threshold_;
}

Image EdgeDetection::filter(const Image& a) {
    Grayscale gs;
    Image hlp = gs.filter(a);
    const Channel& grayscale_channel = hlp.GetRed();
    Channel nw = Channel(a.GetWidth(), a.GetHeight());
    for (int i = 0; i < a.GetWidth(); ++i) {
        for (int j = 0; j < a.GetHeight(); ++j) {
            if (ClampColor(ApplyEdgeKernel(grayscale_channel, i, j, a.GetWidth(), a.GetHeight())) > threshold) {
                nw[i][j] = 1;
            }
        }
    }
    return Image(a.GetWidth(), a.GetHeight(), nw, nw, nw);
}

long double EdgeDetection::ApplyEdgeKernel(const Channel& matrix, int i, int j, int bw, int bh) {
    long double ret = 4 * matrix[i][j];
    for (auto [mw, mh] : offsets_) {
        ret -= matrix[ClampIndex(i + mw, bw)][ClampIndex(j + mh, bh)];
    }
    return ret;
}

GaussianBlur::GaussianBlur(long double sigma_) {
    if (!std::isfinite(sigma_) || sigma_ <= 0) {
        throw std::invalid_argument("Фильтр -blur: sigma должна быть конечным положительным числом");
    }
    sigma = sigma_;
}

Image GaussianBlur::filter(const Image& a) {
    const Channel& oldred = a.GetRed();
    const Channel& oldgreen = a.GetGreen();
    const Channel& oldblue = a.GetBlue();
    Channel builder = Channel(a.GetWidth(), a.GetHeight());
    Channel halfblurred = builder;
    Channel halfblurgreen = builder;
    Channel halfblurblue = builder;
    Channel newblurred = builder;
    Channel newblurgreen = builder;
    Channel newblurblue = builder;
    if (3 * sigma + 1 >= std::numeric_limits<int>::max()) {
        throw std::invalid_argument("Фильтр -blur: sigma слишком велика для вычисления радиуса размытия");
    }
    int radius = static_cast<int>(3 * sigma + 1);
    std::vector<long double> weights(radius + 1);
    long double fixweight = 0;
    for (int i = 0; i <= radius; ++i) {
        const long double scaled = static_cast<long double>(i) / sigma;
        weights[i] = std::exp(-GAUSSIAN_EXPONENT_FACTOR * scaled * scaled);
        fixweight += 2 * weights[i];
    }
    fixweight -= weights[0];
    long double normalizator = static_cast<long double>(1) / fixweight;
    for (int i = 0; i < a.GetWidth(); ++i) {
        for (int j = 0; j < a.GetHeight(); ++j) {
            for (int j1 = j - radius; j1 <= j + radius; ++j1) {
                int x = j - j1;
                int j2 = ClampIndex(j1, a.GetHeight());
                halfblurred[i][j] += oldred[i][j2] * weights[abs(x)] * normalizator;
                halfblurgreen[i][j] += oldgreen[i][j2] * weights[abs(x)] * normalizator;
                halfblurblue[i][j] += oldblue[i][j2] * weights[abs(x)] * normalizator;
            }
        }
    }
    for (int i = 0; i < a.GetWidth(); ++i) {
        for (int j = 0; j < a.GetHeight(); ++j) {
            for (int i1 = i - radius; i1 <= i + radius; ++i1) {
                int x = i - i1;
                int i2 = ClampIndex(i1, a.GetWidth());
                newblurred[i][j] += halfblurred[i2][j] * weights[abs(x)] * normalizator;
                newblurgreen[i][j] += halfblurgreen[i2][j] * weights[abs(x)] * normalizator;
                newblurblue[i][j] += halfblurblue[i2][j] * weights[abs(x)] * normalizator;
            }
        }
    }
    for (int i = 0; i < a.GetWidth(); ++i) {
        for (int j = 0; j < a.GetHeight(); ++j) {
            newblurred[i][j] = ClampColor(newblurred[i][j]);
            newblurgreen[i][j] = ClampColor(newblurgreen[i][j]);
            newblurblue[i][j] = ClampColor(newblurblue[i][j]);
        }
    }
    return Image(a.GetWidth(), a.GetHeight(), newblurred, newblurgreen, newblurblue);
}

int Crystalize::RandomPoint(int a, int b) {
    static std::mt19937 mt(RANDOM_SEED);
    if (b <= a) {
        throw std::invalid_argument("Внутренняя ошибка программы");
    }
    return mt() % (b - a) + a;
}

std::vector<std::vector<std::pair<int, int>>> Crystalize::BuildCenters(int w, int h, int d) {
    int wd = (w + d - 1) / d;
    int hd = (h + d - 1) / d;
    std::vector<std::vector<std::pair<int, int>>> ret(wd, std::vector<std::pair<int, int>>(hd));
    for (int i = 0; i < wd; ++i) {
        for (int j = 0; j < hd; ++j) {
            int c1 = RandomPoint(i * d, std::min(w, i * d + d));
            int c2 = RandomPoint(j * d, std::min(h, j * d + d));
            ret[i][j] = {c1, c2};
        }
    }
    return ret;
}

std::pair<int, int> Crystalize::GetBlock(int i, int j, int d) {
    return {i / d, j / d};
}

std::int64_t Crystalize::SquaredDistance(int x1, int y1, int x2, int y2) {
    return (static_cast<std::int64_t>(x1) - x2) * (x1 - x2) + (static_cast<std::int64_t>(y1) - y2) * (y1 - y2);
}

Image Crystalize::filter(const Image& a) {
    int area = a.GetWidth() * a.GetHeight();
    int d = std::max(1, static_cast<int>(std::lround(std::sqrt(area / CRYSTAL_TARGET_COUNT))));
    std::vector<std::vector<std::pair<int, int>>> blockcentr = BuildCenters(a.GetWidth(), a.GetHeight(), d);
    int blwd = static_cast<int>(blockcentr.size());
    int blh = static_cast<int>(blockcentr[0].size());
    const Channel& oldred = a.GetRed();
    const Channel& oldgreen = a.GetGreen();
    const Channel& oldblue = a.GetBlue();
    Channel nwred = a.GetRed();
    Channel nwgreen = a.GetGreen();
    Channel nwblue = a.GetBlue();
    for (int i = 0; i < a.GetWidth(); ++i) {
        for (int j = 0; j < a.GetHeight(); ++j) {
            std::pair<int, int> nowbl = GetBlock(i, j, d);
            std::int64_t dist = INITIAL_DISTANCE;
            std::pair<int, int> closest = {0, 0};
            for (int i_ = std::max(0, nowbl.first - 2); i_ <= std::min(blwd - 1, nowbl.first + 2); ++i_) {
                for (int j_ = std::max(0, nowbl.second - 2); j_ <= std::min(blh - 1, nowbl.second + 2); ++j_) {
                    if (SquaredDistance(i, j, blockcentr[i_][j_].first, blockcentr[i_][j_].second) < dist) {
                        dist = SquaredDistance(i, j, blockcentr[i_][j_].first, blockcentr[i_][j_].second);
                        closest = {blockcentr[i_][j_].first, blockcentr[i_][j_].second};
                    }
                }
            }
            nwred[i][j] = oldred[closest.first][closest.second];
            nwgreen[i][j] = oldgreen[closest.first][closest.second];
            nwblue[i][j] = oldblue[closest.first][closest.second];
        }
    }

    return Image(a.GetWidth(), a.GetHeight(), nwred, nwgreen, nwblue);
}
