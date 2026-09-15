#include "opencv.h"
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

namespace
{
    cv::Point cvpoint(const sc::point_i& rhs)
    {
        return {rhs.x(), rhs.y()};
    }

    cv::Point2d cvpoint(const sc::point& rhs)
    {
        return {rhs.x(), rhs.y()};
    }

    cv::Size cvsize(const sc::size_i& rhs)
    {
        return {rhs.width(), rhs.height()};
    }

    cv::Size2d cvsize(const sc::size& rhs)
    {
        return {rhs.width(), rhs.height()};
    }
}

namespace sc
{
    namespace impl
    {
        struct internals
        {
            cv::Mat image_mat;
            cv::Mat blob_mat;
            std::span<float> blob_data;
            std::vector<int64_t> blob_shape;
            cv::Size padding;
        };
    }

    image::image() : impl(new impl::internals())
    {
    }

    image::image(const std::string& filename) : impl(new impl::internals())
    {
        impl->image_mat = cv::imread(filename);
        if (impl->image_mat.empty()) throw std::runtime_error{"Could not open image " + filename};
        size_ = {impl->image_mat.cols, impl->image_mat.rows};
    }

    image::image(const image& copy_from) : impl(new impl::internals())
    {
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

    image::image(image&& move_from) noexcept
        : impl(move_from.impl), features{}, size_(move_from.size_)
    {
        std::memcpy(features, move_from.features, sizeof(features));
        move_from.impl = nullptr;
        move_from.size_ = {};
    }

    image& image::operator=(const image& copy_from)
    {
        if (this == &copy_from) return *this;
        image copy{copy_from};
        std::swap(impl, copy.impl);
        std::swap(size_, copy.size_);
        std::memcpy(features, copy.features, sizeof(features));
        return *this;
    }

    image& image::operator=(image&& move_from) noexcept
    {
        if (this == &move_from) return *this;
        delete impl;
        impl = move_from.impl;
        size_ = move_from.size_;
        std::memcpy(features, move_from.features, sizeof(features));
        move_from.impl = nullptr;
        move_from.size_ = {};
        return *this;
    }

    image::~image()
    {
        delete impl;
    }

    bool image::show(int timeout, const std::string& window_name) const
    {
#ifdef __APPLE__
        cv::imshow(window_name, impl->image_mat);
        if (timeout >= 0) return cv::waitKey(timeout) != 27;
#endif
        return true;
    }

    size_i image::size() const
    {
        return size_;
    }

    size_i image::cropped_size() const
    {
        return size_ - padding() * 2;
    }

    point image::padding() const
    {
        return {impl->padding.width, impl->padding.height};
    }

    bool image::empty() const
    {
        return !impl || impl->image_mat.empty();
    }

    bool image::save(const std::string& filename) const
    {
        return cv::imwrite(filename, impl->image_mat);
    }

    image image::resized(const size_i new_size) const
    {
        image result{*this};
        result.resize_to(new_size);
        return result;
    }

    void image::resize_to(const size_i new_size)
    {
        if (new_size.width() <= 0 || new_size.height() <= 0)
            throw std::invalid_argument{"Image size must be positive"};
        cv::resize(impl->image_mat, impl->image_mat, cvsize(new_size), 0, 0, cv::INTER_LINEAR);
        size_ = new_size;
        impl->padding = {};
        impl->blob_mat.release();
        impl->blob_data = {};
        impl->blob_shape.clear();
    }

    image image::cropped(const rect_i& area) const
    {
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

    void image::text(const std::string& label, const point_i pos) const
    {
        const auto font = cv::FONT_HERSHEY_SIMPLEX;
        cv::putText(impl->image_mat, label, cvpoint(pos), font, 0.75, {255, 0, 0}, 1);
    }

    void image::circle(const point_i& pos, const int radius) const
    {
        cv::circle(impl->image_mat, cvpoint(pos), radius, {0, 255, 0}, 2);
    }

    void image::setFeatures(const float* new_features)
    {
        std::memcpy(features, new_features, sizeof(features));
    }

    void image::generate_blob(const double scale, const double mean, const bool swap_rb) const
    {
        const cv::Scalar scalar_mean{mean, mean, mean};
        cv::dnn::blobFromImage(impl->image_mat, impl->blob_mat,
                               scale,
                               cvsize(size_),
                               scalar_mean,
                               swap_rb,
                               false);
        impl->blob_shape.clear();
        impl->blob_shape.reserve(impl->blob_mat.dims);
        for (int i = 0; i < impl->blob_mat.dims; ++i) impl->blob_shape.push_back(impl->blob_mat.size[i]);
        impl->blob_data = {impl->blob_mat.ptr<float>(), impl->blob_mat.total()};
    }

    float* image::blob() const
    {
        if (impl->blob_mat.empty()) throw std::runtime_error{"Blob not generated"};
        return impl->blob_data.data();
    }

    size_t image::blob_size() const
    {
        if (impl->blob_mat.empty()) throw std::runtime_error{"Blob not generated"};
        return impl->blob_data.size();
    }

    const int64_t* image::blob_shape() const
    {
        if (impl->blob_mat.empty()) throw std::runtime_error{"Blob not generated"};
        return impl->blob_shape.data();
    }

    size_t image::blob_shape_size() const
    {
        if (impl->blob_mat.empty()) throw std::runtime_error{"Blob not generated"};
        return impl->blob_shape.size();
    }

    void image::rect(const point& left_top, const point& right_bottom) const
    {
        cv::rectangle(impl->image_mat, cvpoint(left_top), cvpoint(right_bottom), {255, 0, 0}, 2);
    }

    void image::rect(const rect_i& box) const
    {
        cv::rectangle(impl->image_mat, cvpoint(box.left_top()), cvpoint(box.right_bottom()), {255, 0, 0}, 2);
    }

    void image::rect(const sc::rect& box) const
    {
        cv::rectangle(impl->image_mat, cvpoint(box.left_top()), cvpoint(box.right_bottom()), {255, 0, 0}, 2);
    }

    image image::warp(const std::array<point, 5>& map_from, const std::array<point, 5>& map_to, size_i to_size) const
    {
        std::vector<cv::Point2f> src;
        std::vector<cv::Point2f> dst;
        src.reserve(5);
        dst.reserve(5);
        for (const auto& p : map_from) src.emplace_back(p.x(), p.y());
        for (const auto& p : map_to) dst.emplace_back(p.x(), p.y());

        const cv::Mat M = cv::estimateAffinePartial2D(src, dst, cv::noArray(), cv::LMEDS);
        cv::Mat warped;
        cv::warpAffine(impl->image_mat, warped, M, cvsize(to_size));
        image copy(*this);
        copy.impl->image_mat = warped;
        copy.size_ = to_size;
        copy.impl->padding = {};
        copy.impl->blob_mat.release();
        copy.impl->blob_data = {};
        copy.impl->blob_shape.clear();
        return {copy};
    }


    void image::crop()
    {
        impl->image_mat = impl->image_mat(
            cv::Rect(
                impl->padding.width,
                impl->padding.height,
                size_.width() - 2 * impl->padding.width,
                size_.height() - 2 * impl->padding.height
            )
        );
        impl->padding = {0, 0};
        size_ = {impl->image_mat.cols, impl->image_mat.rows};
        impl->blob_mat.release();
        impl->blob_data = {};
        impl->blob_shape.clear();
    }

    int image::snap_to_stride(const int value, const int stride)
    {
        return (value + stride - 1) / stride * stride;
    }

    size_i image::get_snap_size(int stride, const std::vector<size_i>& valid)
    {
        size_i best{size_};
        if (!valid.empty())
        {
            best = valid.back();
            int bestScore = std::numeric_limits<int>::max();
            for (const auto& candidate : valid)
            {
                if (candidate.width() < size_.width()) continue;
                if (candidate.height() < size_.height()) continue;
                const int score = std::abs(candidate.width() - size_.width()) + std::abs(
                    candidate.height() - size_.height());
                if (score < bestScore)
                {
                    bestScore = score;
                    best = candidate;
                }
            }
        }
        if (stride)
        {
            best.width(snap_to_stride(best.width(), stride));
            best.height(snap_to_stride(best.height(), stride));
        }
        return best;
    }

    void image::snap_to_size(int stride, const std::vector<size_i>& valid)
    {
        auto new_size = get_snap_size(stride, valid);
        const float scale = std::min((float)new_size.width() / (float)size_.width(),
                                     (float)new_size.height() / (float)size_.height());
        const cv::Size resizedSize(cvRound((float)size_.width() * scale),
                                   cvRound((float)size_.height() * scale));
        impl->padding = {(new_size.width() - resizedSize.width) / 2, (new_size.height() - resizedSize.height) / 2};

        cv::resize(impl->image_mat, impl->image_mat, resizedSize, 0, 0, cv::INTER_LINEAR);
        cv::Mat canvas(new_size.height(), new_size.width(), CV_8UC3, cv::Scalar(114, 114, 114));
        cv::Rect roi(impl->padding.width, impl->padding.height, resizedSize.width, resizedSize.height);
        impl->image_mat.copyTo(canvas(roi));
        canvas.copyTo(impl->image_mat);
        size_ = {impl->image_mat.cols, impl->image_mat.rows};
        impl->blob_mat.release();
        impl->blob_data = {};
        impl->blob_shape.clear();
    }
} // sc
