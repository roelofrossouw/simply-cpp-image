#include "svg2png.h"

#include "opencv.h"

#include <core.h>
#include <drawing.h>
#include <utf8.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sc {
    namespace {
        drawing parsed(const std::string_view svg_data) {
            try {
                return drawing::from_svg(utf8::strip_bom(svg_data));
            } catch (const std::runtime_error &error) {
                throw std::invalid_argument{std::string{"svg2png: "} + error.what()};
            }
        }

        // Its page, or what's drawn when it has none.
        size own_size(const drawing &picture) {
            if (picture.page().area() > 0) return picture.page();
            const rect covered = picture.bounds();
            return {std::max(0.0, covered.right()), std::max(0.0, covered.bottom())};
        }
    }

    std::string svg2png::FromString(const std::string_view svg_data) { return FromString(svg_data, 0, 0, color::White); }

    std::string svg2png::FromFile(const std::string &filename) { return FromString(file_get_contents(filename)); }

    std::string svg2png::FromString(const std::string_view svg_data, int width, int height, const color &background) {
        const auto picture = parsed(svg_data);
        const size own = own_size(picture);
        if (own.width() <= 0 || own.height() <= 0) throw std::invalid_argument{"svg2png: the SVG has no size"};
        if (width <= 0 && height <= 0) {
            width = static_cast<int>(std::lround(own.width()));
            height = static_cast<int>(std::lround(own.height()));
        } else if (width <= 0) {
            width = static_cast<int>(std::lround(own.width() * height / own.height()));
        } else if (height <= 0) {
            height = static_cast<int>(std::lround(own.height() * width / own.width()));
        }
        image canvas{{std::max(1, width), std::max(1, height)}, background.alpha() > 0 ? background : color::White}; // TODO: transparency
        canvas.draw(picture, rect{0, 0, static_cast<double>(canvas.size().width()), static_cast<double>(canvas.size().height())});
        return canvas.encoded("png");
    }

    std::string svg2png::FromFile(const std::string &filename, const int width, const int height, const color &background) {
        return FromString(file_get_contents(filename), width, height, background);
    }

    size svg2png::Size(const std::string_view svg_data) { return own_size(parsed(svg_data)); }
}
