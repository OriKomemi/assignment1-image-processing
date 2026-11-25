#include "filters.hpp"

#include <png.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

namespace fs = std::filesystem;

struct PngImage : png_image {
    PngImage() : png_image{} { version = PNG_IMAGE_VERSION; }
    ~PngImage() { png_image_free(this); }
};

GrayImage readGrayscale(const fs::path& path) {
    const auto filename = path.string();
    PngImage png;
    if (!png_image_begin_read_from_file(&png, filename.c_str())) {
        throw std::runtime_error(filename + ": " + png.message);
    }

    constexpr auto maxDimension = std::numeric_limits<int>::max() / 2;
    if (png.width > maxDimension || png.height > maxDimension) {
        throw std::runtime_error(filename + ": image dimensions are too large");
    }

    png.format = PNG_FORMAT_RGBA;
    std::vector<png_byte> rgba(PNG_IMAGE_SIZE(png));
    if (!png_image_finish_read(&png, nullptr, rgba.data(), 0, nullptr)) {
        throw std::runtime_error(filename + ": " + png.message);
    }

    GrayImage gray(static_cast<int>(png.width), static_cast<int>(png.height));
    for (std::size_t i = 0; i < gray.pixels.size(); ++i) {
        const auto* rgb = rgba.data() + 4 * i;
        // BT.601 luma, rounded to the nearest integer. Alpha is ignored.
        gray.pixels[i] = static_cast<std::uint8_t>(
            (299 * rgb[0] + 587 * rgb[1] + 114 * rgb[2] + 500) / 1000);
    }
    return gray;
}

void writePng(const fs::path& path, const GrayImage& image) {
    const auto filename = path.string();
    PngImage png;
    png.width = image.width;
    png.height = image.height;
    png.format = PNG_FORMAT_GRAY;
    png.flags = PNG_IMAGE_FLAG_FAST;
    if (!png_image_write_to_file(&png, filename.c_str(), 0, image.pixels.data(), 0, nullptr)) {
        throw std::runtime_error(filename + ": " + png.message);
    }
}

void writeDebug(const fs::path& path, const GrayImage& image, int step) {
    std::string values;
    values.reserve(image.pixels.size() * 3);
    for (const auto pixel : image.pixels) {
        if (!values.empty()) {
            values += ',';
        }
        const int level = (pixel + step / 2) / step;
        values += std::to_string(level);
    }

    std::ofstream file(path, std::ios::binary);
    file << values;
    file.close();
    if (!file) {
        throw std::runtime_error("Could not write " + path.string());
    }
}

void writeResult(const fs::path& folder, const GrayImage& image,
                 const char* pngName, const char* debugName, int step) {
    writePng(folder / pngName, image);
    writeDebug(folder / debugName, image, step);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: assignment1 [input.png [output-directory]]\n";
        return 0;
    }
    if (argc > 3) {
        std::cerr << "Usage: assignment1 [input.png [output-directory]]\n";
        return 2;
    }

    try {
        const fs::path input = argc > 1 ? argv[1] : "Lenna.png";
        const fs::path folder = argc > 2 ? argv[2] : ".";
        const auto grayscale = readGrayscale(input);

        fs::create_directories(folder);
        writeResult(folder, grayscale, "Grayscale.png", "Grayscale.txt", 17);
        writeResult(folder, canny(grayscale), "Canny.png", "Canny.txt", 255);
        writeResult(folder, halftone(grayscale), "Haftone.png", "Halftone.txt", 255);
        writeResult(folder, floydSteinberg(grayscale),
                    "FloyedSteinberg.png", "FloyedSteinberg.txt", 17);

        std::cout << "Wrote four PNGs and four debug files to "
                  << fs::absolute(folder).lexically_normal().string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "assignment1: " << error.what() << '\n';
        return 1;
    }
}
