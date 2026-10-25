#pragma once

#include "Image.h"

#include <cstdint>
#include <utility>
#include <vector>

struct Filter {
    virtual Image filter(const Image& a) = 0;
    virtual ~Filter();

protected:
    long double ClampColor(long double x);
    int ClampIndex(int x, int upbound, int lowbound = 0);
};

struct Grayscale : Filter {
    Image filter(const Image& a) override;
};

struct Negative : Filter {
    Image filter(const Image& a) override;
};

struct Crop : Filter {
    int nwidth;
    int nheight;
    Crop(int a, int b);
    Image filter(const Image& a) override;
};

struct Sharpening : Filter {
    Image filter(const Image& a) override;

private:
    std::vector<std::pair<int, int>> offsets_ = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    long double ApplySharpeningKernel(const Channel& matrix, int i, int j, int bw, int bh);
};

struct EdgeDetection : Filter {
    long double threshold;

    explicit EdgeDetection(long double threshold_);

    Image filter(const Image& a) override;

private:
    std::vector<std::pair<int, int>> offsets_ = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

    long double ApplyEdgeKernel(const Channel& matrix, int i, int j, int bw, int bh);
};

struct GaussianBlur : Filter {
    long double sigma;

    explicit GaussianBlur(long double sigma_);

    Image filter(const Image& a) override;
};

struct Crystalize : Filter {
    Image filter(const Image& a) override;

private:
    std::vector<std::vector<std::pair<int, int>>> BuildCenters(int w, int h, int d);

    std::pair<int, int> GetBlock(int i, int j, int d);

    std::int64_t SquaredDistance(int x1, int y1, int x2, int y2);

    int RandomPoint(int a, int b);
};
