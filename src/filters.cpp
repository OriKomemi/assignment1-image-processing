#include "filters.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

std::vector<float> gaussianBlur(const GrayImage& input) {
    constexpr std::array<int, 5> weights{1, 4, 6, 4, 1};
    std::vector<float> horizontal(input.pixels.size());
    std::vector<float> blurred(input.pixels.size());

    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            int sum = 0;
            for (int k = -2; k <= 2; ++k) {
                const int nx = std::clamp(x + k, 0, input.width - 1);
                sum += weights[k + 2] * input.pixels[input.index(nx, y)];
            }
            horizontal[input.index(x, y)] = static_cast<float>(sum);
        }
    }
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            float sum = 0.0f;
            for (int k = -2; k <= 2; ++k) {
                const int ny = std::clamp(y + k, 0, input.height - 1);
                sum += weights[k + 2] * horizontal[input.index(x, ny)];
            }
            blurred[input.index(x, y)] = sum / 256.0f;
        }
    }
    return blurred;
}

struct Gradient {
    float x = 0.0f;
    float y = 0.0f;
    float magnitude = 0.0f;
};

std::vector<Gradient> sobelGradients(const GrayImage& input) {
    const auto blurred = gaussianBlur(input);
    std::vector<Gradient> gradients(input.pixels.size());
    for (int y = 1; y < input.height - 1; ++y) {
        for (int x = 1; x < input.width - 1; ++x) {
            const auto i = input.index(x, y);
            const auto top = i - input.width;
            const auto bottom = i + input.width;
            const float gx = blurred[top + 1] + 2 * blurred[i + 1] + blurred[bottom + 1]
                           - blurred[top - 1] - 2 * blurred[i - 1] - blurred[bottom - 1];
            const float gy = blurred[bottom - 1] + 2 * blurred[bottom] + blurred[bottom + 1]
                           - blurred[top - 1] - 2 * blurred[top] - blurred[top + 1];
            gradients[i] = {gx, gy, std::sqrt(gx * gx + gy * gy)};
        }
    }

    return gradients;
}

}  // namespace

GrayImage canny(const GrayImage& input) {
    GrayImage edges(input.width, input.height);
    if (input.width < 3 || input.height < 3) {
        return edges;
    }

    const auto gradients = sobelGradients(input);
    constexpr float lowThreshold = 35.0f;
    constexpr float highThreshold = 85.0f;
    constexpr std::uint8_t weak = 1;
    constexpr std::uint8_t strong = 255;
    std::vector<std::size_t> frontier;

    for (int y = 1; y < input.height - 1; ++y) {
        for (int x = 1; x < input.width - 1; ++x) {
            const auto i = input.index(x, y);
            const auto [gx, gy, magnitude] = gradients[i];
            if (magnitude < lowThreshold) {
                continue;
            }

            // Interpolate the magnitudes one step away in the gradient direction.
            const float ax = std::abs(gx);
            const float ay = std::abs(gy);
            const int slope = (gx < 0) == (gy < 0) ? 1 : -1;
            const bool horizontal = ax >= ay;
            const int dx = horizontal ? 1 : 0;
            const int dy = horizontal ? 0 : 1;
            const float weight = horizontal ? ay / ax : ax / ay;
            const auto forward = gradients[input.index(x + dx, y + dy)].magnitude;
            const auto backward = gradients[input.index(x - dx, y - dy)].magnitude;
            const int diagonalX = horizontal ? 1 : slope;
            const int diagonalY = horizontal ? slope : 1;
            const auto forwardDiagonal =
                gradients[input.index(x + diagonalX, y + diagonalY)].magnitude;
            const auto backwardDiagonal =
                gradients[input.index(x - diagonalX, y - diagonalY)].magnitude;
            const float ahead = forward + weight * (forwardDiagonal - forward);
            const float behind = backward + weight * (backwardDiagonal - backward);
            // Resolve equal adjacent peaks consistently so a straight edge stays thin.
            if (magnitude < ahead || magnitude <= behind) {
                continue;
            }
            if (magnitude >= highThreshold) {
                edges.pixels[i] = strong;
                frontier.push_back(i);
            } else {
                edges.pixels[i] = weak;
            }
        }
    }

    // Only weak pixels connected to a strong edge survive hysteresis.
    while (!frontier.empty()) {
        const auto i = static_cast<std::ptrdiff_t>(frontier.back());
        frontier.pop_back();
        for (int dy = -1; dy <= 1; ++dy) {
            const auto row = i + dy * input.width;
            for (int dx = -1; dx <= 1; ++dx) {
                const auto neighbor = row + dx;
                if (edges.pixels[neighbor] == weak) {
                    edges.pixels[neighbor] = strong;
                    frontier.push_back(neighbor);
                }
            }
        }
    }
    std::replace(edges.pixels.begin(), edges.pixels.end(), weak, std::uint8_t{0});
    return edges;
}

GrayImage halftone(const GrayImage& input) {
    GrayImage output(input.width * 2, input.height * 2);
    constexpr std::array<int, 4> rank{0, 2, 3, 1};
    for (int y = 0; y < input.height; ++y) {
        for (int x = 0; x < input.width; ++x) {
            const int white = (4 * input.pixels[input.index(x, y)] + 127) / 255;
            const auto top = output.index(2 * x, 2 * y);
            const auto bottom = top + output.width;
            output.pixels[top] = white > rank[0] ? 255 : 0;
            output.pixels[top + 1] = white > rank[1] ? 255 : 0;
            output.pixels[bottom] = white > rank[2] ? 255 : 0;
            output.pixels[bottom + 1] = white > rank[3] ? 255 : 0;
        }
    }
    return output;
}

GrayImage floydSteinberg(const GrayImage& input) {
    GrayImage output(input.width, input.height);
    std::vector<float> adjusted(input.pixels.begin(), input.pixels.end());
    for (int y = 0; y < input.height; ++y) {
        const bool below = y + 1 < input.height;
        for (int x = 0; x < input.width; ++x) {
            const auto i = input.index(x, y);
            const int level = static_cast<int>(std::clamp(adjusted[i], 0.0f, 255.0f)
                                               / 17.0f + 0.5f);
            output.pixels[i] = static_cast<std::uint8_t>(17 * level);
            const float error = adjusted[i] - output.pixels[i];
            const bool left = x > 0;
            const bool right = x + 1 < input.width;

            if (left && right && below) {
                const float unit = error / 16.0f;
                adjusted[i + 1] += 7 * unit;
                adjusted[i + input.width - 1] += 3 * unit;
                adjusted[i + input.width] += 5 * unit;
                adjusted[i + input.width + 1] += unit;
                continue;
            }

            // At a border, share the error equally among the remaining neighbors.
            std::array<std::size_t, 3> neighbors{};
            int count = 0;
            if (right) {
                neighbors[count++] = i + 1;
            }
            if (below) {
                const auto next = i + input.width;
                neighbors[count++] = next;
                if (left) {
                    neighbors[count++] = next - 1;
                }
                if (right) {
                    neighbors[count++] = next + 1;
                }
            }
            for (int n = 0; n < count; ++n) {
                adjusted[neighbors[n]] += error / count;
            }
        }
    }
    return output;
}
