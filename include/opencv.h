#pragma once
#include <string>
#include <vector>
#include <sc.h>

namespace sc {
    namespace impl {
        struct internals;
    }

    class image {
    public:
        /// Creates an empty image.
        image();

        /// Loads an image from a file, throwing if the file cannot be opened.
        image(const std::string &filename);

        /// Creates an independent copy of another image.
        image(const image &copy_from);

        /// Transfers ownership of another image's storage.
        image(image &&move_from) noexcept;

        ~image();

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
        [[nodiscard]] point padding() const;

        /// Returns true when no image data is loaded.
        [[nodiscard]] bool empty() const;

        /// Saves the image and returns whether OpenCV reported success.
        bool save(const std::string &filename) const;

        /// Returns a resized copy of this image.
        image resized(size_i new_size) const;

        /// Resizes this image in place.
        void resize_to(size_i new_size);

        /// Returns a copy containing the specified rectangular area.
        image cropped(const rect_i &area) const;

        /// Draws text onto the image in place.
        void text(const std::string &label, point_i pos) const;

        /// Draws a circle onto the image in place.
        void circle(const point_i &pos, int radius) const;

        /// Copies 512 feature values into the image's feature buffer.
        // void setFeatures(const float *new_features);

        /// Generates a DNN blob using the image's current dimensions.
        /// The blob is replaced by later image processing.
        void generate_blob(double scale, double mean, bool swap_rb) const;

        /// Returns the mutable 512-value feature buffer.
        // const float *getFeatures() const { return features; }

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

        /// Draws a rectangle between two points in place.
        void rect(const point &left_top, const point &right_bottom) const;

        /// Draws a rectangle from an integer rectangle in place.
        void rect(const rect_i &box) const;

        /// Draws a rectangle from a floating-point rectangle in place.
        void rect(const sc::rect &box) const;

        /// Returns an affine-warped copy using five corresponding points.
        image warp(const std::array<point, 5> &map_from, const std::array<point, 5> &map_to,
                   size_i tosize = {112, 112}) const;

    private:
        impl::internals *impl;
        float features[512];
        size_i size_;
    };
} // sc
