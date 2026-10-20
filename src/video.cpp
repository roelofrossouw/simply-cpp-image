#include "video.h"
#include "internals.h"

#include <opencv2/videoio.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace sc {
    namespace impl {
        struct video_internals {
            cv::VideoCapture capture;
            double fps = 0;
            std::size_t read = 0;                            // frames read so far
            std::optional<std::chrono::milliseconds> target; // when the next frame next(every) returns should be

            // Where frame number n is, by the frame rate (the container's own timestamps are
            // unreliable for some files and backends).
            [[nodiscard]] std::chrono::milliseconds time_of(const std::size_t n) const {
                if (fps <= 0) return std::chrono::milliseconds{static_cast<long long>(capture.get(cv::CAP_PROP_POS_MSEC))};
                return std::chrono::milliseconds{std::llround(static_cast<double>(n) * 1000.0 / fps)};
            }
        };
    }

    video::video(const std::filesystem::path &file) : impl(std::make_unique<impl::video_internals>()) {
        if (!std::filesystem::exists(file)) throw std::runtime_error("sc::video: no such file " + file.string());
        if (!impl->capture.open(file.string()) || !impl->capture.isOpened())
            throw std::runtime_error("sc::video: can't read " + file.string() + " as a video");
        impl->fps = impl->capture.get(cv::CAP_PROP_FPS);
        if (!std::isfinite(impl->fps) || impl->fps < 0) impl->fps = 0;
    }

    video::~video() = default;

    video::video(video &&) noexcept = default;

    video &video::operator=(video &&) noexcept = default;

    size_i video::size() const {
        return {static_cast<int>(impl->capture.get(cv::CAP_PROP_FRAME_WIDTH)), static_cast<int>(impl->capture.get(cv::CAP_PROP_FRAME_HEIGHT))};
    }

    double video::fps() const {
        return impl->fps;
    }

    std::size_t video::frame_count() const {
        const double count = impl->capture.get(cv::CAP_PROP_FRAME_COUNT);
        return std::isfinite(count) && count > 0 ? static_cast<std::size_t>(count) : 0;
    }

    std::chrono::milliseconds video::duration() const {
        if (impl->fps <= 0) return std::chrono::milliseconds{0};
        return std::chrono::milliseconds{std::llround(static_cast<double>(frame_count()) * 1000.0 / impl->fps)};
    }

    std::optional<video::frame> video::next() {
        return next(std::chrono::milliseconds{0});
    }

    std::optional<video::frame> video::next(const std::chrono::milliseconds every) {
        auto &in = *impl;
        // The frames are wanted at fixed times (the first frame's, then every after it), so a frame
        // rate that doesn't divide every doesn't make them drift: each is the first frame within
        // half a frame of its time. The others are grabbed without being decoded.
        const std::chrono::milliseconds half_frame{in.fps > 0 ? std::llround(500 / in.fps) : 0};
        while (true) {
            if (!in.capture.grab()) return std::nullopt;
            const std::size_t number = in.read++;
            const auto time = in.time_of(number);
            if (in.target && time + half_frame < *in.target) continue;
            cv::Mat decoded;
            if (!in.capture.retrieve(decoded) || decoded.empty()) return std::nullopt;
            frame out{image{}, number, time};
            out.picture.impl->image_mat = std::move(decoded);
            out.picture.size_ = {out.picture.impl->image_mat.cols, out.picture.impl->image_mat.rows};
            // The next time wanted; after a gap longer than every (or with every 0), from this frame.
            in.target = in.target ? *in.target + every : time + every;
            if (*in.target <= time) in.target = time + every;
            return out;
        }
    }
}
