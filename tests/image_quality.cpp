#include <image.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <string>
#include <vector>

#include <sc_test.h>

namespace {
    // A busy scene, as a road or a cab is: shapes of many greys and colours, edges everywhere.
    cv::Mat scene(const int seed) {
        cv::RNG random{static_cast<std::uint64_t>(seed)};
        cv::Mat picture{720, 1280, CV_8UC3, cv::Scalar{110, 120, 130}};
        for (int n = 0; n < 400; ++n) {
            const cv::Point corner{random.uniform(0, 1280), random.uniform(0, 720)};
            const cv::Scalar colour{double(random.uniform(0, 256)), double(random.uniform(0, 256)), double(random.uniform(0, 256))};
            if (n % 2) cv::rectangle(picture, corner, corner + cv::Point{random.uniform(10, 120), random.uniform(10, 120)}, colour, cv::FILLED);
            else cv::line(picture, corner, {random.uniform(0, 1280), random.uniform(0, 720)}, colour, random.uniform(1, 5));
        }
        return picture;
    }

    // An sc::image of these pixels, through PNG so only the public API is used.
    sc::image image_of(const cv::Mat &pixels) {
        std::vector<uchar> png;
        cv::imencode(".png", pixels, png);
        return sc::image{std::string{png.begin(), png.end()}};
    }

    sc::image_quality quality_of(const cv::Mat &pixels) { return sc::image_quality::of(image_of(pixels)); }

    std::string jpeg_of(const cv::Mat &pixels, const int quality = 90) {
        std::vector<uchar> bytes;
        cv::imencode(".jpg", pixels, bytes, {cv::IMWRITE_JPEG_QUALITY, quality});
        return {bytes.begin(), bytes.end()};
    }

    // The camera's date and time, white on the picture's top edge.
    void stamp(cv::Mat &picture) {
        cv::putText(picture, "2026-11-02 14:03:27  CH2  60 km/h", {20, 40}, cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar::all(255), 3);
    }
}

int main() {
    using enum sc::image_quality::verdict;
    const auto busy = scene(1);

    SECTION("A sharp, busy picture is ok");
    {
        const auto quality = quality_of(busy);
        CHECK_EQ(to_string(quality.outcome), std::string{"ok"});
        CHECK_EQ(quality.size, sc::size_i(1280, 720));
        CHECK(quality.reason.empty());
        CHECK_BETWEEN(quality.brightness, 60.0, 200.0);
        CHECK(quality.sharpness > 100);
        CHECK(quality.colourfulness > 20);
        CHECK_LT(quality.empty_share, 0.2);
    }

    SECTION("Out of focus is blurred, even with the camera's sharp date and time on it");
    {
        cv::Mat soft;
        cv::GaussianBlur(busy, soft, {0, 0}, 8);
        CHECK_EQ(to_string(quality_of(soft).outcome), std::string{"blurred"});
        stamp(soft);
        CHECK_EQ(to_string(quality_of(soft).outcome), std::string{"blurred"});
    }

    SECTION("One colour throughout is uniform");
    {
        CHECK_EQ(to_string(quality_of(cv::Mat{720, 1280, CV_8UC3, cv::Scalar::all(0)}).outcome), std::string{"uniform"});
        CHECK_EQ(to_string(quality_of(cv::Mat{720, 1280, CV_8UC3, cv::Scalar{0, 160, 0}}).outcome), std::string{"uniform"});
        CHECK_EQ(to_string(quality_of(cv::Mat{720, 1280, CV_8UC3, cv::Scalar{200, 60, 20}}).outcome), std::string{"uniform"});
    }

    SECTION("Most of the lens behind a hand is covered");
    {
        auto hand = busy.clone();
        cv::rectangle(hand, cv::Rect{0, 0, 1000, 720}, cv::Scalar{90, 110, 150}, cv::FILLED);
        const auto quality = quality_of(hand);
        CHECK_EQ(to_string(quality.outcome), std::string{"covered"});
        CHECK(quality.empty_share >= 0.7);
        // A quarter covered isn't yet.
        auto corner = busy.clone();
        cv::rectangle(corner, cv::Rect{0, 0, 640, 360}, cv::Scalar{90, 110, 150}, cv::FILLED);
        CHECK_EQ(to_string(quality_of(corner).outcome), std::string{"ok"});
    }

    SECTION("Dark and washed out");
    {
        CHECK_EQ(to_string(quality_of(busy * 0.2).outcome), std::string{"dark"});
        cv::Mat glare;
        busy.convertTo(glare, -1, 0.15, 225);
        CHECK_EQ(to_string(quality_of(glare).outcome), std::string{"bright"});
    }

    SECTION("Infrared at night is grey: no colourfulness");
    {
        cv::Mat grey, infrared;
        cv::cvtColor(busy, grey, cv::COLOR_BGR2GRAY);
        cv::cvtColor(grey, infrared, cv::COLOR_GRAY2BGR);
        const auto quality = quality_of(infrared);
        CHECK_LT(quality.colourfulness, 1.0);
        CHECK_EQ(to_string(quality.outcome), std::string{"ok"});
    }

    SECTION("Files: a JPEG cut short and bytes that aren't an image are corrupt");
    {
        const auto whole = jpeg_of(busy);
        CHECK_EQ(to_string(sc::image_quality::of_bytes(whole).outcome), std::string{"ok"});
        // Padded after its end marker, as dashcams do: still whole.
        CHECK_EQ(to_string(sc::image_quality::of_bytes(whole + std::string(16, '\0')).outcome), std::string{"ok"});
        CHECK_EQ(to_string(sc::image_quality::of_bytes(whole + "\x00\xdb\xd2\x93\xdc\x52" + std::string(8, '\xff')).outcome), std::string{"ok"});
        const auto cut = sc::image_quality::of_bytes(whole.substr(0, whole.size() * 6 / 10));
        CHECK_EQ(to_string(cut.outcome), std::string{"corrupt"});
        CHECK_EQ(cut.reason, std::string{"JPEG cut short (no end marker)"});
        const auto junk = sc::image_quality::of_bytes("this is no picture");
        CHECK_EQ(to_string(junk.outcome), std::string{"corrupt"});
        CHECK(!junk.reason.empty());
        CHECK_EQ(to_string(sc::image_quality::of_bytes("").outcome), std::string{"corrupt"});
        CHECK_EQ(to_string(sc::image_quality::of(std::filesystem::path{"/nonexistent/x.jpg"}).outcome), std::string{"corrupt"});
    }

    SECTION("Fingerprints: the same picture again, and another one");
    {
        const auto first = quality_of(busy).fingerprint;
        const auto again = sc::image_quality::of_bytes(jpeg_of(busy, 60)).fingerprint;
        CHECK_LT(sc::image_quality::differs(first, again), 5);
        CHECK(sc::image_quality::differs(first, quality_of(scene(2)).fingerprint) > 10);
        CHECK_EQ(sc::image_quality::differs(first, first), 0);
    }

    SECTION("Small images and limits");
    {
        const auto small = quality_of(busy(cv::Rect{0, 0, 3, 2}).clone());
        CHECK_EQ(small.size, sc::size_i(3, 2));
        sc::image_quality::limits limits;
        limits.grid_rows = 0;
        CHECK_THROWS_AS((void) sc::image_quality::of(image_of(busy), limits), std::invalid_argument);
        CHECK_EQ(to_string(sc::image_quality::of(sc::image{}).outcome), std::string{"corrupt"});
    }

    TEST_SUMMARY();
}
