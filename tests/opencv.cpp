#include <image.h>

#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

#include <sc_test.h>

using namespace std;

namespace {
    // Known dimensions of the checked in fixture.
    constexpr int source_width = 640;
    constexpr int source_height = 427;

    // show() opens a window and waits, so it is left out entirely: a headless server
    // has nothing to display on, and the old test bailed out at the first call.
}

int main() {
    const string source = "resource/test.jpg";
    const auto scratch = filesystem::temp_directory_path() / "sc-image-opencv-test";
    filesystem::create_directories(scratch);

    SECTION("Loading");
    {
        const sc::image original{source};
        CHECK(!original.empty());
        CHECK_EQ(original.size(), sc::size_i(source_width, source_height));
        CHECK_EQ(original.channels(), 3);
        // Nothing has been letterboxed yet.
        CHECK_EQ(original.cropped_size(), original.size());
        CHECK_EQ(original.padding(), sc::point(0, 0));
        sc::image mutable_copy{original};

        const sc::image blank;
        CHECK(blank.empty());
        CHECK_EQ(blank.size(), sc::size_i(0, 0));
        CHECK_EQ(blank.channels(), 0);

        // A file that is not there is an error, not an empty image.
        CHECK_THROWS_AS(sc::image{"resource/no-such-image.jpg"}, runtime_error);
    }

    SECTION("Converting color channels");
    {
        sc::image converted{source};
        const auto original_size = converted.size();
        converted.generate_blob(1.0 / 255, 0, true);
        CHECK_NOTHROW(converted.blob());
        converted.channels(1);
        CHECK_EQ(converted.channels(), 1);
        CHECK_EQ(converted.size(), original_size);
        CHECK_THROWS_AS(converted.blob(), runtime_error);

        converted.channels(3);
        CHECK_EQ(converted.channels(), 3);
        CHECK_EQ(converted.size(), original_size);

        CHECK_THROWS_AS(converted.channels(2), invalid_argument);
        CHECK_THROWS_AS(sc::image{}.channels(1), invalid_argument);
    }

    SECTION("Copying and moving");
    {
        const sc::image original{source};

        const sc::image copied{original};
        CHECK_EQ(copied.size(), original.size());
        CHECK(!original.empty()); // copying leaves the source alone

        sc::image donor{original};
        const sc::image moved{std::move(donor)};
        CHECK_EQ(moved.size(), original.size());
        CHECK(donor.empty()); // the storage was handed over

        sc::image assigned;
        assigned = original;
        CHECK_EQ(assigned.size(), original.size());
        CHECK(!assigned.empty());
    }

    SECTION("Resizing");
    {
        const sc::image original{source};

        const auto resized = original.resized({64, 48});
        CHECK_EQ(resized.size(), sc::size_i(64, 48));
        CHECK_EQ(original.size(), sc::size_i(source_width, source_height)); // resized() copies

        sc::image in_place{original};
        in_place.resize_to({32, 16});
        CHECK_EQ(in_place.size(), sc::size_i(32, 16));

        // <= 0 now auto sizes (if one dim)
        // CHECK_THROWS_AS(original.resized({0, 10}), invalid_argument);
        // CHECK_THROWS_AS(original.resized({-5, 10}), invalid_argument);

        // Both sizes can't be <= 0
        CHECK_THROWS_AS(original.resized({0, 0}), invalid_argument);
        CHECK_THROWS_AS(original.resized({-5, 0}), invalid_argument);
        CHECK_THROWS_AS(original.resized({0, -5}), invalid_argument);
    }

    SECTION("Joining images horizontally");
    {
        const sc::image left{source};
        const auto right = left.resized({320, source_height});
        const auto joined = sc::image::side_by_side(left, right);
        CHECK_EQ(joined.size(), sc::size_i(source_width + 320, source_height));
        CHECK_THROWS_AS(sc::image::side_by_side(left, sc::image{}), invalid_argument);
        CHECK_THROWS_AS(sc::image::side_by_side(left, left.resized({320, 100})), invalid_argument);
    }

    SECTION("Cropping");
    {
        const sc::image original{source};

        const auto cropped = original.cropped({0, 0, 100, 80});
        CHECK_EQ(cropped.size(), sc::size_i(100, 80));

        const auto offset = original.cropped({10, 20, 50, 40});
        CHECK_EQ(offset.size(), sc::size_i(50, 40));

        // Asking for more than there is is an error rather than a silent clamp.
        CHECK_THROWS_AS(original.cropped({0, 0, source_width + 10, source_height}), invalid_argument);
    }

    SECTION("Finding and drawing contours");
    {
        const float pixels[] = {
            0, 0, 0, 0, 0,
            0, 1, 1, 1, 0,
            0, 1, 1, 1, 0,
            0, 1, 1, 1, 0,
            0, 0, 0, 0, 0
        };
        auto mask = sc::image::from_blob(pixels, 5, 5, 1);
        mask.mask(127);
        const auto contours = mask.find_contours();
        CHECK_EQ(contours.size(), size_t{1});
        CHECK_EQ(contours.front().size(), size_t{4});
        CHECK(std::ranges::find(contours.front(), sc::point_i{1, 1}) != contours.front().end());

        const auto rectangles = mask.find_min_area_rects();
        CHECK_EQ(rectangles.size(), size_t{1});
        CHECK(mask.find_min_area_rects(4).size() == 1);
        CHECK(mask.find_min_area_rects(4.01).empty());
        CHECK_THROWS_AS(mask.find_min_area_rects(-1), invalid_argument);
        const auto &rectangle = rectangles.front();
        CHECK_NEAR(rectangle.width(), 2.0, 1.0);
        CHECK_NEAR(rectangle.height(), 2.0, 1.0);
        const auto rectangle_polygon = static_cast<sc::polygon>(rectangle);
        CHECK_EQ(rectangle_polygon.size(), size_t{4});
        CHECK(std::ranges::find(rectangle_polygon, sc::point_i{1, 1}) != rectangle_polygon.end());
        CHECK(std::ranges::find(rectangle_polygon, sc::point_i{3, 1}) != rectangle_polygon.end());
        CHECK(std::ranges::find(rectangle_polygon, sc::point_i{3, 3}) != rectangle_polygon.end());
        CHECK(std::ranges::find(rectangle_polygon, sc::point_i{1, 3}) != rectangle_polygon.end());
        const auto deskewed = mask.deskewed(rectangle);
        CHECK(!deskewed.empty());
        CHECK_EQ(deskewed.size(), sc::size_i(2, 2));
        const sc::rotated_rect invalid_area{{0, 0}, {0, 0}, 0};
        CHECK_THROWS_AS(mask.deskewed(invalid_area), invalid_argument);

        const float blank_pixels[14 * 12]{};
        const auto blank_image = sc::image::from_blob(blank_pixels, 14, 12, 1);
        const sc::polygon tilted_rectangle{{2, 5}, {10, 1}, {12, 5}, {4, 9}};
        const auto tilted_crop = blank_image.deskewed(static_cast<sc::rotated_rect>(tilted_rectangle));
        CHECK_EQ(tilted_crop.size(), sc::size_i(9, 4));

        const sc::polygon portrait_rectangle{{2, 1}, {4, 1}, {4, 10}, {2, 10}};
        const auto horizontal_crop = blank_image.deskewed(static_cast<sc::rotated_rect>(portrait_rectangle));
        CHECK_EQ(horizontal_crop.size(), sc::size_i(9, 2));

        sc::image canvas{source};
        CHECK_NOTHROW(canvas.draw_contours(contours));
        CHECK_NOTHROW(canvas.draw_contours({}));
        CHECK_NOTHROW(canvas.draw(sc::rect{1, 1, 2, 2}));
        CHECK_NOTHROW(canvas.draw(sc::rect_i{1, 1, 2, 2}));
        CHECK_NOTHROW(canvas.draw(rectangle));
        CHECK_NOTHROW(canvas.draw(rectangle_polygon));
        CHECK_NOTHROW(canvas.draw(sc::circle{{2, 2}, 1.5}));
        CHECK_NOTHROW(canvas.draw(sc::circle_i{{2, 2}, 1}));
        CHECK_NOTHROW(canvas.draw(std::vector<sc::polygon>{}));
        CHECK_NOTHROW(canvas.draw(std::vector<sc::polygon>{rectangle_polygon}));
        CHECK_NOTHROW(canvas.draw(rectangles));
        CHECK_NOTHROW(canvas.draw(std::vector<sc::rotated_rect>{}));
        CHECK_NOTHROW(canvas.draw(std::vector<sc::rect>{{1, 1, 2, 2}}));
        CHECK_NOTHROW(canvas.draw(std::vector<sc::circle>{{{2, 2}, 1.5}}));
        CHECK_NOTHROW(canvas.draw(std::vector<sc::circle_i>{{{2, 2}, 1}}));
    }

    SECTION("Stride alignment");
    {
        // Rounds up to the next multiple, and leaves an exact multiple alone.
        CHECK_EQ(sc::image::snap_to_stride(100, 32), 128);
        CHECK_EQ(sc::image::snap_to_stride(128, 32), 128);
        CHECK_EQ(sc::image::snap_to_stride(1, 32), 32);
        CHECK_EQ(sc::image::snap_to_stride(0, 32), 0);
        CHECK_EQ(sc::image::snap_to_stride(33, 16), 48);

        sc::image snapped{source};
        const auto target = snapped.get_snap_size(32);
        CHECK_EQ(target, sc::size_i(640, 448)); // 427 rounds up to 448, 640 already fits
        CHECK_EQ(target.width() % 32, 0);
        CHECK_EQ(target.height() % 32, 0);

        snapped.snap_to_size(32);
        CHECK_EQ(snapped.size(), target);
        // Letterboxed rather than stretched: the content keeps its own size and the
        // difference becomes padding.
        CHECK_EQ(snapped.padding(), sc::point(0, 10));
        CHECK_LT(snapped.cropped_size().height(), snapped.size().height());

        snapped.crop();
        CHECK_EQ(snapped.padding(), sc::point(0, 0));
    }

    SECTION("Blob generation");
    {
        sc::image blob_source{source};
        // Nothing to hand out before a blob has been made.
        CHECK_THROWS_AS(blob_source.blob(), runtime_error);
        CHECK_THROWS_AS(blob_source.blob_size(), runtime_error);
        CHECK_THROWS_AS(blob_source.blob_shape(), runtime_error);

        blob_source.snap_to_size(32);
        blob_source.generate_blob(1.0 / 255, 0, true);

        CHECK_EQ(blob_source.blob_shape_size(), size_t{4});
        const auto *shape = blob_source.blob_shape();
        CHECK_EQ(shape[0], 1L); // one image
        CHECK_EQ(shape[1], 3L); // three channels
        CHECK_EQ(shape[2], static_cast<int64_t>(blob_source.size().height()));
        CHECK_EQ(shape[3], static_cast<int64_t>(blob_source.size().width()));
        // The buffer holds exactly the product of its shape.
        CHECK_EQ(blob_source.blob_size(),
                 static_cast<size_t>(shape[0] * shape[1] * shape[2] * shape[3]));
        CHECK(blob_source.blob() != nullptr);
    }

    SECTION("Creating images from float blobs");
    {
        // Contiguous RGB planes, each containing two pixels.
        const float blob[] = {
            1.0f, 0.0f,
            0.5f, 0.25f,
            0.0f, 1.0f
        };
        auto restored = sc::image::from_blob(blob, 2, 1);
        CHECK(!restored.empty());
        CHECK_EQ(restored.size(), sc::size_i(2, 1));

        restored.generate_blob(1.0 / 255, 0, true);
        const auto *round_trip = restored.blob();
        for (size_t i = 0; i < 6; ++i)
            CHECK_NEAR(round_trip[i], blob[i], 1.0 / 255);

        const float centered_blob[] = {
            0.5f, -0.5f,
            0.0f, -0.25f,
            -0.5f, 0.5f
        };
        auto centered = sc::image::from_blob(centered_blob, 2, 1, 1.0 / 255, 127.5, true);
        centered.generate_blob(1.0 / 255, 127.5, true);
        for (size_t i = 0; i < 6; ++i)
            CHECK_NEAR(centered.blob()[i], centered_blob[i], 1.0 / 255);

        const float grayscale_blob[] = {0.0f, 0.5f, 1.0f};
        auto grayscale = sc::image::from_blob(grayscale_blob, 3, 1, 1);
        CHECK_EQ(grayscale.size(), sc::size_i(3, 1));
        grayscale.generate_blob(1.0 / 255, 0, true);
        CHECK_EQ(grayscale.blob_shape()[1], 1L);
        CHECK_EQ(grayscale.blob_size(), size_t{3});
        for (size_t i = 0; i < 3; ++i) {
            CHECK_NEAR(grayscale.blob()[i], grayscale_blob[i], 1.0 / 255);
        }

        CHECK_THROWS_AS(sc::image::from_blob(nullptr, 2, 1), invalid_argument);
        CHECK_THROWS_AS(sc::image::from_blob(blob, 0, 1), invalid_argument);
        CHECK_THROWS_AS(sc::image::from_blob(blob, 2, 1, 0, 127.5, true), invalid_argument);
        CHECK_THROWS_AS(sc::image::from_blob(grayscale_blob, 3, 1, 2), invalid_argument);
        const float invalid_blob[] = {0.0f, 0.0f, std::numeric_limits<float>::infinity()};
        CHECK_THROWS_AS(sc::image::from_blob(invalid_blob, 1, 1), invalid_argument);
    }

    SECTION("Saving round trips");
    {
        const sc::image original{source};
        const auto resized = original.resized({64, 48});
        const auto path = (scratch / "saved.png").string();

        CHECK(resized.save(path));
        CHECK(filesystem::exists(path));
        CHECK_LT(uintmax_t{0}, filesystem::file_size(path));

        const sc::image reloaded{path};
        CHECK_EQ(reloaded.size(), resized.size());
    }

    SECTION("Drawing leaves the image usable");
    {
        sc::image canvas{source};
        CHECK_NOTHROW(canvas.text("label", {10, 10}));
        CHECK_NOTHROW(canvas.circle({20, 20}, 5));
        CHECK_NOTHROW(canvas.rect(sc::rect_i{1, 2, 30, 40}));
        CHECK_NOTHROW(canvas.rect(sc::point{1, 2}, sc::point{30, 40}));
        // Drawing is in place and must not resize anything.
        CHECK_EQ(canvas.size(), sc::size_i(source_width, source_height));
        CHECK(!canvas.empty());
    }

    error_code ignored;
    filesystem::remove_all(scratch, ignored);

    TEST_SUMMARY();
}
