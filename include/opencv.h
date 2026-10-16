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
    /// draw(shape, appearance), the same as on an sc::pdf, in pixels. Points are rounded to whole
    /// pixels and lines antialiased. Colours are painted opaque (a partly transparent one as if
    /// solid; only the PDF engines honour alpha), as grey on a single-channel image. Text uses
    /// OpenCV's Hershey fonts: sans-serif (the default) or serif, bold from weight 600, italic, with
    /// font.size as the text's height in pixels; other families fall back to sans-serif. Text is
    /// measured (canvas::text_width() and the rest) in the same font.
    class image : public canvas {
    public:
        /// Creates an empty image.
        image();

        /// Loads an image from a file as three-channel BGR, throwing std::runtime_error if it can't
        /// be read. filename may instead be PNG data itself (as from svg2png).
        image(const std::string &filename);

        /// Creates an image from a contiguous RGB float blob in [1, 3, height, width] order, with
        /// values in [0, 1]. Throws std::invalid_argument on a null blob, a non-positive size or
        /// a value that isn't finite.
        [[nodiscard]] static image from_blob(const float *blob, int width, int height);

        /// Creates an image from a contiguous single-channel or RGB float blob in
        /// [1, channels, height, width] order; one channel gives a single-channel (grey) image.
        [[nodiscard]] static image from_blob(const float *blob, int width, int height, int channels);

        /// Inverts generate_blob() preprocessing: pixel = blob_value / scale + mean, clamped to
        /// 0..255. Pass the same scale, mean and swap_rb given to generate_blob().
        [[nodiscard]] static image from_blob(const float *blob, int width, int height,
                                             double scale, double mean, bool swap_rb);

        /// Inverts generate_blob() preprocessing for a single-channel or RGB blob.
        [[nodiscard]] static image from_blob(const float *blob, int width, int height, int channels,
                                             double scale, double mean, bool swap_rb);

        /// Creates an independent copy of another image, blob and padding included.
        image(const image &copy_from);

        /// Takes another image's storage; that one is left empty(), fit only to be assigned or destroyed.
        image(image &&move_from) noexcept;

        /// Call after changing the pixels: drops the padding and the blob, and rereads the size.
        void image_changed();

        ~image() override;

        /// Replaces this image with an independent copy of another image.
        image &operator=(const image &copy_from);

        /// Replaces this image by transferring storage from another image.
        image &operator=(image &&move_from) noexcept;

        /// Displays the image and returns false when Escape is pressed, in milliseconds: zero waits
        /// indefinitely, negative doesn't wait. Only on Apple platforms; elsewhere it returns true.
        bool show(int timeout = 0, const std::string &window_name = "Preview") const;

        /// In pixels, padding included.
        [[nodiscard]] size_i size() const;

        /// The picture inside the padding.
        [[nodiscard]] size_i cropped_size() const;

        /// The letterbox padding snap_to_size() added on each side; none otherwise.
        [[nodiscard]] size_i padding() const;

        [[nodiscard]] bool empty() const;

        /// 1 (grey) or 3 (BGR); 0 when empty.
        [[nodiscard]] int channels() const;

        /// Converts between grey (1) and BGR (3) in place, dropping the blob; throws on an empty image.
        void channels(int new_channels);

        /// Saves in the format the file's extension names; false if OpenCV couldn't.
        bool save(const std::string &filename) const;

        /// A resized copy; see resize_to().
        [[nodiscard]] image resized(size_i new_size) const;

        /// The outermost outlines of the non-zero (white) areas, each a polygon to draw with draw().
        /// A colour image is greyed first. method is OpenCV's ContourApproximationModes (2: simple).
        [[nodiscard]] std::vector<polygon_i> find_contours(int method = 2) const;

        /// Each find_contours() outline's smallest rotated rectangle, corners rounded to whole
        /// pixels; those with an area under minimum_area (pixels) are left out.
        [[nodiscard]] std::vector<rotated_rect> find_min_area_rects(double minimum_area = 0, int method = 2) const;

        /// Dilates in place with a rectangular kernel of size pixels.
        void dilate(size_i size = {5, 3});

        /// Both images joined horizontally; they must be non-empty, of equal height and channels.
        static image side_by_side(const image &left, const image &right);

        /// Resizes in place; a width or height of 0 or less follows from the other, keeping the
        /// aspect ratio (not both). Drops the padding and the blob.
        void resize_to(size_i new_size);

        /// A copy of area, which must lie within the image (std::invalid_argument otherwise).
        image cropped(const rect_i &area) const;

        /// Cuts out a rotated rectangle and turns it level, its width horizontal; throws if it lies
        /// outside the image.
        [[nodiscard]] image deskewed(const rotated_rect &area) const;

        /// Rotates clockwise in place by a multiple of 90 degrees (negative turns anticlockwise);
        /// other angles leave the pixels as they are.
        void rotate(int degrees);

        /// Generates a DNN blob of floats, [1, channels, height, width]: (pixel - mean) * scale,
        /// red and blue swapped if swap_rb. Anything that changes the image drops it.
        void generate_blob(double scale, double mean, bool swap_rb) const;

        /// Like cropped(), but unchecked: OpenCV throws if area isn't within the image.
        image crop(const rect_i &area) const;

        /// Crops in place to area, unchecked as crop().
        void crop_to(const rect_i &area);

        /// Removes the current letterbox padding in place.
        void crop();

        /// Rounds a value up to the next multiple of stride.
        static int snap_to_stride(int value, int stride);

        /// The size snap_to_size() pads to: the valid size nearest the image's that holds it (the
        /// last one if none does), or the image's own size without valid; then rounded up to a
        /// multiple of stride (0: not rounded).
        size_i get_snap_size(int stride = 32, const std::vector<size_i> &valid = {});

        /// Scales the image to fit get_snap_size(), keeping its aspect ratio, and pads it evenly with
        /// grey (114) to that size, in place; padding() then gives the padding on each side.
        void snap_to_size(int stride = 32, const std::vector<size_i> &valid = {});

        /// Returns the generated DNN blob data; throws if no blob exists.
        [[nodiscard]] float *blob() const;

        /// Returns the number of values in the generated DNN blob; throws if absent.
        [[nodiscard]] size_t blob_size() const;

        /// Returns the dimensions of the generated DNN blob; throws if absent.
        [[nodiscard]] const int64_t *blob_shape() const;

        /// Returns the number of dimensions in the blob shape; throws if absent.
        [[nodiscard]] size_t blob_shape_size() const;

        /// A copy warped (rotation, uniform scale and shift, fitted to five point pairs, as face
        /// landmarks) to tosize.
        image warp(const std::array<point, 5> &map_from, const std::array<point, 5> &map_to, size_i tosize = {112, 112}) const;

        /// Thresholds in place: values above threshold become 255, the rest 0, as 8-bit.
        void mask(double threshold = 0.5);

    protected:
        // An appearance's stroke and fill in their colour, alpha ignored; a stroke's width rounds to whole pixels, at
        // least 1. Dashes and other stroke and fill styles aren't drawn: plain outlines and flat fills.
        void paint_polygon(const polygon &shape, const sc::appearance &look) override;

        void paint_circle(const sc::circle &shape, const sc::appearance &look) override;

        void paint_line(const sc::line &shape, const sc::stroke &stroke) override;

        void paint_text(const sc::text &sample, const sc::fill &fill) override;

        // In pixels, for the Hershey font that draws it; they work on an empty image too.
        double measure_width(const sc::text &sample) override;

        double measure_x_height(const font &font) override;

        double measure_cap_height(const font &font) override;

        // All of a path's outlines filled together, curves as short lines. OpenCV fills by the
        // even-odd rule whatever the fill's rule: nested outlines are always holes.
        void paint_path(const path &shape, const sc::appearance &look) override;

    private:
        impl::internals *impl;
        float features[512];
        size_i size_;
    };
} // sc
