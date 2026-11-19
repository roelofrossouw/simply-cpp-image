#include <sc/image/image.h>
#include <sc/core/core.h>

#include <filesystem>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <stdexcept>
#include <string>

#include <sc_test.h>

using namespace std;

namespace {
    // The eight bytes every PNG starts with.
    const string png_signature{"\x89PNG\r\n\x1A\n", 8};

    void check_is_png(const string &data, const string &what) {
        CHECK_MSG(data.size() > 8, what + " is only " + to_string(data.size()) + " bytes");
        if (data.size() <= 8) return;
        CHECK_EQ(data.substr(0, 8), png_signature);
        // The header chunk comes first and the end chunk last.
        CHECK_EQ(data.substr(12, 4), string{"IHDR"});
        CHECK_MSG(data.find("IEND") != string::npos, what + " has no IEND chunk");
    }
}

int main() {
    const auto scratch = filesystem::temp_directory_path() / "sc-image-svg2png-test";
    filesystem::create_directories(scratch);

    SECTION("Rendering a file");
    {
        const auto png = sc::svg2png::FromFile("resource/logo18.svg");
        check_is_png(png, "logo18.svg");
    }

    SECTION("Rendering a string");
    {
        const auto png = sc::svg2png::FromString(
            R"(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20"><rect width="40" height="20" fill="red"/></svg>)");
        check_is_png(png, "inline svg");

        // The rendered png is a real image of the size the svg asked for.
        const auto path = (scratch / "inline.png").string();
        sc::file_put_contents(path, png);
        const sc::image rendered{path};
        CHECK(!rendered.empty());
        CHECK_EQ(rendered.size(), sc::size_i(40, 20));
    }

    SECTION("The svg dimensions carry through to the png");
    {
        const auto png = sc::svg2png::FromString(
            R"(<svg xmlns="http://www.w3.org/2000/svg" width="17" height="33"><circle cx="8" cy="16" r="8"/></svg>)");
        const auto path = (scratch / "odd.png").string();
        sc::file_put_contents(path, png);
        const sc::image rendered{path};
        CHECK_EQ(rendered.size(), sc::size_i(17, 33));
    }

    SECTION("A size of its own, the aspect ratio kept, a background");
    {
        const string svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20"><circle cx="10" cy="10" r="5"/></svg>)";
        CHECK(sc::svg2png::Size(svg) == (sc::size{40, 20}));
        const auto read = [&](const string &png, const string &name) {
            const auto path = (scratch / name).string();
            sc::file_put_contents(path, png);
            return sc::image{path};
        };
        CHECK_EQ(read(sc::svg2png::FromString(svg, 100, 30, sc::color::Transparent), "both.png").size(), sc::size_i(100, 30));
        CHECK_EQ(read(sc::svg2png::FromString(svg, 80, 0, sc::color::Transparent), "wide.png").size(), sc::size_i(80, 40)); // aspect kept
        CHECK_EQ(read(sc::svg2png::FromString(svg, 0, 10, sc::color::White), "low.png").size(), sc::size_i(20, 10));

        // A white background, saved as JPEG at a quality.
        const auto jpeg = (scratch / "white.jpg").string();
        CHECK(read(sc::svg2png::FromString(svg, 0, 0, sc::color::White), "white.png").save(jpeg, 80));
        CHECK_EQ(sc::image{jpeg}.size(), sc::size_i(40, 20));
    }

    SECTION("Text is drawn in the font it's measured with, so text-anchor lines up");
    {
        // The repro from sc-webframe's chart labels: end, start and middle anchored at x = 100 (200 px).
        const string svg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 60" width="200" height="60" font-family="Arial, Helvetica, sans-serif" font-size="11"><text x="100" y="15" text-anchor="end">R200m end</text><text x="100" y="35">R200m start</text><text x="100" y="55" text-anchor="middle">mid</text></svg>)";
        const auto png = sc::svg2png::FromString(svg, 400, 0, sc::color::White);
        const cv::Mat grey = cv::imdecode(vector<uchar>(png.begin(), png.end()), cv::IMREAD_GRAYSCALE);
        CHECK_EQ(grey.cols, 400);
        // The ink's leftmost and rightmost columns in a band of rows (svg y from..to).
        const auto ink = [&](const int from, const int to) {
            int left = grey.cols, right = -1;
            for (int y = from * 2; y < to * 2 && y < grey.rows; ++y)
                for (int x = 0; x < grey.cols; ++x)
                    if (grey.at<uchar>(y, x) < 128) {
                        left = min(left, x);
                        right = max(right, x);
                    }
            return pair{left, right};
        };
        const auto [end_left, end_right] = ink(2, 20);
        CHECK_MSG(end_right >= 192 && end_right <= 202, "end-anchored text ends at " + to_string(end_right) + " px, not about 200");
        const auto [start_left, start_right] = ink(22, 40);
        CHECK_MSG(start_left >= 198 && start_left <= 206, "start-anchored text starts at " + to_string(start_left) + " px, not about 200");
        const auto [mid_left, mid_right] = ink(42, 60);
        const int middle = (mid_left + mid_right) / 2;
        CHECK_MSG(middle >= 194 && middle <= 206, "middle-anchored text centres on " + to_string(middle) + " px, not about 200");
    }

    SECTION("Stroke and fill opacity, also inherited from a group");
    {
        const string svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20"><g stroke="#888" stroke-opacity="0.2" stroke-width="4"><line x1="0" y1="5" x2="40" y2="5"/></g><g fill="#000" fill-opacity="0.5"><rect x="0" y="10" width="40" height="10"/></g></svg>)";
        const auto png = sc::svg2png::FromString(svg, 0, 0, sc::color::White);
        const cv::Mat grey = cv::imdecode(vector<uchar>(png.begin(), png.end()), cv::IMREAD_GRAYSCALE);
        const int line = grey.at<uchar>(5, 20), rect = grey.at<uchar>(15, 20);
        // White under 20% of #888 (136) is about 231; under 50% black about 128.
        CHECK_MSG(line >= 224 && line <= 238, "a 0.2 stroke-opacity line is " + to_string(line) + ", not about 231");
        CHECK_MSG(rect >= 120 && rect <= 136, "a 0.5 fill-opacity rect is " + to_string(rect) + ", not about 128");
    }

    SECTION("Unusable input is rejected");
    {
        // A file that is not there reaches the renderer as no data at all.
        CHECK_THROWS_AS(sc::svg2png::FromFile("resource/no-such-file.svg"), invalid_argument);
        CHECK_THROWS_AS(sc::svg2png::FromString(""), invalid_argument);
        CHECK_THROWS_AS(sc::svg2png::FromString("this is not svg"), invalid_argument);
        CHECK_THROWS_AS(sc::svg2png::Size("this is not svg"), invalid_argument);
        // Valid input must not throw.
        CHECK_NOTHROW(sc::svg2png::FromFile("resource/logo18.svg"));
    }

    error_code ignored;
    filesystem::remove_all(scratch, ignored);

    TEST_SUMMARY();
}
