#include <image.h>

#include <opencv2/videoio.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <sc_test.h>

using namespace std::chrono_literals;

namespace {
    // The grey level of frame n of the test video: 0, 8, 16, ...
    constexpr int frames = 30;
    constexpr double rate = 10;

    int grey_of(const int n) { return n * 8; }

    // How bright a frame is, 0 to 255, from its centre pixel.
    double brightness(const sc::image &picture) {
        auto pixel = picture.cropped(sc::rect_i{picture.size().width() / 2, picture.size().height() / 2, 1, 1});
        pixel.generate_blob(1.0, 0, true);
        return pixel.blob()[0];
    }
}

int main() {
    const auto scratch = std::filesystem::temp_directory_path() / "sc-image-video-test";
    std::filesystem::create_directories(scratch);
    const auto clip_file = scratch / "grey.avi";
    {
        // Motion JPEG in AVI: written by OpenCV itself, whatever backends it has.
        cv::VideoWriter writer{clip_file.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), rate, cv::Size{64, 48}};
        CHECK(writer.isOpened());
        for (int n = 0; n < frames; ++n) writer.write(cv::Mat{48, 64, CV_8UC3, cv::Scalar::all(grey_of(n))});
    }

    SECTION("What the file declares");
    {
        const sc::video clip{clip_file};
        CHECK_EQ(clip.size(), sc::size_i(64, 48));
        CHECK_EQ(clip.fps(), rate);
        CHECK_EQ(clip.frame_count(), std::size_t{frames});
        CHECK_EQ(clip.duration().count(), 3000LL);
    }

    SECTION("Every frame, in order, as images");
    {
        sc::video clip{clip_file};
        int count = 0;
        while (const auto frame = clip.next()) {
            CHECK_EQ(frame->number, std::size_t(count));
            CHECK_EQ(frame->time.count(), count * 100LL);
            CHECK_EQ(frame->picture.size(), sc::size_i(64, 48));
            CHECK_EQ(frame->picture.channels(), 3);
            CHECK_MSG(std::abs(brightness(frame->picture) - grey_of(count)) < 6, "frame " + std::to_string(count));
            ++count;
        }
        CHECK_EQ(count, frames);
        CHECK(!clip.next()); // still at the end
    }

    SECTION("A frame a second: the rest skipped");
    {
        sc::video clip{clip_file};
        std::vector<std::size_t> numbers;
        std::vector<long long> times;
        while (const auto frame = clip.next(1s)) {
            numbers.push_back(frame->number);
            times.push_back(frame->time.count());
        }
        CHECK_EQ(numbers, (std::vector<std::size_t>{0, 10, 20}));
        CHECK_EQ(times, (std::vector<long long>{0, 1000, 2000}));
    }

    SECTION("Moving a video");
    {
        sc::video first{clip_file};
        sc::video second{std::move(first)};
        const auto frame = second.next();
        CHECK(frame.has_value());
    }

    SECTION("Mistakes");
    {
        CHECK_THROWS_AS(sc::video{scratch / "no-such.mp4"}, std::runtime_error);
        const auto not_a_video = scratch / "text.mp4";
        std::ofstream{not_a_video} << "not a video";
        CHECK_THROWS_AS(sc::video{not_a_video}, std::runtime_error);
    }

    std::filesystem::remove_all(scratch);
    TEST_SUMMARY();
}
