// Changing images with sc-image: resizing, cropping, rotating, straightening a tilted area,
// grayscale, joining two side by side, and preparing an image as a neural network's input. Each line
// shows a call, as written, and what it returned. Pass an image file to use that instead of the
// built-in SVG. Writes only to the temporary directory.

#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>

#include <sc.h>

#include <image.h>
#include <svg2png.h>

int main(int argc, char **argv) {
    try {
        const auto folder = std::filesystem::temp_directory_path();
        std::string source = argc > 1 ? argv[1] : "";
        sc::console::title("simply-cpp image: resizing, cropping, rotating and more");
        if (source.empty()) {
            sc::console::output() << "No image given (sc-image-transform <image>), so using a rendered one\n";
            source = (folder / "sc-image-transform.png").string();
            std::ofstream{source, std::ios::binary} << sc::svg2png::FromString(
                R"(<svg xmlns="http://www.w3.org/2000/svg" width="640" height="360">)"
                R"(<rect width="640" height="360" fill="#87ceeb"/>)"
                R"(<circle cx="520" cy="80" r="45" fill="#f4d35e"/>)"
                R"(<rect y="270" width="640" height="90" fill="#3a7d44"/>)"
                R"svg(<rect x="170" y="150" width="300" height="60" fill="#c0392b" transform="rotate(-15 320 180)"/></svg>)svg");
        }
        const sc::image photo{source};
        sc::console::step("const sc::image photo{source};   // " + source);
        SC_SHOW(photo.size());
        SC_SHOW(photo.channels());

        sc::console::heading("Resizing, cropping and rotating");
        SC_SHOW(photo.resized({320, 180}).size());
        SC_SHOW(photo.cropped(sc::rect_i{100, 50, 200, 100}).size());
        auto turned = photo;
        SC_STEP(turned.rotate(90)); // in steps of 90 degrees
        SC_SHOW(turned.size());

        sc::console::heading("Straightening a tilted area: deskewed()");
        sc::console::note("Cuts out a rotated rectangle and turns it level, as for a line of text in a photo.");
        const sc::rotated_rect banner{{320, 180}, {300, 60}, -15}; // centre, size, angle
        const auto level = photo.deskewed(banner);
        sc::console::step("const auto level = photo.deskewed(banner);");
        SC_SHOW(level.size());
        SC_SHOW(level.save((folder / "sc-image-transform-level.png").string()));

        sc::console::heading("Grayscale, and two images side by side");
        auto gray = photo.resized({320, 180});
        SC_STEP(gray.channels(1));
        SC_SHOW(gray.channels());
        SC_STEP(gray.channels(3)); // back to three channels (still gray) to sit beside a colour one
        const auto pair = sc::image::side_by_side(photo.resized({320, 180}), gray);
        sc::console::step("const auto pair = sc::image::side_by_side(photo.resized({320, 180}), gray);");
        SC_SHOW(pair.size());
        SC_SHOW(pair.save((folder / "sc-image-transform-pair.jpg").string()));

        sc::console::heading("Preparing a neural network's input");
        sc::console::note("Networks take sizes in multiples of 32: the image is scaled to fit and padded around.");
        auto input = photo;
        SC_SHOW(input.get_snap_size(32));
        SC_STEP(input.snap_to_size(32));
        SC_SHOW(input.size());
        SC_SHOW(input.padding());      // added on each side
        SC_SHOW(input.cropped_size()); // the picture inside the padding
        sc::console::subheading("As a blob of floats, [batch, channels, height, width]");
        SC_STEP(input.generate_blob(1.0 / 255, 0, true)); // scaled to 0..1, BGR swapped to RGB
        SC_SHOW(std::vector<int64_t>(input.blob_shape(), input.blob_shape() + input.blob_shape_size()));
        SC_SHOW(input.blob_size());
        const auto back = sc::image::from_blob(input.blob(), input.size().width(), input.size().height(), 1.0 / 255, 0, true);
        sc::console::step("const auto back = sc::image::from_blob(input.blob(), width, height, 1.0 / 255, 0, true);");
        SC_SHOW(back.size());

        sc::console::output() << "\nThe images are in " << folder.string() << " (sc-image-transform-*)\n";
    } catch (const std::exception &error) {
        std::cerr << "sc-image-transform: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
