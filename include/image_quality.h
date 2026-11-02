#pragma once

#include <opencv.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace sc {
    /// Where each of sc::image_quality's verdicts starts. Measured on the greyed, scaled picture, 0 to 255.
    struct image_quality_limits {
        int analysis_width = 640;        // pixels; narrower images aren't enlarged
        int grid_columns = 4;
        int grid_rows = 4;
        double uniform_contrast = 4;     // the whole picture's contrast below this is uniform
        double empty_cell_contrast = 8;  // a cell's contrast below this is empty, unless it's dark (sky, shadow)
        double covered_share = 0.5;      // this share of empty cells or more is covered
        double dark_brightness = 30;     // average brightness below this is dark
        double bright_brightness = 230;  // above this is bright
        double blurred_sharpness = 35;   // the cells' median sharpness below this is blurred
    };

    /// Whether a camera's picture is fit to look at: covered, blacked out, blurred, a solid colour
    /// or a broken file. Plain measurements (OpenCV, no model), a few milliseconds an image: the
    /// image is greyed and scaled to analysis_width first, so the numbers don't depend on its size.
    ///
    ///     const auto quality = sc::image_quality::of("snapshot.jpg");
    ///     if (quality.outcome != sc::image_quality::verdict::ok) std::cout << to_string(quality.outcome);
    ///
    /// The picture is cut into a grid of cells (4 x 4 by default), measured each on its own: a
    /// covered part of the lens shows as cells with nothing in them, and the camera's own date and
    /// time text, sharp as it is, lands in a cell or two without making a blurred picture look
    /// sharp (sharpness is the cells' median).
    ///
    /// A camera that sends the same picture again (frozen) shows in fingerprint: compare it with
    /// the camera's last one with differs(). Keeping that last one is the caller's part.
    struct image_quality {
        enum class verdict {
            ok,
            corrupt,  // not an image, or a JPEG cut short
            uniform,  // one colour throughout: blacked out, a blue or green screen
            covered,  // most of the cells empty: tape, a hand, a cloth
            dark,     // too dark to see much (night without infrared, or covered by something dark)
            bright,   // washed out (glare, the sun into the lens)
            blurred,  // nothing sharp anywhere: out of focus, fogged, a dirty lens
        };

        using limits = image_quality_limits;

        verdict outcome = verdict::corrupt;
        std::string reason;          // why it's corrupt; empty otherwise
        size_i size;                 // the image's own size in pixels (0 x 0 when corrupt)
        double brightness = 0;       // average grey level, 0 to 255
        double contrast = 0;         // the grey levels' standard deviation
        double sharpness = 0;        // the cells' median variance of the Laplacian (edges)
        double colourfulness = 0;    // Hasler and Süsstrunk's; under about 5 is grey (infrared at night)
        double empty_share = 0;      // the share of cells with nothing in them (and not dark), 0 to 1
        std::uint64_t fingerprint{}; // a difference hash of the picture: 64 bits, 8 x 8

        /// Measures a picture.
        [[nodiscard]] static image_quality of(const image &picture, const limits &given = {});

        /// Reads and measures a file; one that can't be read, or a JPEG without its end marker
        /// in its last 4 KB (cut short in transfer; padding after it is fine), is corrupt (with a reason), never an exception. A JPEG cut
        /// short is still measured as far as it decodes, where OpenCV's libjpeg decodes it at all.
        [[nodiscard]] static image_quality of(const std::filesystem::path &file, const limits &given = {});

        /// Measures an image file's bytes, as of(file) does.
        [[nodiscard]] static image_quality of_bytes(std::string_view bytes, const limits &given = {});

        /// How many of the fingerprints' 64 bits differ: 0 is the same picture, under about 5 a
        /// picture the eye can't tell apart (JPEG noise), over 10 another one. A parked vehicle's
        /// camera sees the same scene too, so a run of them means frozen only together with
        /// something that should have changed (the time shown, the vehicle moving).
        [[nodiscard]] static int differs(std::uint64_t one, std::uint64_t other);
    };

    [[nodiscard]] std::string to_string(image_quality::verdict verdict);
}
