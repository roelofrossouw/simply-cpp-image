// Renders an SVG to PNG (lunasvg), then loads it, resizes it, draws on it and saves it (OpenCV).
// Each line shows a call, as written, and what it returned. Pass an image file to start from
// that instead of the built-in SVG. Writes only to the temporary directory.

#include <image.h>
#include <svg2png.h>
#include <timer.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {
    // One step of the demo: the call, as written in the source, and what it returned.
    template<typename T>
    void show(const std::string_view call, const T &result) {
        std::ostringstream text;
        text << std::boolalpha << result;
        std::cout << "  " << call << "\n      -> " << text.str() << '\n';
    }

    void heading(const std::string_view title) { std::cout << '\n' << title << '\n'; }
}

#define SHOW(expression) show(#expression, expression)
#define STEP(statement) (std::cout << "  " << #statement << '\n', statement)

int main(int argc, char **argv) {
    try {
        const auto folder = std::filesystem::temp_directory_path();
        std::string source = argc > 1 ? argv[1] : "";
        const auto output = (folder / "sc-image-demo-out.jpg").string();
        std::cout << "simply-cpp image: rendering, loading, editing and saving, each call with what it returned\n";
        sc::timer sw;

        // [readme]
        if (source.empty()) {
            heading("Rendering an SVG to PNG (lunasvg)");
            const std::string svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="320" height="200">)"
                                    R"(<rect width="320" height="200" fill="#2a6f97"/>)"
                                    R"(<circle cx="160" cy="100" r="70" fill="#f4d35e"/></svg>)";
            const auto png = sc::svg2png::FromString(svg);
            std::cout << "  sc::svg2png::FromString(svg)\n      -> " << png.size() << " bytes of PNG\n";
            source = (folder / "sc-image-demo.png").string();
            std::ofstream{source, std::ios::binary} << png;
        }

        heading("Loading and editing (OpenCV)");
        const sc::image image{source};
        std::cout << "  const sc::image image{source};   // " << source << '\n';
        SHOW(image.size());
        auto preview = image.resized({160, 100});
        std::cout << "  auto preview = image.resized({160, 100});\n";
        SHOW(preview.size());
        STEP(preview.rect(sc::rect{10, 10, 140, 80}));
        STEP(preview.text("simply-cpp", {20, 56}));

        heading("Saving");
        SHOW(preview.save(output));
        // [/readme]

        std::cout << "\nThe result is " << output << "\nAll of that took " << sw << ".\n";
    } catch (const std::exception &error) {
        std::cerr << "sc-image-demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
