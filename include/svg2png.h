#ifndef SC_SVG2PNG_H
#define SC_SVG2PNG_H
#include <string>

namespace sc {
    /// SVG to PNG with lunasvg, at the SVG's own size. A UTF-8 BOM is allowed. Both throw
    /// std::invalid_argument when the SVG can't be read, parsed or rendered; the PNG comes as bytes.
    class svg2png {
        static void copy_data(void *target, void *source, int size);

        static std::string PngData(void *data);

    public:
        static std::string FromString(std::string_view svg_data);

        static std::string FromFile(const std::string &filename);
    };
}

#endif //SC_SVG2PNG_H
