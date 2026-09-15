#include <svg2png.h>
#include <lunasvg.h>
#include <stdexcept>
#include "core.h"
#include "utf8.h"

using namespace lunasvg;
using namespace std;

namespace sc {
    void svg2png::copy_data(void *target, void *source, int size) {
        auto *png_string = (string *) target;
        *png_string = string(static_cast<char *>(source), size);
    }

    std::string svg2png::PngData(void *data) {
        if (data == nullptr) throw std::invalid_argument("svg2png::PngData: Invalid SVG data");
        auto *document = static_cast<Document *>(data);
        const auto bitmap = document->renderToBitmap();
        if (bitmap.isNull()) throw std::invalid_argument("svg2png::PngData: Could not create PNG");
        string png_data;
        bitmap.writeToPng(copy_data, &png_data);
        return png_data;
    }

    std::string svg2png::FromString(const std::string_view svg_data) {
        // Up to v3.5.0 a UTF8 bom causes an error
        auto document = Document::loadFromData(utf8::strip_bom(svg_data).data());
        return PngData(document.get());
    }

    std::string svg2png::FromFile(const std::string &filename) {
        // Don't use loadfromfile directly up to v3.5.0 the UTF8 bom causes an error
        return FromString(file_get_contents(filename));
    }
}
