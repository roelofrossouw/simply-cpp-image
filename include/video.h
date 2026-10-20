#pragma once

#include <opencv.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>

namespace sc {
    namespace impl {
        struct video_internals;
    }

    /// A video file read frame by frame (OpenCV's videoio, with whichever backend it was built with:
    /// FFmpeg on Ubuntu and Homebrew, so MP4, MOV, MKV, AVI and the rest). Each frame is an
    /// sc::image, three-channel BGR like a loaded photo, so it can be drawn on, saved or given to
    /// sc-ai's models.
    ///
    ///     sc::video clip{"phone.mp4"};
    ///     while (const auto frame = clip.next(std::chrono::seconds(1))) // a frame a second
    ///         frame->picture.save("frame-" + std::to_string(frame->number) + ".jpg");
    class video {
    public:
        /// A frame, where it is in the video.
        struct frame {
            sc::image picture;
            std::size_t number;             // counting from 0
            std::chrono::milliseconds time; // from the start
        };

        /// Opens the file; throws std::runtime_error when it can't be read as a video.
        explicit video(const std::filesystem::path &file);

        ~video();

        video(const video &) = delete;

        video &operator=(const video &) = delete;

        video(video &&) noexcept;

        video &operator=(video &&) noexcept;

        /// The frames' size in pixels.
        [[nodiscard]] size_i size() const;

        /// Frames a second, as the file declares them (0 when it doesn't).
        [[nodiscard]] double fps() const;

        /// How many frames, and how long, as the file declares (some containers only estimate).
        [[nodiscard]] std::size_t frame_count() const;

        [[nodiscard]] std::chrono::milliseconds duration() const;

        /// The next frame, or nothing at the end.
        [[nodiscard]] std::optional<frame> next();

        /// A frame every so often: the first frame first, then the one nearest (within half a frame)
        /// each time every after it, so they don't drift when the frame rate doesn't divide every.
        /// The ones between are skipped without being decoded: next(std::chrono::seconds(1)) gives a
        /// frame a second. Nothing at the end.
        [[nodiscard]] std::optional<frame> next(std::chrono::milliseconds every);

    private:
        std::unique_ptr<impl::video_internals> impl;
    };
}
