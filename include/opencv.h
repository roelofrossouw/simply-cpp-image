#pragma once
#include <array>
#include <string>
#include <vector>
#include <sc.h>
#include <canvas.h>

namespace sc {
    namespace impl {
        struct internals;
    }

    /// An image (OpenCV), and an sc::canvas: shapes and text are drawn on it in place with
    /// canvas::draw(), the same as on an sc::pdf, in pixels. Points are rounded to whole pixels and
    /// lines antialiased; colours are written opaque, as grey on a single-channel image. Text uses
    /// OpenCV's Hershey fonts: sans-serif (the default) or serif, bold from weight 600, italic, with
    /// font.size as the text's height in pixels; other families fall back to sans-serif. Text is
    /// measured (canvas::text_width() and the rest) in the same font.
    class image : public canvas {
    public:
        /// Creates an empty image.
        image();

        /// Loads an image from a file, throwing if the file cannot be opened.
        image(const std::string &filename);

        /// Creates an image from a contiguous RGB float blob in [1, 3, height, width] order.
        /// Values are expected to be normalized to [0, 1].
        [[nodiscard]] static image from_blob(const float *blob, int width, int height);

        /// Creates an image from a contiguous single-channel or RGB float blob in
        /// [1, channels, height, width] order. Single-channel pixels are converted to grayscale.
        [[nodiscard]] static image from_blob(const float *blob, int width, int height, int channels);

        /// Inverts generate_blob() preprocessing: source_value = blob_value / scale + mean.
        /// Set swap_rb to the same value passed to generate_blob().
        [[nodiscard]] static image from_blob(const float *blob, int width, int height,
                                             double scale, double mean, bool swap_rb);

        /// Inverts generate_blob() preprocessing for a single-channel or RGB blob.
        [[nodiscard]] static image from_blob(const float *blob, int width, int height, int channels,
                                             double scale, double mean, bool swap_rb);

        /// Creates an independent copy of another image.
        image(const image &copy_from);

        /// Transfers ownership of another image's storage.
        image(image &&move_from) noexcept;

        /// Resets all metadata
        void image_changed();

        ~image() override;

        /// Replaces this image with an independent copy of another image.
        image &operator=(const image &copy_from);

        /// Replaces this image by transferring storage from another image.
        image &operator=(image &&move_from) noexcept;

        /// Displays the image and returns false when Escape is pressed.
        /// A timeout of zero waits indefinitely; negative values do not wait.
        bool show(int timeout = 0, const std::string &window_name = "Preview") const;

        /// Returns the current image dimensions.
        [[nodiscard]] size_i size() const;

        /// Returns the dimensions of the image content excluding padding.
        [[nodiscard]] size_i cropped_size() const;

        /// Returns the current letterbox padding around the image content.
        [[nodiscard]] size_i padding() const;

        /// Returns true when no image data is loaded.
        [[nodiscard]] bool empty() const;

        /// Returns the number of color channels in the image.
        [[nodiscard]] int channels() const;

        /// Converts the image between single-channel grayscale and three-channel BGR.
        void channels(int new_channels);

        /// Saves the image and returns whether OpenCV reported success.
        bool save(const std::string &filename) const;

        /// Returns a resized copy of this image.
        [[nodiscard]] image resized(size_i new_size) const;

        /// Finds external contours: each an outline, as a polygon (draw them with canvas::draw()).
        [[nodiscard]] std::vector<polygon_i> find_contours(int method = 2) const;

        /// Returns each external contour's minimum-area rotated rectangle.
        /// Rectangles with an area below minimum_area are omitted.
        [[nodiscard]] std::vector<rotated_rect> find_min_area_rects(double minimum_area = 0, int method = 2) const;

        void dilate(size_i size = {5, 3});

        /// Returns both non-empty images joined horizontally; their heights must match.
        static image side_by_side(const image &left, const image &right);

        /// Resizes this image in place.
        void resize_to(size_i new_size);

        /// Returns a copy containing the specified rectangular area.
        image cropped(const rect_i &area) const;

        /// Crops a rotated rectangle and rotates the result to make its width horizontal.
        [[nodiscard]] image deskewed(const rotated_rect &area) const;

        void rotate(int degrees);

        /// Copies 512 feature values into the image's feature buffer.
        // void setFeatures(const float *new_features);

        /// Generates a DNN blob using the image's current dimensions.
        /// The blob is replaced by later image processing.
        void generate_blob(double scale, double mean, bool swap_rb) const;

        /// Returns the mutable 512-value feature buffer.
        // const float *getFeatures() const { return features; }

        /// Get a cropped area from an image
        image crop(const rect_i &area) const;

        void crop_to(const rect_i &area);

        /// Removes the current letterbox padding in place.
        void crop();

        /// Rounds a value up to the next multiple of stride.
        static int snap_to_stride(int value, int stride);

        /// Calculates a stride-aligned size, optionally choosing from valid sizes.
        size_i get_snap_size(int stride = 32, const std::vector<size_i> &valid = {});

        /// Resizes and letterboxes the image to a calculated size in place.
        void snap_to_size(int stride = 32, const std::vector<size_i> &valid = {});

        /// Returns the generated DNN blob data; throws if no blob exists.
        [[nodiscard]] float *blob() const;

        /// Returns the number of values in the generated DNN blob; throws if absent.
        [[nodiscard]] size_t blob_size() const;

        /// Returns the dimensions of the generated DNN blob; throws if absent.
        [[nodiscard]] const int64_t *blob_shape() const;

        /// Returns the number of dimensions in the blob shape; throws if absent.
        [[nodiscard]] size_t blob_shape_size() const;

        /// Returns an affine-warped copy using five corresponding points.
        image warp(const std::array<point, 5> &map_from, const std::array<point, 5> &map_to, size_i tosize = {112, 112}) const;

        /// Convert the image to a binary mask image
        void mask(double threshold = 0.5);

    protected:
        // Strokes and fills in their colour (opaque); a stroke's width rounds to whole pixels, at
        // least 1. Dashes and other stroke and fill styles aren't drawn: plain outlines and flat fills.
        void paint_polygon(const polygon &shape, const sc::stroke &stroke, const sc::fill &fill) override;

        void paint_circle(const sc::circle &shape, const sc::stroke &stroke, const sc::fill &fill) override;

        void paint_line(const sc::line &shape, const sc::stroke &stroke) override;

        void paint_text(const sc::text &sample, const sc::fill &fill) override;

        // In pixels, for the Hershey font that draws it; they work on an empty image too. The
        // x-height is two thirds of the cap height, as Hershey fonts draw it.
        double measure_width(const sc::text &sample) override;

        double measure_x_height(const font &font) override;

        double measure_cap_height(const font &font) override;

        // All of a path's outlines filled together, curves as short lines. OpenCV fills by the
        // even-odd rule whatever the fill's rule: nested outlines are always holes.
        void paint_path(const path &shape, const sc::stroke &stroke, const sc::fill &fill) override;

    private:
        impl::internals *impl;
        float features[512];
        size_i size_;
    };
} // sc
