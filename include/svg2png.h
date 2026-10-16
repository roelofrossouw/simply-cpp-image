#ifndef SC_SVG2PNG_H
#define SC_SVG2PNG_H
#include <color.h>
#include <size.h>

#include <string>
#include <string_view>

namespace sc {
    /// SVG to PNG with lunasvg, as bytes. A UTF-8 BOM is allowed. All throw std::invalid_argument
    /// when the SVG can't be read, parsed or rendered.
    class svg2png {
        static void copy_data(void *target, void *source, int size);

        static std::string PngData(void *data, int width = -1, int height = -1, unsigned background = 0);

    public:
        /// At the SVG's own size, on a transparent background.
        static std::string FromString(std::string_view svg_data);

        static std::string FromFile(const std::string &filename);

        /// width x height pixels (0 for the SVG's own size; one of them 0 keeps the aspect ratio), on
        /// background.
        static std::string FromString(std::string_view svg_data, int width, int height, const color &background);

        static std::string FromFile(const std::string &filename, int width, int height, const color &background);

        /// The SVG's own size in pixels: its width and height, or its viewBox's.
        static size Size(std::string_view svg_data);
    };
}

#endif //SC_SVG2PNG_H
