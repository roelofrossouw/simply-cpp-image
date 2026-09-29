#include "opencv.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <span>
#include <utility>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>

namespace {
    cv::Point cvpoint(const sc::point_i &rhs) {
        return {rhs.x(), rhs.y()};
    }

    cv::Point2d cvpoint(const sc::point &rhs) {
        return {rhs.x(), rhs.y()};
    }

    cv::Size cvsize(const sc::size_i &rhs) {
        return {rhs.width(), rhs.height()};
    }

    cv::Size2d cvsize(const sc::size &rhs) {
        return {rhs.width(), rhs.height()};
    }
}

namespace sc {
    namespace impl {
        struct internals {
            cv::Mat image_mat;
            cv::Mat blob_mat;
            std::span<float> blob_data;
            std::vector<int64_t> blob_shape;
            cv::Size padding;
        };
    }

    image::image() : impl(new impl::internals()) {
    }

    image::image(const std::string &filename) : impl(new impl::internals()) {
        const std::string png_signature{"\x89PNG\r\n\x1A\n", 8};
        if (filename.substr(0, 8) == png_signature) {
            std::vector<uchar> buffer(filename.begin(), filename.end());
            impl->image_mat = cv::imdecode(buffer, cv::ImreadModes::IMREAD_COLOR);
        } else {
            impl->image_mat = cv::imread(filename);
        }
        if (impl->image_mat.empty()) throw std::runtime_error{"Could not open image " + filename};
        size_ = {impl->image_mat.cols, impl->image_mat.rows};
    }

    image image::from_blob(const float *blob, const int width, const int height) {
        return from_blob(blob, width, height, 3);
    }

    image image::from_blob(const float *blob, const int width, const int height, const int channels) {
        return from_blob(blob, width, height, channels, 1.0 / 255.0, 0.0, true);
    }

    image image::from_blob(const float *blob, const int width, const int height,
                           const double scale, const double mean, const bool swap_rb) {
        return from_blob(blob, width, height, 3, scale, mean, swap_rb);
    }

