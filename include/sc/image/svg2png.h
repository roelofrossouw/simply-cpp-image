#ifndef SC_SVG2PNG_H
#define SC_SVG2PNG_H

#include <sc/core/color.h>
#include <sc/geometry/size.h>

#include <string>
#include <string_view>

namespace sc::image {
    /// SVG to PNG, as bytes: read by sc-graphics' sc::graphics::drawing and drawn on an sc::image (OpenCV),
    /// so it draws what both do: paths, the basic shapes, groups, transforms, flat colours and
    /// strokes, opacity, and text in its font's own glyphs (as it's measured, so text-anchor
    /// lines up). A UTF-8 BOM is allowed. All throw
    /// std::invalid_argument when the SVG can't be read.
    ///
    /// Clip paths are drawn, a white mask (read as a clip) and linear gradients; fill rules are
    /// kept. TODO: radial gradients (drawn flat), masks by their luminance and a gradient's
    /// stop opacity; a transparent background comes out white.
    class svg2png {
    public:
        /// At the SVG's own size, on white.
        static std::string from_string(std::string_view svg_data);

        static std::string from_file(const std::string &filename);

        /// width x height pixels (0 for the SVG's own size; one of them 0 keeps the aspect ratio), on
        /// background.
        static std::string from_string(std::string_view svg_data, int width, int height, const color &background);

        static std::string from_file(const std::string &filename, int width, int height, const color &background);

        /// The SVG's own size in pixels: its viewBox's, or its width and height.
        static sc::size size(std::string_view svg_data);
    };
}

#endif //SC_SVG2PNG_H
