#ifndef SC_IMAGE_INTERNALS_H
#define SC_IMAGE_INTERNALS_H

#include <opencv.h>

#include <opencv2/core.hpp>

#include <cstdint>
#include <span>
#include <vector>

// What an sc::image holds, shared by the sources that make one (opencv.cpp, video.cpp); not
// installed, so OpenCV stays out of the headers.
namespace sc::impl {
    struct internals {
        cv::Mat image_mat;
        cv::Mat blob_mat;
        std::span<float> blob_data;
        std::vector<int64_t> blob_shape;
        size_i padding;
        std::vector<cv::Mat> clips; // pushed clip masks, each already within the one before
    };
}

#endif //SC_IMAGE_INTERNALS_H
