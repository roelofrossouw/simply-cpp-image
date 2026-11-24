#pragma once
#include <sc/core/log_level.h>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <sc/graphics/canvas.h>

namespace sc::image {
    namespace impl {
        struct internals;
    }

    /// What reading images writes to stderr. libjpeg's warnings about a damaged JPEG ("Corrupt
    /// JPEG data: premature end of data segment", the image still read as far as it goes) are
    /// warnings, written at warning (the default) and above: log_level(sc::log_level::error)
    /// keeps them quiet. On Linux, where OpenCV uses the system's libjpeg; elsewhere they're
    /// always written. OpenCV's own messages follow it too, and below warning so do FFmpeg's
    /// about videos it can't read ("moov atom not found"): set it before opening a video.
    void log_level(sc::log_level level);
    [[nodiscard]] sc::log_level log_level();

    /// An image (OpenCV), and an sc::graphics::canvas: shapes and text are drawn on it in place with
    /// draw(shape, appearance), the same as on an sc::pdflite::writer, in pixels. Points are rounded to whole
    /// pixels, lines antialiased, clip paths honoured. Colours are blended by their alpha (a gradient's stops'
    /// alphas aren't), as grey on a single-channel image. Text is drawn in the installed face font.face()
    /// finds (fontconfig's substitute for a family that isn't installed), from its glyph outlines, with
    /// font.size as the em in pixels, as on the other canvases, and measured (canvas::text_width() and
    /// the rest) in the same face. Only when no outline font is installed do OpenCV's Hershey fonts
    /// stand in: sans-serif or serif, bold from weight 600, italic, font.size as the text's height.
    class picture : public graphics::canvas {
    public:
        picture();

        /// Loads an image from a file as three-channel BGR, throwing std::runtime_error if it can't
        /// be read. filename may instead be PNG data itself (as from svg2png).
        picture(const std::string &filename);

        /// A blank three-channel image of this size, filled with colour (its alpha ignored).
        picture(size_i size, const color &colour);

        /// Creates an image from a contiguous RGB float blob in [1, 3, height, width] order, with
        /// values in [0, 1]. Throws std::invalid_argument on a null blob, a non-positive size or
        /// a value that isn't finite.
        [[nodiscard]] static picture from_blob(const float *blob, int width, int height);

        /// Creates an image from a contiguous single-channel or RGB float blob in
        /// [1, channels, height, width] order; one channel gives a single-channel (grey) image.
        [[nodiscard]] static picture from_blob(const float *blob, int width, int height, int channels);

        /// Inverts generate_blob() preprocessing: pixel = blob_value / scale + mean, clamped to
        /// 0..255. Pass the same scale, mean and swap_rb given to generate_blob().
        [[nodiscard]] static picture from_blob(const float *blob, int width, int height,
                                             double scale, double mean, bool swap_rb);

        /// Inverts generate_blob() preprocessing for a single-channel or RGB blob.
        [[nodiscard]] static picture from_blob(const float *blob, int width, int height, int channels,
                                             double scale, double mean, bool swap_rb);

        /// Creates an independent copy of another image, blob and padding included.
        picture(const picture &copy_from);

        /// Takes another image's storage; that one is left empty(), fit only to be assigned or destroyed.
        picture(picture &&move_from) noexcept;

        /// Call after changing the pixels: drops the padding and the blob, and rereads the size.
        void image_changed();

        ~picture() override;

        picture &operator=(const picture &copy_from);

        picture &operator=(picture &&move_from) noexcept;

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

        /// The image as a file's bytes: type is an extension ("png", "jpg", "webp", "bmp", "tiff"),
        /// quality 1 to 100 for JPEG and WebP. Throws std::runtime_error when OpenCV can't encode it.
        [[nodiscard]] std::string encoded(const std::string &type = "png", int quality = 95) const;

        /// Saves in the format the file's extension names; false if OpenCV couldn't.
        bool save(const std::string &filename) const;

        /// Saved with this quality, 1 to 100, where the format has one (JPEG, WebP); the others
        /// ignore it.
        bool save(const std::string &filename, int quality) const;

        /// A resized copy; see resize_to().
        [[nodiscard]] picture resized(size_i new_size) const;

        /// The outermost outlines of the non-zero (white) areas, each a polygon to draw with draw().
        /// A colour image is greyed first. method is OpenCV's ContourApproximationModes (2: simple).
        [[nodiscard]] std::vector<polygon_i> find_contours(int method = 2) const;

