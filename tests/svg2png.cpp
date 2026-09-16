#include <image.h>

#include <filesystem>
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

    SECTION("Unusable input is rejected");
    {
        // A file that is not there reaches the renderer as no data at all.
        CHECK_THROWS_AS(sc::svg2png::FromFile("resource/no-such-file.svg"), invalid_argument);
        CHECK_THROWS_AS(sc::svg2png::FromString(""), invalid_argument);
        CHECK_THROWS_AS(sc::svg2png::FromString("this is not svg"), invalid_argument);
        // Valid input must not throw.
        CHECK_NOTHROW(sc::svg2png::FromFile("resource/logo18.svg"));
    }

    error_code ignored;
    filesystem::remove_all(scratch, ignored);

    TEST_SUMMARY();
}
