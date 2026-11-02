#include "image_quality.h"
#include "internals.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace sc {
    namespace {
        image_quality corrupt(std::string reason) {
            image_quality quality;
            quality.reason = std::move(reason);
            return quality;
        }

        // The standard deviations of red - green and of yellow - blue, and their means.
        double colourfulness_of(const cv::Mat &bgr) {
            if (bgr.channels() != 3) return 0;
            cv::Mat wide;
            bgr.convertTo(wide, CV_32FC3);
            std::vector<cv::Mat> planes;
            cv::split(wide, planes);
            const cv::Mat red_green = planes[2] - planes[1];
            const cv::Mat yellow_blue = 0.5 * (planes[2] + planes[1]) - planes[0];
            cv::Scalar rg_mean, rg_spread, yb_mean, yb_spread;
            cv::meanStdDev(red_green, rg_mean, rg_spread);
            cv::meanStdDev(yellow_blue, yb_mean, yb_spread);
            return std::hypot(rg_spread[0], yb_spread[0]) + 0.3 * std::hypot(rg_mean[0], yb_mean[0]);
        }

        // 8 rows of 9 greys, each bit set where a grey is darker than the one to its right.
        std::uint64_t fingerprint_of(const cv::Mat &grey) {
            cv::Mat small;
            cv::resize(grey, small, cv::Size{9, 8}, 0, 0, cv::INTER_AREA);
            std::uint64_t bits = 0;
            for (int row = 0; row < 8; ++row)
                for (int column = 0; column < 8; ++column)
                    bits = bits << 1 | (small.at<uchar>(row, column) < small.at<uchar>(row, column + 1) ? 1u : 0u);
            return bits;
        }

        image_quality measure(const cv::Mat &picture, const image_quality::limits &given) {
            if (given.analysis_width < 16 || given.grid_columns < 1 || given.grid_rows < 1)
                throw std::invalid_argument{"image_quality: analysis_width must be 16 or more, the grid at least 1 x 1"};
            if (picture.empty()) return corrupt("empty image");

            image_quality quality;
            quality.size = {picture.cols, picture.rows};

            cv::Mat scaled = picture;
            if (picture.cols > given.analysis_width) {
                const int height = std::max(1, static_cast<int>(std::lround(double(picture.rows) * given.analysis_width / picture.cols)));
                cv::resize(picture, scaled, cv::Size{given.analysis_width, height}, 0, 0, cv::INTER_AREA);
            }
            cv::Mat grey;
            if (scaled.channels() == 3) cv::cvtColor(scaled, grey, cv::COLOR_BGR2GRAY);
            else if (scaled.channels() == 4) cv::cvtColor(scaled, grey, cv::COLOR_BGRA2GRAY);
            else grey = scaled;
            if (scaled.channels() == 4) cv::cvtColor(scaled, scaled, cv::COLOR_BGRA2BGR);

            cv::Scalar mean, spread;
            cv::meanStdDev(grey, mean, spread);
            quality.brightness = mean[0];
            quality.contrast = spread[0];
            quality.colourfulness = colourfulness_of(scaled);
            quality.fingerprint = fingerprint_of(grey);

            cv::Mat edges;
            cv::Laplacian(grey, edges, CV_64F);
            const int columns = std::min(given.grid_columns, grey.cols), rows = std::min(given.grid_rows, grey.rows);
            std::vector<double> cell_sharpness;
            int empty = 0;
            for (int row = 0; row < rows; ++row)
                for (int column = 0; column < columns; ++column) {
                    const int left = column * grey.cols / columns, top = row * grey.rows / rows;
                    const cv::Rect cell{left, top, (column + 1) * grey.cols / columns - left, (row + 1) * grey.rows / rows - top};
                    cv::meanStdDev(grey(cell), mean, spread);
                    // Dark and flat is night sky or shadow; a cover over the lens is lit (or makes it all uniform).
                    if (spread[0] < given.empty_cell_contrast && mean[0] >= given.dark_brightness) ++empty;
                    cv::meanStdDev(edges(cell), mean, spread);
                    cell_sharpness.push_back(spread[0] * spread[0]);
                }
            const auto middle = cell_sharpness.begin() + static_cast<std::ptrdiff_t>(cell_sharpness.size() / 2);
            std::ranges::nth_element(cell_sharpness, middle);
            quality.sharpness = *middle;
            quality.empty_share = double(empty) / double(cell_sharpness.size());

            using enum image_quality::verdict;
            if (quality.contrast < given.uniform_contrast) quality.outcome = uniform;
            else if (quality.brightness < given.dark_brightness) quality.outcome = dark;
            else if (quality.brightness > given.bright_brightness) quality.outcome = bright;
            else if (quality.empty_share >= given.covered_share) quality.outcome = covered;
            else if (quality.sharpness < given.blurred_sharpness) quality.outcome = blurred;
            else quality.outcome = ok;
            return quality;
        }

        // A JPEG ends with FF D9. Dashcams pad it after that (zeros, FF bytes, a few stray ones),
        // and in the compressed data FF D9 is never anything else, so it's looked for near the end.
        bool jpeg_complete(const std::string_view bytes) {
            const auto tail = bytes.substr(bytes.size() > 4096 ? bytes.size() - 4096 : 0);
            return tail.rfind("\xFF\xD9") != std::string_view::npos;
        }
    }

    image_quality image_quality::of(const image &picture, const limits &given) {
        return measure(picture.impl->image_mat, given);
    }

    image_quality image_quality::of_bytes(const std::string_view bytes, const limits &given) {
        if (bytes.empty()) return corrupt("empty file");
        const cv::Mat buffer{1, static_cast<int>(bytes.size()), CV_8UC1, const_cast<char *>(bytes.data())};
        cv::Mat decoded;
        try {
            decoded = cv::imdecode(buffer, cv::IMREAD_COLOR);
        } catch (const cv::Exception &) {
        }
        const bool jpeg = bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xD8;
        const bool cut_short = jpeg && !jpeg_complete(bytes);
        if (decoded.empty()) return corrupt(cut_short ? "JPEG cut short (no end marker)" : "not an image OpenCV can read");
        auto quality = measure(decoded, given);
        if (cut_short) {
            quality.outcome = verdict::corrupt;
            quality.reason = "JPEG cut short (no end marker)";
        }
        return quality;
    }

    image_quality image_quality::of(const std::filesystem::path &file, const limits &given) {
        std::ifstream in{file, std::ios::binary};
        if (!in) return corrupt("can't read " + file.string());
        const std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
        return of_bytes(bytes, given);
    }

    int image_quality::differs(const std::uint64_t one, const std::uint64_t other) {
        return std::popcount(one ^ other);
    }

    std::string to_string(const image_quality::verdict verdict) {
        switch (verdict) {
            using enum image_quality::verdict;
            case ok: return "ok";
            case corrupt: return "corrupt";
            case uniform: return "uniform";
            case covered: return "covered";
            case dark: return "dark";
            case bright: return "bright";
            case blurred: return "blurred";
        }
        return "unknown";
    }
}
