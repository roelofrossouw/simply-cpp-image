#ifndef SC_SVG2PNG_H
#define SC_SVG2PNG_H
#include <string>

namespace sc {
    class svg2png {
        static void copy_data(void *target, void *source, int size);

        static std::string PngData(void *data);

    public:
        static std::string FromString(std::string_view svg_data);

        static std::string FromFile(const std::string &filename);
    };
}

#endif //SC_SVG2PNG_H
