// Renders an SVG to PNG (lunasvg), then loads it, resizes it, draws on it and saves it (OpenCV).
// Each line shows a call, as written, and what it returned. Pass an image file to start from
// that instead of the built-in SVG. Writes only to the temporary directory.

#include <console.h>
#include <image.h>
#include <svg2png.h>
#include <timer.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    try {
        const auto folder = std::filesystem::temp_directory_path();
        std::string source = argc > 1 ? argv[1] : "";
        const auto output = (folder / "sc-image-demo-out.jpg").string();
        sc::console::title("simply-cpp image: rendering, loading, editing and saving, each call with what it returned");
        sc::timer sw;

        // [readme]
        if (source.empty()) {
            sc::console::heading("Rendering an SVG to PNG (lunasvg)");
            const std::string svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="320" height="200">)"
                                    R"(<rect width="320" height="200" fill="#2a6f97"/>)"
                                    R"(<circle cx="160" cy="100" r="70" fill="#f4d35e"/></svg>)";
            const auto png = sc::svg2png::FromString(svg);
            sc::console::show_text("sc::svg2png::FromString(svg)", std::to_string(png.size()) + " bytes of PNG");
            source = (folder / "sc-image-demo.png").string();
            std::ofstream{source, std::ios::binary} << png;
        }

        sc::console::heading("Loading and editing (OpenCV)");
        const sc::image image{source};
        sc::console::step("const sc::image image{source};   // " + source);
        SC_SHOW(image.size());
        auto preview = image.resized({160, 100});
        sc::console::step("auto preview = image.resized({160, 100});");
        SC_SHOW(preview.size());
        SC_STEP(preview.rect(sc::rect{10, 10, 140, 80}));
        SC_STEP(preview.text("simply-cpp", {20, 56}));

        sc::console::heading("Saving");
        SC_SHOW(preview.save(output));
        // [/readme]

        sc::console::output() << "\nThe result is " << output << "\nAll of that took " << sw << ".\n";
    } catch (const std::exception &error) {
        std::cerr << "sc-image-demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
