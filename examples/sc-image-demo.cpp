// Renders an SVG to PNG (sc::svg2png), then loads it, resizes it, draws on it and saves it (OpenCV).
// Each line shows a call, as written, and what it returned. --help lists the options: an image
// file to start from instead of the built-in SVG. Writes only to the temporary directory.

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>

#include <params.h>
#include <sc.h>

#include <image.h>
#include <svg2png.h>

int main(int argc, char **argv) {
    // The command line: -h/--help comes with sc::params, which also reports mistakes and exits.
    const sc::params args{argc, argv, {
        {"name", "sc-image-demo"},
        {"description", "Renders an SVG to a PNG, then loads it back, resizes it, draws on it and saves the result: the round "
                        "trip from vector graphics to pixels. It only writes to the temporary directory.\nMore on the wiki: "
                        "https://github.com/roelofrossouw/simply-cpp/wiki/Images"},
        {"positional", {
            {"image", {{"required", false}, {"help", "an image to start from instead of the built-in SVG"}}}}}}};

    try {
        const auto folder = std::filesystem::temp_directory_path();
        std::string source = args.as<std::string>("image", "");
        const auto output = (folder / "sc-image-demo-out.jpg").string();
        sc::console::title("simply-cpp image: rendering, loading, editing and saving, each call with what it returned");
        sc::timer sw;

        // [readme]
        if (source.empty()) {
            sc::console::heading("Rendering an SVG to PNG (sc::svg2png)");
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
        SC_STEP(preview.draw(sc::rect{10, 10, 140, 80}, sc::stroke{sc::color::White, 2}));
        SC_STEP(preview.draw(sc::text{"simply-cpp", {20, 56}, sc::font::bold("sans-serif", 18)}, sc::color::White));

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
