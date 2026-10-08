// Renders an SVG to PNG (lunasvg), loads it, draws on it, resizes and saves it (OpenCV).
// Pass an image file to use that instead of the built-in SVG. Needs no server, and writes
// only to the temporary directory.

#include <image.h>
#include <svg2png.h>
#include <timer.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char **argv) {
    try {
        const auto folder = std::filesystem::temp_directory_path();
        std::string source = argc > 1 ? argv[1] : "";
        if (source.empty()) {
            source = (folder / "sc-image-demo.png").string();
            std::ofstream{source, std::ios::binary} << sc::svg2png::FromString(
                R"(<svg xmlns="http://www.w3.org/2000/svg" width="320" height="200">)"
                R"(<rect width="320" height="200" fill="#2a6f97"/>)"
                R"(<circle cx="160" cy="100" r="70" fill="#f4d35e"/></svg>)");
        }
        const auto output = (folder / "sc-image-demo-out.jpg").string();

        // [readme]
        sc::timer sw;
        const sc::image image{source};
        std::cout << "Loaded " << source << ", size " << image.size() << '\n';

        auto preview = image.resized({160, 100});
        preview.rect(sc::rect{10, 10, 140, 80});
        preview.text("simply-cpp", {20, 56});
        if (!preview.save(output)) throw std::runtime_error{"could not save " + output};
        std::cout << "Saved " << output << ", size " << preview.size() << '\n';
        std::cout << "Done after " << sw << '\n';
        // [/readme]
    } catch (const std::exception &error) {
        std::cerr << "sc-image-demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
