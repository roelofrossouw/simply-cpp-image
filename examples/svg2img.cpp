// svg2img: an SVG file as an image (PNG, JPEG, WebP, BMP or TIFF), at its own size or one given:
// read by sc-graphics' sc::drawing and drawn on an sc::image (sc::svg2png). --help lists the options.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include <core.h>
#include <params.h>

#include <opencv.h>
#include <svg2png.h>

namespace {
    // An extension's image type: "jpeg" is jpg, "tif" tiff; empty when it isn't one.
    std::string type_of(std::string extension) {
        if (!extension.empty() && extension.front() == '.') extension.erase(0, 1);
        std::ranges::transform(extension, extension.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        static const std::map<std::string, std::string> types{{"png", "png"}, {"jpg", "jpg"}, {"jpeg", "jpg"}, {"webp", "webp"},
                                                              {"bmp", "bmp"}, {"tif", "tiff"}, {"tiff", "tiff"}};
        const auto found = types.find(extension);
        return found == types.end() ? std::string{} : found->second;
    }
}

int main(int argc, char **argv) {
    const sc::params args{argc, argv, {
        {"name", "svg2img"},
        {"description", "Renders an SVG file as an image: PNG (the default), JPEG, WebP, BMP or TIFF. Without OUTPUT it's written "
                        "next to INPUT, with the type's extension. Not drawn yet: gradients, masks, clipping, transparency and "
                        "text in real fonts (OpenCV's Hershey fonts stand in)."},
        {"options", {
            {"type", {{"short", "t"}, {"choices", {"png", "jpg", "webp", "bmp", "tiff"}},
                      {"help", "the image type (default: OUTPUT's extension, else png)"}}},
            {"width", {{"short", "W"}, {"type", "integer"}, {"value", "PIXELS"}, {"help", "the width; alone, the height keeps the aspect ratio"}}},
            {"height", {{"short", "H"}, {"type", "integer"}, {"value", "PIXELS"}, {"help", "the height; alone, the width keeps the aspect ratio"}}},
            {"scale", {{"short", "s"}, {"type", "number"}, {"default", 1}, {"help", "the SVG's own size times this, without a width or height"}}},
            {"background", {{"short", "b"}, {"value", "COLOUR"},
                            {"help", "a CSS colour behind the drawing (default: white; transparent isn't drawn yet)"}}},
            {"quality", {{"short", "q"}, {"type", "integer"}, {"default", 95}, {"help", "JPEG and WebP quality, 1 to 100"}}}}},
        {"positional", {
            {"input", {{"help", "the SVG file"}}},
            {"output", {{"required", false}, {"help", "the image to write"}}}}}}};

    try {
        const std::filesystem::path input = args.as<std::string>("input");
        const std::filesystem::path given = args.as<std::string>("output", "");
        std::string type = args.as<std::string>("type", "");
        if (!given.empty()) {
            const auto by_extension = type_of(given.extension().string());
            if (type.empty() && by_extension.empty()) throw std::invalid_argument{"can't tell the image type of " + given.string() + ": give --type"};
            if (!type.empty() && !by_extension.empty() && type != by_extension)
                throw std::invalid_argument{given.string() + " isn't a " + type + " file name"};
            if (type.empty()) type = by_extension;
        }
        if (type.empty()) type = "png";
        const std::filesystem::path output = given.empty() ? std::filesystem::path{input}.replace_extension(type) : given;

        if (!std::filesystem::is_regular_file(input)) throw std::runtime_error{"can't read " + input.string()};
        const std::string svg = sc::file_get_contents(input.string());

        int width = args.as<int>("width", 0), height = args.as<int>("height", 0);
        if (width < 0 || height < 0) throw std::invalid_argument{"the width and height can't be negative"};
        if (width == 0 && height == 0) {
            const double scale = args.as<double>("scale");
            if (scale <= 0) throw std::invalid_argument{"--scale must be above 0"};
            const auto own = sc::svg2png::Size(svg);
            width = std::max(1, static_cast<int>(std::lround(own.width() * scale)));
            height = std::max(1, static_cast<int>(std::lround(own.height() * scale)));
        }
        const auto background_name = args.as<std::string>("background", "white");
        const sc::color background{background_name};

        const std::string png = sc::svg2png::FromString(svg, width, height, background);
        if (type == "png") {
            std::ofstream out{output, std::ios::binary};
            out << png;
            if (!out) throw std::runtime_error{"can't write " + output.string()};
        } else {
            const sc::image picture{png};
            if (!picture.save(output.string(), args.as<int>("quality"))) throw std::runtime_error{"can't write " + output.string()};
        }
        const sc::image written{output.string()};
        std::cout << output.string() << ": " << written.size().width() << "x" << written.size().height() << " " << type << '\n';
    } catch (const std::exception &error) {
        std::cerr << "svg2img: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