        /// Each find_contours() outline's smallest rotated rectangle, corners rounded to whole
        /// pixels; those with an area under minimum_area (pixels) are left out.
        [[nodiscard]] std::vector<rotated_rect> find_min_area_rects(double minimum_area = 0, int method = 2) const;

        /// Both images joined horizontally; they must be non-empty, of equal height and channels.
        static picture side_by_side(const picture &left, const picture &right);

        /// Resizes in place; a width or height of 0 or less follows from the other, keeping the
        /// aspect ratio (not both). Drops the padding and the blob.
        void resize_to(size_i new_size);

        /// A copy of area, which must lie within the image (std::invalid_argument otherwise).
        picture cropped(const rect_i &area) const;

        /// Cuts out a rotated rectangle and turns it level, its width horizontal; throws if it lies
        /// outside the image.
        [[nodiscard]] picture deskewed(const rotated_rect &area) const;

        /// Rotates clockwise in place by a multiple of 90 degrees (negative turns anticlockwise);
        /// other angles leave the pixels as they are.
        void rotate(int degrees);

        /// Generates a DNN blob of floats, [1, channels, height, width]: (pixel - mean) * scale,
        /// red and blue swapped if swap_rb. Anything that changes the image drops it.
        void generate_blob(double scale, double mean, bool swap_rb) const;

        /// Like cropped(), but unchecked: OpenCV throws if area isn't within the image.
        picture crop(const rect_i &area) const;

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

        /// The generate_blob() blob; these throw std::runtime_error when there is none.
        [[nodiscard]] float *blob() const;

        [[nodiscard]] size_t blob_size() const;

        [[nodiscard]] const int64_t *blob_shape() const;

        [[nodiscard]] size_t blob_shape_size() const;

        /// A copy warped (rotation, uniform scale and shift, fitted to five point pairs, as face
        /// landmarks) to tosize.
        picture warp(const std::array<point, 5> &map_from, const std::array<point, 5> &map_to, size_i tosize = {112, 112}) const;

        /// Thresholds in place: values above threshold become 255, the rest 0, as 8-bit.
        void mask(double threshold = 0.5);

    protected:
        // An appearance's stroke and fill in their colour, blended by its alpha (a gradient's stops' alphas are
        // ignored); text in its font's own glyph outlines (OpenCV's Hershey font only when no font is installed);
        // a stroke's width rounds to whole pixels, at
        // least 1. Dashes and other stroke and fill styles aren't drawn: plain outlines and flat fills.
        //
        // A linear gradient fill is drawn too: each pixel the gradient's colour at its centre.
        //
        // TODO: radial gradients and gradient strokes, masks (by luminance; SVG masks are read as
        // clips), and a gradient's stops' alphas.
        void paint_polygon(const polygon &shape, const sc::graphics::appearance &look) override;

        void paint_circle(const sc::circle &shape, const sc::graphics::appearance &look) override;

        void paint_line(const sc::line &shape, const sc::graphics::stroke &stroke) override;

        void paint_text(const sc::graphics::text &sample, const sc::graphics::fill &fill) override;

        // Decoded and resampled into place (bilinear), its alpha honoured with smooth edges.
        // Throws std::runtime_error when OpenCV can't decode it.
        void paint_bitmap(const graphics::bitmap &picture, const transform &placement) override;

        // In pixels, in the face that draws it (Hershey without one); they work on an empty image too.
        double measure_width(const sc::graphics::text &sample) override;

        double measure_x_height(const graphics::font &font) override;

        double measure_cap_height(const graphics::font &font) override;

        // All of a path's outlines filled together, curves as short lines, by the fill's rule: even-odd
        // (every crossing toggles), or nonzero (outlines add up by their direction; one wound the
        // other way cuts a hole).
        void paint_path(const path &shape, const sc::graphics::appearance &look) override;

    public:
        // Clipped to inside shape's outlines (each filled on its own, so overlapping ones add up,
        // but a hole cut by one inside another isn't), with a smooth edge; clips nest.
        void push_clip(const path &shape) override;

        void pop_clip() override;

    private:
        friend class video; // makes its frames
        friend struct quality; // measures its pixels

        std::unique_ptr<impl::internals> impl;
        size_i size_;
    };
} // sc
