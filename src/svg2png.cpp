#include <svg2png.h>
#include <lunasvg.h>
#include <algorithm>
#include <cmath>
#include <memory>
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

    std::string svg2png::PngData(void *data, const int width, const int height, const unsigned background) {
        if (data == nullptr) throw std::invalid_argument("svg2png::PngData: Invalid SVG data");
        auto *document = static_cast<Document *>(data);
        const auto bitmap = document->renderToBitmap(width, height, background);
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

    namespace {
        std::unique_ptr<Document> parsed(const std::string_view svg_data) {
            const auto svg = utf8::strip_bom(svg_data);
            auto document = Document::loadFromData(svg.data(), svg.size());
            if (!document) throw std::invalid_argument("svg2png: Invalid SVG data");
            return document;
        }

        // 0xRRGGBBAA, as lunasvg takes a background.
        unsigned rgba(const color &c) {
            const auto channel = [](const double v) { return static_cast<unsigned>(std::lround(std::clamp(v, 0.0, 1.0) * 255)); };
            return channel(c.red()) << 24 | channel(c.green()) << 16 | channel(c.blue()) << 8 | channel(c.alpha());
        }
    }

    std::string svg2png::FromString(const std::string_view svg_data, const int width, const int height, const color &background) {
        const auto document = parsed(svg_data);
        return PngData(document.get(), width > 0 ? width : -1, height > 0 ? height : -1, rgba(background)); // -1: lunasvg keeps the aspect
    }

    std::string svg2png::FromFile(const std::string &filename, const int width, const int height, const color &background) {
        return FromString(file_get_contents(filename), width, height, background);
    }

    size svg2png::Size(const std::string_view svg_data) {
        const auto document = parsed(svg_data);
        return {document->width(), document->height()};
    }
}
