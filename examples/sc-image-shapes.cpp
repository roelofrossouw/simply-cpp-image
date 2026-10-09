// Finding and drawing shapes with sc-image: an SVG of white shapes on black is rendered, their
// outlines found as contours and fitted with rotated rectangles, and sc-core geometry drawn back on
// top. Each line shows a call, as written, and what it returned. Writes only to the temporary
// directory.

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>

#include <sc.h>

#include <image.h>
#include <svg2png.h>

int main() {
    try {
        const auto folder = std::filesystem::temp_directory_path();
        const auto source = (folder / "sc-image-shapes.png").string();
        const auto output = (folder / "sc-image-shapes-out.png").string();
        sc::console::title("simply-cpp image: finding and drawing shapes");

        sc::console::heading("A test image");
        sc::console::note("A tilted rectangle, a circle and a triangle, rendered from SVG.");
        std::ofstream{source, std::ios::binary} << sc::svg2png::FromString(
            R"(<svg xmlns="http://www.w3.org/2000/svg" width="400" height="240">)"
            R"(<rect width="400" height="240" fill="black"/>)"
            R"svg(<rect x="50" y="60" width="110" height="50" fill="white" transform="rotate(20 105 85)"/>)svg"
            R"(<circle cx="290" cy="80" r="45" fill="white"/>)"
            R"(<polygon points="150,215 210,150 260,210" fill="white"/></svg>)");
        const sc::image shapes{source};
        sc::console::step("const sc::image shapes{source};   // " + source);
        SC_SHOW(shapes.size());

        sc::console::heading("Outlines: find_contours()");
        const auto contours = shapes.find_contours();
        sc::console::step("const auto contours = shapes.find_contours();");
        SC_SHOW(contours.size());
        for (const auto &contour: contours) {
            const auto bounds = static_cast<sc::rect_i>(sc::polygon{contour});
            sc::console::note(std::to_string(contour.size()) + " points, within " + sc::console::format(bounds));
        }

        sc::console::heading("Fitted rectangles: find_min_area_rects()");
        sc::console::note("Each outline's smallest enclosing rectangle, at whatever angle fits: centre x size @ angle.");
        const auto fitted = shapes.find_min_area_rects(100); // ignore anything under 100 pixels
        sc::console::step("const auto fitted = shapes.find_min_area_rects(100);");
        for (const auto &box: fitted) sc::console::note(sc::console::format(box));

        sc::console::heading("Drawing on it");
        SC_STEP(shapes.draw(fitted));                         // rotated rectangles, as outlines
        SC_STEP(shapes.draw(sc::circle{{290, 80}, 55}));       // sc-core's circle
        SC_STEP(shapes.draw(sc::rect{10, 10, 380, 220}));      // and rectangle
        SC_STEP(shapes.text("3 shapes", {300, 225}));
        SC_SHOW(shapes.save(output));

        sc::console::output() << "\nThe result is " << output << '\n';
    } catch (const std::exception &error) {
        std::cerr << "sc-image-shapes: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
