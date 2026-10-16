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
        // lunasvg up to 3.5.0 rejects a UTF-8 BOM, so strip it.
        const auto svg = utf8::strip_bom(svg_data);
        auto document = Document::loadFromData(svg.data(), svg.size()); // a string_view needn't end in a null
        return PngData(document.get());
    }

    std::string svg2png::FromFile(const std::string &filename) {
        // Not lunasvg's loadFromFile(): it would keep a UTF-8 BOM (see above).
        return FromString(file_get_contents(filename));
    }
}