    image image::from_blob(const float *blob, const int width, const int height, const int channels,
                           const double scale, const double mean, const bool swap_rb) {
        if (!blob) throw std::invalid_argument{"Image blob cannot be null"};
        if (width <= 0 || height <= 0) throw std::invalid_argument{"Image blob dimensions must be positive"};
        if (channels != 1 && channels != 3)
            throw std::invalid_argument{"Image blob must have one or three channels"};
        if (!std::isfinite(scale) || scale == 0.0 || !std::isfinite(mean))
            throw std::invalid_argument{"Image blob scale must be finite and non-zero, and mean must be finite"};

        image result;
        result.impl->image_mat.create(height, width, channels == 1 ? CV_8UC1 : CV_8UC3);
        const size_t pixel_count = static_cast<size_t>(width) * height;
        if (channels == 1) {
            auto *pixels = result.impl->image_mat.ptr<uchar>();
            for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
                const float value = blob[pixel];
                if (!std::isfinite(value))
                    throw std::invalid_argument{"Image blob values must be finite"};
                const double pixel_value = std::clamp(static_cast<double>(value) / scale + mean, 0.0, 255.0);
                pixels[pixel] = cv::saturate_cast<uchar>(pixel_value);
            }
        } else {
            for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
                auto &bgr = result.impl->image_mat.ptr<cv::Vec3b>()[pixel];
                for (int channel = 0; channel < 3; ++channel) {
                    const float value = blob[static_cast<size_t>(channel) * pixel_count + pixel];
                    if (!std::isfinite(value))
                        throw std::invalid_argument{"Image blob values must be finite"};
                    const double pixel_value = std::clamp(static_cast<double>(value) / scale + mean, 0.0, 255.0);
                    const auto channel_value = cv::saturate_cast<uchar>(pixel_value);
                    const auto bgr_channel = swap_rb ? 2 - channel : channel;
                    bgr[bgr_channel] = channel_value;
                }
            }
        }
        result.size_ = {width, height};
        return result;
    }

    image::image(const image &copy_from) : impl(new impl::internals()) {
        impl->image_mat = copy_from.impl->image_mat.clone();
        impl->blob_mat = copy_from.impl->blob_mat.clone();
        impl->blob_shape = copy_from.impl->blob_shape;
        impl->blob_data = impl->blob_mat.empty()
                              ? std::span<float>{}
                              : std::span<float>{impl->blob_mat.ptr<float>(), impl->blob_mat.total()};
        impl->padding = copy_from.impl->padding;
        size_ = copy_from.size_;
        std::memcpy(features, copy_from.features, sizeof(features));
    }

    image::image(image &&move_from) noexcept
        : impl(move_from.impl), features{}, size_(move_from.size_) {
        std::memcpy(features, move_from.features, sizeof(features));
        move_from.impl = nullptr;
        move_from.size_ = {};
    }

    image &image::operator=(const image &copy_from) {
        if (this == &copy_from) return *this;
        image copy{copy_from};
        std::swap(impl, copy.impl);
        std::swap(size_, copy.size_);
        std::memcpy(features, copy.features, sizeof(features));
        return *this;
    }

    image &image::operator=(image &&move_from) noexcept {
        if (this == &move_from) return *this;
        delete impl;
        impl = move_from.impl;
        size_ = move_from.size_;
        std::memcpy(features, move_from.features, sizeof(features));
        move_from.impl = nullptr;
        move_from.size_ = {};
        return *this;
    }

    void image::image_changed() {
        impl->padding = {0, 0};
        impl->blob_mat.release();
        impl->blob_data = {};
        impl->blob_shape.clear();
        size_ = {impl->image_mat.cols, impl->image_mat.rows};
    }

    image::~image() {
        delete impl;
    }

    bool image::show(int timeout, const std::string &window_name) const {
#ifdef __APPLE__
        cv::imshow(window_name, impl->image_mat);
        if (timeout >= 0) return cv::waitKey(timeout) != 27;
#endif
        return true;
    }

    size_i image::size() const {
        return size_;
    }

    size_i image::cropped_size() const {
        return size_ - padding() * 2;
    }

    point image::padding() const {
        return {impl->padding.width, impl->padding.height};
    }

    bool image::empty() const {
        return !impl || impl->image_mat.empty();
    }

    bool image::save(const std::string &filename) const {
        return cv::imwrite(filename, impl->image_mat);
    }

    image image::resized(const size_i new_size) const {
        image result{*this};
        result.resize_to(new_size);
        return result;
    }

    std::vector<std::vector<point_i> > image::find_contours(const int method) const {
        std::vector<std::vector<cv::Point> > cv_contours;
        cv::Mat input;
        if (impl->image_mat.channels() == 1) input = impl->image_mat.clone();
        else cv::cvtColor(impl->image_mat, input, cv::COLOR_BGR2GRAY);
        cv::findContours(input, cv_contours, cv::RETR_EXTERNAL, method);
        std::vector<std::vector<point_i> > contours;
        contours.reserve(cv_contours.size());
        for (const auto &cv_contour: cv_contours) {
            auto &contour = contours.emplace_back();
            contour.reserve(cv_contour.size());
            for (const auto &point: cv_contour) contour.emplace_back(point.x, point.y);
        }
        return contours;
    }

    std::vector<rotated_rect> image::find_min_area_rects(const double minimum_area, const int method) const {
        if (!std::isfinite(minimum_area) || minimum_area < 0)
            throw std::invalid_argument{"Minimum contour rectangle area must be finite and non-negative"};
        const auto contours = find_contours(method);
        std::vector<rotated_rect> rectangles;
        rectangles.reserve(contours.size());
        for (const auto &contour: contours) {
            if (contour.empty()) continue;
            std::vector<cv::Point> cv_contour;
            cv_contour.reserve(contour.size());
            for (const auto &point: contour) cv_contour.push_back(cvpoint(point));

            const cv::RotatedRect rectangle = cv::minAreaRect(cv_contour);
            if (static_cast<double>(rectangle.size.width) * rectangle.size.height < minimum_area) continue;
            cv::Point2f vertices[4];
            rectangle.points(vertices);
            std::vector<point_i> corners;
            corners.reserve(4);
            for (const auto &vertex: vertices)
                corners.emplace_back(cvRound(vertex.x), cvRound(vertex.y));
            rectangles.emplace_back(static_cast<rotated_rect>(polygon{std::move(corners)}));
        }
        return rectangles;
    }

    void image::dilate(size_i size) {
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cvsize(size));
        cv::dilate(impl->image_mat, impl->image_mat, kernel);
    }

    image image::side_by_side(const image &left, const image &right) {
        if (left.empty() || right.empty())
            throw std::invalid_argument{"Cannot join empty images"};
        if (left.size_.height() != right.size_.height())
            throw std::invalid_argument{"Images must have equal heights"};
        image result;
        cv::hconcat(left.impl->image_mat, right.impl->image_mat, result.impl->image_mat);
        result.size_ = {result.impl->image_mat.cols, result.impl->image_mat.rows};
        return result;
    }

    void image::resize_to(size_i new_size) {
        if (new_size.width() <= 0 && new_size.height() <= 0) throw std::invalid_argument{"Image size must be positive"};
        if (new_size.width() <= 0) new_size.width(new_size.height() * size_.width() / size_.height());
        if (new_size.height() <= 0) new_size.height(new_size.width() * size_.height() / size_.width());
        cv::resize(impl->image_mat, impl->image_mat, cvsize(new_size), 0, 0, cv::INTER_LINEAR);
        image_changed();
    }

    image image::cropped(const rect_i &area) const {
        const rect_i bounds{0, 0, size_.width(), size_.height()};
        if (area.left() < bounds.left() || area.top() < bounds.top()
            || area.right() > bounds.right() || area.bottom() > bounds.bottom()
            || area.width() <= 0 || area.height() <= 0)
            throw std::invalid_argument{"Crop area is outside the image"};

        image result;
        result.impl->image_mat = impl->image_mat(
            cv::Rect(area.left(), area.top(), area.width(), area.height())).clone();
        result.size_ = {area.width(), area.height()};
        return result;
    }

    image image::deskewed(const rotated_rect &bounds) const {
        if (empty()) throw std::invalid_argument{"Cannot crop an empty image"};
        if (!std::isfinite(bounds.width()) || !std::isfinite(bounds.height()) ||
            !std::isfinite(bounds.angle()) || bounds.width() <= 0 || bounds.height() <= 0)
            throw std::invalid_argument{"Rotated rectangle must have finite positive dimensions and angle"};

        const sc::rect bounding_box = static_cast<sc::rect>(bounds);
        if (!std::isfinite(bounding_box.left()) || !std::isfinite(bounding_box.top()) ||
            !std::isfinite(bounding_box.right()) || !std::isfinite(bounding_box.bottom()))
            throw std::invalid_argument{"Rotated rectangle bounds must be finite"};
        const int left = static_cast<int>(std::clamp(std::floor(bounding_box.left()), 0.0,
                                                     static_cast<double>(size_.width())));
        const int top = static_cast<int>(std::clamp(std::floor(bounding_box.top()), 0.0,
                                                    static_cast<double>(size_.height())));
        const int right = static_cast<int>(std::clamp(std::ceil(bounding_box.right()), 0.0,
                                                      static_cast<double>(size_.width())));
        const int bottom = static_cast<int>(std::clamp(std::ceil(bounding_box.bottom()), 0.0,
                                                       static_cast<double>(size_.height())));
        if (right <= left || bottom <= top) throw std::invalid_argument{"Rotated rectangle is outside the image"};
        const cv::Rect source_bounds{left, top, right - left, bottom - top};
        cv::Mat source_crop = impl->image_mat(source_bounds);
        const cv::Point2f crop_center{
            static_cast<float>(bounds.center().x() - source_bounds.x),
            static_cast<float>(bounds.center().y() - source_bounds.y)
        };
        cv::Mat transform = cv::getRotationMatrix2D(crop_center, bounds.angle(), 1.0);
        const double radians = bounds.angle() * std::acos(-1.0) / 180.0;
        const int rotated_width = std::max(1, cvRound(source_crop.cols * std::abs(std::cos(radians)) +
                                                      source_crop.rows * std::abs(std::sin(radians))));
        const int rotated_height = std::max(1, cvRound(source_crop.rows * std::abs(std::cos(radians)) +
                                                       source_crop.cols * std::abs(std::sin(radians))));
        transform.at<double>(0, 2) += (rotated_width - source_crop.cols) / 2.0;
        transform.at<double>(1, 2) += (rotated_height - source_crop.rows) / 2.0;
        cv::Mat rotated;
        cv::warpAffine(source_crop, rotated, transform, {rotated_width, rotated_height});

        const int crop_width = std::max(1, cvRound(bounds.width()));
        const int crop_height = std::max(1, cvRound(bounds.height()));
        const cv::Point output_center{rotated.cols / 2, rotated.rows / 2};
        const cv::Rect crop_bounds{
            output_center.x - crop_width / 2,
            output_center.y - crop_height / 2,
            crop_width,
            crop_height
        };
        const cv::Rect valid_crop = crop_bounds & cv::Rect{0, 0, rotated.cols, rotated.rows};
        if (valid_crop.empty()) throw std::invalid_argument{"Rotated polygon is outside the image"};

        image result;
        result.impl->image_mat = rotated(valid_crop).clone();
        result.size_ = {valid_crop.width, valid_crop.height};
        return result;
    }

    void image::text(const std::string &label, const point_i pos) const {
        const auto font = cv::FONT_HERSHEY_SIMPLEX;
        cv::putText(impl->image_mat, label, cvpoint(pos), font, 0.75, {255, 0, 0}, 1);
    }

    void image::circle(const point_i &pos, const int radius) const {
        cv::circle(impl->image_mat, cvpoint(pos), radius, {0, 255, 0}, 2);
    }

    void image::draw_contours(const std::vector<std::vector<point_i> > &contours) const {
        std::vector<std::vector<cv::Point> > cv_contours;
        cv_contours.reserve(contours.size());
        for (const auto &contour: contours) {
            auto &cv_contour = cv_contours.emplace_back();
            cv_contour.reserve(contour.size());
            for (const auto &point: contour) cv_contour.push_back(cvpoint(point));
        }
        cv::drawContours(impl->image_mat, cv_contours, -1, {255, 0, 0}, 2);
    }

    void image::draw(const polygon &shape) const {
        draw(std::vector<polygon>{shape});
    }

    void image::draw(const std::vector<polygon> &polygons) const {
        std::vector<std::vector<cv::Point> > cv_polygons;
        cv_polygons.reserve(polygons.size());
        for (const auto &polygon: polygons) {
            auto &cv_polygon = cv_polygons.emplace_back();
            cv_polygon.reserve(polygon.size());
            for (const auto &point: polygon) cv_polygon.push_back(cvpoint(point));
        }
        cv::drawContours(impl->image_mat, cv_polygons, -1, {255, 0, 0}, 2);
    }

    void image::draw(const std::vector<rotated_rect> &rectangles) const {
        std::vector<std::vector<cv::Point> > cv_rectangles;
        cv_rectangles.reserve(rectangles.size());
        for (const auto &rectangle: rectangles) {
            const auto corners = static_cast<polygon>(rectangle);
            auto &cv_rectangle = cv_rectangles.emplace_back();
            cv_rectangle.reserve(corners.size());
            for (const auto &point: corners) cv_rectangle.push_back(cvpoint(point));
        }
        cv::drawContours(impl->image_mat, cv_rectangles, -1, {255, 0, 0}, 2);
    }

    // void image::setFeatures(const float *new_features) {
    //     std::memcpy(features, new_features, sizeof(features));
    // }

    void image::generate_blob(const double scale, const double mean, const bool swap_rb) const {
        const cv::Scalar scalar_mean{mean, mean, mean};
        cv::dnn::blobFromImage(impl->image_mat, impl->blob_mat, scale, cvsize(size_), scalar_mean, swap_rb, false);
        impl->blob_shape.clear();
        impl->blob_shape.reserve(impl->blob_mat.dims);
        for (int i = 0; i < impl->blob_mat.dims; ++i) impl->blob_shape.push_back(impl->blob_mat.size[i]);
        impl->blob_data = {impl->blob_mat.ptr<float>(), impl->blob_mat.total()};
    }

    float *image::blob() const {
        if (impl->blob_mat.empty()) throw std::runtime_error{"Blob not generated"};
        return impl->blob_data.data();
    }

    size_t image::blob_size() const {
        if (impl->blob_mat.empty()) throw std::runtime_error{"Blob not generated"};
        return impl->blob_data.size();
    }

    const int64_t *image::blob_shape() const {
        if (impl->blob_mat.empty()) throw std::runtime_error{"Blob not generated"};
        return impl->blob_shape.data();
    }

    size_t image::blob_shape_size() const {
        if (impl->blob_mat.empty()) throw std::runtime_error{"Blob not generated"};
        return impl->blob_shape.size();
    }

    void image::rect(const point &left_top, const point &right_bottom) const {
        cv::rectangle(impl->image_mat, cvpoint(left_top), cvpoint(right_bottom), {255, 0, 0}, 2);
    }

    void image::rect(const rect_i &box) const {
        cv::rectangle(impl->image_mat, cvpoint(box.left_top()), cvpoint(box.right_bottom()), {255, 0, 0}, 2);
    }

    void image::rect(const sc::rect &box) const {
        cv::rectangle(impl->image_mat, cvpoint(box.left_top()), cvpoint(box.right_bottom()), {255, 0, 0}, 2);
    }

    image image::warp(const std::array<point, 5> &map_from, const std::array<point, 5> &map_to, size_i to_size) const {
        std::vector<cv::Point2f> src;
        std::vector<cv::Point2f> dst;
        src.reserve(5);
        dst.reserve(5);
        for (const auto &p: map_from) src.emplace_back(p.x(), p.y());
        for (const auto &p: map_to) dst.emplace_back(p.x(), p.y());

        const cv::Mat M = cv::estimateAffinePartial2D(src, dst, cv::noArray(), cv::LMEDS);
        cv::Mat warped;
        cv::warpAffine(impl->image_mat, warped, M, cvsize(to_size));
        image copy(*this);
        copy.impl->image_mat = warped;
        copy.image_changed();
        return {copy};
    }

    image image::crop(const rect_i &area) const {
        image new_image{*this};
        new_image.impl->image_mat = new_image.impl->image_mat(cv::Rect(area.left(), area.top(), area.width(), area.height()));
        new_image.image_changed();
        return new_image;
    }

    void image::rotate(int degrees) {
        while (degrees < 0) degrees += 360;
        degrees %= 360;
        if (degrees == 90) cv::rotate(impl->image_mat, impl->image_mat, cv::ROTATE_90_CLOCKWISE);
        else if (degrees == 270) cv::rotate(impl->image_mat, impl->image_mat, cv::ROTATE_90_COUNTERCLOCKWISE);
        else if (degrees == 180) cv::rotate(impl->image_mat, impl->image_mat, cv::ROTATE_180);
        image_changed();
    }

    void image::crop() {
        impl->image_mat = impl->image_mat(
            cv::Rect(
                impl->padding.width,
                impl->padding.height,
                size_.width() - 2 * impl->padding.width,
                size_.height() - 2 * impl->padding.height
            )
        );
        image_changed();
    }

    int image::snap_to_stride(const int value, const int stride) {
        return (value + stride - 1) / stride * stride;
    }

    size_i image::get_snap_size(int stride, const std::vector<size_i> &valid) {
        size_i best{size_};
        if (!valid.empty()) {
            best = valid.back();
            int bestScore = std::numeric_limits<int>::max();
            for (const auto &candidate: valid) {
                if (candidate.width() < size_.width()) continue;
                if (candidate.height() < size_.height()) continue;
                const int score = std::abs(candidate.width() - size_.width()) + std::abs(
                                      candidate.height() - size_.height());
                if (score < bestScore) {
                    bestScore = score;
                    best = candidate;
                }
            }
        }
        if (stride) {
            best.width(snap_to_stride(best.width(), stride));
            best.height(snap_to_stride(best.height(), stride));
        }
        return best;
    }

    void image::snap_to_size(int stride, const std::vector<size_i> &valid) {
        auto new_size = get_snap_size(stride, valid);
        const float scale = std::min((float) new_size.width() / (float) size_.width(),
                                     (float) new_size.height() / (float) size_.height());
        const cv::Size resizedSize(cvRound((float) size_.width() * scale),
                                   cvRound((float) size_.height() * scale));
        impl->padding = {(new_size.width() - resizedSize.width) / 2, (new_size.height() - resizedSize.height) / 2};

        cv::resize(impl->image_mat, impl->image_mat, resizedSize, 0, 0, cv::INTER_LINEAR);
        cv::Mat canvas(new_size.height(), new_size.width(), CV_8UC3, cv::Scalar(114, 114, 114));
        cv::Rect roi(impl->padding.width, impl->padding.height, resizedSize.width, resizedSize.height);
        impl->image_mat.copyTo(canvas(roi));
        canvas.copyTo(impl->image_mat);
        auto keep_padding = impl->padding;
        image_changed();
        impl->padding = keep_padding;
    }

    void image::mask(double threshold) {
        cv::threshold(impl->image_mat, impl->image_mat, threshold, 255, cv::THRESH_BINARY);
        impl->image_mat.convertTo(impl->image_mat, CV_8U);
    }
} // sc
