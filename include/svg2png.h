#ifndef SC_SVG2PNG_H
#define SC_SVG2PNG_H

#include <color.h>
#include <size.h>

#include <string>
#include <string_view>

namespace sc {
    /// SVG to PNG, as bytes: read by sc-graphics' sc::drawing and drawn on an sc::image (OpenCV),
    /// so it draws what both do: paths, the basic shapes, groups, transforms, flat colours and
    /// strokes, and text in OpenCV's Hershey fonts. A UTF-8 BOM is allowed. All throw
    /// std::invalid_argument when the SVG can't be read.
    ///
    /// Clip paths are drawn, and a white mask (read as a clip). TODO: gradients, masks by their
    /// luminance, opacity (transparent parts come out opaque, a transparent background white), the
    /// nonzero fill rule and text in real fonts.
    class svg2png {
    public:
        /// At the SVG's own size, on white.
        static std::string FromString(std::string_view svg_data);

        static std::string FromFile(const std::string &filename);

        /// width x height pixels (0 for the SVG's own size; one of them 0 keeps the aspect ratio), on
        /// background.
        static std::string FromString(std::string_view svg_data, int width, int height, const color &background);

        static std::string FromFile(const std::string &filename, int width, int height, const color &background);

        /// The SVG's own size in pixels: its viewBox's, or its width and height.
        static size Size(std::string_view svg_data);
    };
}

#endif //SC_SVG2PNG_H
