// image-quality: whether camera pictures are fit to look at (sc::image_quality): a line of numbers
// for each, and with --html a contact sheet, worst first, to set the limits by eye. --help lists
// the options.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <base64.h>
#include <params.h>

#include <image_quality.h>
#include <opencv.h>

namespace {
    struct measured {
        std::filesystem::path file;
        sc::image_quality quality;
        int previous_differs = -1; // from the picture before it in its folder; -1 for the first
        std::string thumbnail;     // a JPEG, base64
    };

    bool is_picture(const std::filesystem::path &file) {
        auto extension = file.extension().string();
        std::ranges::transform(extension, extension.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        static const std::set<std::string> types{".jpg", ".jpeg", ".png", ".webp", ".bmp", ".tif", ".tiff"};
        return types.contains(extension);
    }

    // The worst kinds first on the sheet.
    int rank(const sc::image_quality::verdict verdict) {
        using enum sc::image_quality::verdict;
        switch (verdict) {
            case corrupt: return 0;
            case uniform: return 1;
            case covered: return 2;
            case dark: return 3;
            case bright: return 4;
            case blurred: return 5;
            case ok: return 6;
        }
        return 7;
    }

    std::string fixed(const double value, const int decimals = 1) {
        char text[32];
        std::snprintf(text, sizeof text, "%.*f", decimals, value);
        return text;
    }

    std::string hex(const std::uint64_t value) {
        char text[20];
        std::snprintf(text, sizeof text, "%016llx", static_cast<unsigned long long>(value));
        return text;
    }

    std::string escaped(const std::string &text) {
        std::string out;
        for (const char c: text) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default: out += c;
            }
        }
        return out;
    }

    void write_sheet(const std::filesystem::path &output, std::vector<measured> pictures, const int same_bits) {
        std::ranges::stable_sort(pictures, [](const measured &a, const measured &b) {
            if (rank(a.quality.outcome) != rank(b.quality.outcome)) return rank(a.quality.outcome) < rank(b.quality.outcome);
            return a.quality.sharpness < b.quality.sharpness;
        });
        std::map<std::string, int> counts;
        for (const auto &picture: pictures) ++counts[to_string(picture.quality.outcome)];

        std::ostringstream html;
        html << R"(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Image quality</title><style>
body{font:13px system-ui,sans-serif;margin:16px;background:#f4f4f2;color:#222}
h1{font-size:18px;margin:0 0 8px}
.filters button{margin:0 6px 6px 0;padding:4px 10px;border:1px solid #bbb;border-radius:12px;background:#fff;cursor:pointer}
.filters button.on{background:#222;color:#fff;border-color:#222}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:10px}
.card{background:#fff;border-radius:6px;overflow:hidden;box-shadow:0 1px 2px #0002}
.card img{width:100%;display:block;background:#ddd;min-height:60px}
.card .info{padding:6px 8px}
.verdict{display:inline-block;padding:1px 8px;border-radius:9px;color:#fff;font-weight:600}
.ok{background:#2e7d32}.blurred{background:#ef6c00}.dark{background:#455a64}.bright{background:#c0a000}
.covered{background:#c62828}.uniform{background:#6a1b9a}.corrupt{background:#000}
.numbers{color:#555;font-variant-numeric:tabular-nums;margin-top:4px}
.file{color:#888;word-break:break-all;margin-top:2px;font-size:11px}
.same{color:#c62828;font-weight:600}
</style></head><body><h1>Image quality: )" << pictures.size() << R"( pictures, worst first</h1><div class="filters"><button class="on" data-v="">all</button>)";
        for (const auto &[verdict, count]: counts) html << "<button data-v=\"" << verdict << "\">" << verdict << " " << count << "</button>";
        html << R"(</div><div class="grid">)";
        for (const auto &picture: pictures) {
            const auto &q = picture.quality;
            const auto verdict = to_string(q.outcome);
            html << "<div class=\"card\" data-v=\"" << verdict << "\">";
            if (!picture.thumbnail.empty()) html << "<img loading=\"lazy\" src=\"data:image/jpeg;base64," << picture.thumbnail << "\">";
            html << "<div class=\"info\"><span class=\"verdict " << verdict << "\">" << verdict << "</span>";
            if (!q.reason.empty()) html << " " << escaped(q.reason);
            html << "<div class=\"numbers\">sharp " << fixed(q.sharpness) << " &middot; bright " << fixed(q.brightness) << " &middot; contrast "
                 << fixed(q.contrast) << "<br>colour " << fixed(q.colourfulness) << " &middot; empty " << fixed(q.empty_share * 100, 0) << "% &middot; "
                 << q.size.width() << "&times;" << q.size.height();
            if (picture.previous_differs >= 0)
                html << "<br><span class=\"" << (picture.previous_differs <= same_bits ? "same" : "") << "\">differs from the one before: "
                     << picture.previous_differs << " bits</span>";
            html << "</div><div class=\"file\">" << escaped(picture.file.string()) << "</div></div></div>\n";
        }
        html << R"(</div><script>
document.querySelectorAll('.filters button').forEach(b=>b.onclick=()=>{
document.querySelectorAll('.filters button').forEach(o=>o.classList.toggle('on',o===b));
document.querySelectorAll('.card').forEach(c=>c.style.display=!b.dataset.v||c.dataset.v===b.dataset.v?'':'none');});
</script></body></html>
)";
        std::ofstream out{output, std::ios::binary};
        out << html.str();
        if (!out) throw std::runtime_error{"can't write " + output.string()};
    }
}

int main(int argc, char **argv) {
    const sc::image_quality::limits defaults;
    const sc::params args{argc, argv, {
        {"name", "image-quality"},
        {"description", "Measures whether camera pictures are fit to look at: corrupt, uniform (one colour), covered, dark, "
                        "bright, blurred or ok. Prints a line per picture (tab separated: verdict, sharpness, brightness, "
                        "contrast, colourfulness, empty cells %, fingerprint, bits differing from the picture before it in its "
                        "folder, size, file). Folders are read through, their pictures by name. --html writes a contact sheet, "
                        "worst first. The limits are options, to try others."},
        {"options", {
            {"html", {{"value", "FILE"}, {"help", "write a contact sheet here"}}},
            {"thumbnail", {{"type", "integer"}, {"default", 320}, {"value", "PIXELS"}, {"help", "the contact sheet's picture width"}}},
            {"same", {{"type", "integer"}, {"default", 4}, {"value", "BITS"}, {"help", "fingerprints this close count as the same picture"}}},
            {"dark", {{"type", "number"}, {"default", defaults.dark_brightness}, {"help", "dark below this brightness"}}},
            {"bright", {{"type", "number"}, {"default", defaults.bright_brightness}, {"help", "bright above this brightness"}}},
            {"uniform", {{"type", "number"}, {"default", defaults.uniform_contrast}, {"help", "uniform below this contrast"}}},
            {"empty-cell", {{"type", "number"}, {"default", defaults.empty_cell_contrast}, {"help", "a cell is empty below this contrast"}}},
            {"covered", {{"type", "number"}, {"default", defaults.covered_share}, {"help", "covered from this share of empty cells"}}},
            {"blurred", {{"type", "number"}, {"default", defaults.blurred_sharpness}, {"help", "blurred below this sharpness"}}},
            {"grid", {{"type", "integer"}, {"default", defaults.grid_columns}, {"value", "CELLS"}, {"help", "cells across and down"}}}}},
        {"extra", {{"files", {{"required", true}, {"help", "pictures, or folders of them"}}}}}}};

    try {
        sc::image_quality::limits limits;
        limits.dark_brightness = args.as<double>("dark");
        limits.bright_brightness = args.as<double>("bright");
        limits.uniform_contrast = args.as<double>("uniform");
        limits.empty_cell_contrast = args.as<double>("empty-cell");
        limits.covered_share = args.as<double>("covered");
        limits.blurred_sharpness = args.as<double>("blurred");
        limits.grid_columns = limits.grid_rows = args.as<int>("grid");
        const std::string sheet = args.as<std::string>("html", "");
        const int thumbnail = args.as<int>("thumbnail");

        // Folders' pictures by name, so the one before is the same camera's last one when names sort by time.
        std::vector<std::filesystem::path> files;
        for (const auto &given: args["files"]) {
            const std::filesystem::path path = given.get<std::string>();
            if (std::filesystem::is_directory(path)) {
                std::vector<std::filesystem::path> found;
                for (const auto &entry: std::filesystem::recursive_directory_iterator{path})
                    if (entry.is_regular_file() && is_picture(entry.path())) found.push_back(entry.path());
                std::ranges::sort(found);
                files.insert(files.end(), found.begin(), found.end());
            } else files.push_back(path);
        }

        sc::image::log_level(sc::log_level::error);
        std::vector<measured> pictures;
        std::map<std::filesystem::path, std::uint64_t> last_in_folder;
        for (const auto &file: files) {
            measured picture{file, sc::image_quality::of(file, limits)};
            if (picture.quality.size.width() > 0) {
                const auto folder = file.parent_path();
                if (const auto last = last_in_folder.find(folder); last != last_in_folder.end())
                    picture.previous_differs = sc::image_quality::differs(last->second, picture.quality.fingerprint);
                last_in_folder[folder] = picture.quality.fingerprint;
                if (!sheet.empty()) {
                    try {
                        sc::image small{file.string()};
                        if (small.size().width() > thumbnail) small.resize_to({thumbnail, 0});
                        picture.thumbnail = sc::base64::encode(small.encoded("jpg", 70));
                    } catch (const std::exception &) {
                    }
                }
            }
            const auto &q = picture.quality;
            std::cout << to_string(q.outcome) << '\t' << fixed(q.sharpness) << '\t' << fixed(q.brightness) << '\t' << fixed(q.contrast) << '\t'
                      << fixed(q.colourfulness) << '\t' << fixed(q.empty_share * 100, 0) << '\t' << hex(q.fingerprint) << '\t'
                      << (picture.previous_differs < 0 ? std::string{"-"} : std::to_string(picture.previous_differs)) << '\t'
                      << q.size.width() << 'x' << q.size.height() << '\t' << file.string();
            if (!q.reason.empty()) std::cout << '\t' << q.reason;
            std::cout << '\n';
            pictures.push_back(std::move(picture));
        }
        if (!sheet.empty()) {
            write_sheet(sheet, std::move(pictures), args.as<int>("same"));
            std::cerr << "image-quality: wrote " << sheet << '\n';
        }
    } catch (const std::exception &error) {
        std::cerr << "image-quality: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
