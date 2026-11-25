#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct GrayImage {
    int width;
    int height;
    std::vector<std::uint8_t> pixels;

    GrayImage(int width, int height)
        : width(width), height(height),
          pixels(static_cast<std::size_t>(width) * height) {}

    std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(y) * width + x;
    }
};

GrayImage canny(const GrayImage& input);
GrayImage halftone(const GrayImage& input);
GrayImage floydSteinberg(const GrayImage& input);
