#include <image.h>

#include <array>
#include <cmath>
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

    // push_clip() and pop_clip() are a canvas's primitives, protected: a test reaches them so.
    struct clipping_image : sc::image {
        using sc::image::image;
        using sc::image::push_clip;
        using sc::image::pop_clip;
    };
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
        const auto rectangle_polygon = static_cast<sc::polygon_i>(rectangle);
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
        for (const auto &contour: contours) CHECK_NOTHROW(canvas.draw(contour, sc::stroke{sc::color::Blue, 2}));
        CHECK_NOTHROW(canvas.draw(sc::rect{1, 1, 2, 2}));
        CHECK_NOTHROW(canvas.draw(sc::rect_i{1, 1, 2, 2}));
        CHECK_NOTHROW(canvas.draw(rectangle));
        CHECK_NOTHROW(canvas.draw(rectangle_polygon));
        CHECK_NOTHROW(canvas.draw(sc::polygon{{1.2, 1.4}, {3.6, 1.4}, {2.5, 3.5}})); // rounded to pixels
        CHECK_NOTHROW(canvas.draw(sc::circle{{2, 2}, 1.5}));
        CHECK_NOTHROW(canvas.draw(sc::circle_i{{2, 2}, 1}));
        for (const auto &each: rectangles) CHECK_NOTHROW(canvas.draw(each));
        CHECK_NOTHROW(canvas.draw(sc::rounded_rect{sc::rect{0, 0, 4, 4}, 1}, {sc::color::Red, sc::color::Blue}));
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
        CHECK_NOTHROW(canvas.draw(sc::text{"label", {10, 10}}));
        CHECK_NOTHROW(canvas.draw(sc::circle_i{{20, 20}, 5}, sc::stroke{sc::color::Green, 2}));
        CHECK_NOTHROW(canvas.draw(sc::rect_i{1, 2, 30, 40}, sc::stroke{sc::color::Blue, 2}));
        // Drawing is in place and must not resize anything.
        CHECK_EQ(canvas.size(), sc::size_i(source_width, source_height));
        CHECK(!canvas.empty());
    }

    error_code ignored;
    filesystem::remove_all(scratch, ignored);

    SECTION("A blank image, and its bytes");
    {
        const sc::image blank{sc::size_i{30, 20}, sc::color::Red};
        CHECK_EQ(blank.size(), sc::size_i(30, 20));
        CHECK_EQ(blank.channels(), 3);
        const auto png = blank.encoded();
        CHECK_EQ(sc::image{png}.size(), sc::size_i(30, 20)); // PNG bytes load back
        CHECK(!blank.encoded("jpg", 50).empty());
        CHECK_THROWS_AS(blank.encoded("nope"), std::runtime_error);
        CHECK_THROWS_AS((sc::image{sc::size_i{0, 5}, sc::color::Red}), std::invalid_argument);
    }

    SECTION("Clipping: only what's inside the clip is painted, clips nest");
    {
        sc::image canvas{sc::size_i{100, 100}, sc::color::Black};
        sc::drawing clipped{{100, 100}};
        clipped.add(sc::rect{10, 10, 80, 80}, sc::appearance::filled(sc::color::White), to_path(sc::circle{{50, 50}, 20}));
        canvas.draw(clipped);
        auto found = canvas.find_min_area_rects(100);
        CHECK_EQ(found.size(), size_t{1});
        if (!found.empty()) { // the circle's square, not the rect's
            CHECK_NEAR(found[0].width(), 40.0, 3.0);
            CHECK_NEAR(found[0].height(), 40.0, 3.0);
        }

        // A clip inside another: only their overlap; popping one goes back to the other.
        clipping_image nested{sc::size_i{100, 100}, sc::color::Black};
        nested.push_clip(to_path(sc::rect{0, 0, 60, 100}));
        nested.push_clip(to_path(sc::rect{40, 0, 60, 100}));
        nested.draw(sc::rect{0, 0, 100, 50}, sc::appearance::filled(sc::color::White));
        nested.pop_clip();
        nested.draw(sc::rect{0, 60, 100, 40}, sc::appearance::filled(sc::color::White));
        nested.pop_clip();
        found = nested.find_min_area_rects(100);
        CHECK_EQ(found.size(), size_t{2});
        for (const auto &shape: found) {
            if (shape.center().y() < 50) CHECK_NEAR(std::min(shape.width(), shape.height()), 20.0, 3.0); // 40 to 60
            else CHECK_NEAR(std::max(shape.width(), shape.height()), 60.0, 3.0);                       // 0 to 60
        }
    }

    SECTION("Fill rules: nonzero adds outlines up by direction, even-odd toggles; open outlines fill as if closed");
    {
        // Whether the middle of box is painted white (on black).
        const auto white_at = [](const sc::image &picture, const sc::rect_i &box) {
            return !picture.cropped(box).find_min_area_rects(box.width() * box.height() / 2.0).empty();
        };
        sc::path same_way; // two squares wound the same way, overlapping in 40..60
        same_way.move_to({10, 10}).line_to({60, 10}).line_to({60, 60}).line_to({10, 60}).close();
        same_way.move_to({40, 40}).line_to({90, 40}).line_to({90, 90}).line_to({40, 90}).close();
        sc::image nonzero{sc::size_i{100, 100}, sc::color::Black};
        nonzero.draw(same_way, sc::appearance::filled(sc::color::White));
        CHECK(white_at(nonzero, {45, 45, 10, 10})); // the overlap stays filled
        sc::image even_odd{sc::size_i{100, 100}, sc::color::Black};
        even_odd.draw(same_way, sc::appearance::filled(sc::fill{sc::color::White, sc::fill_rule::even_odd}));
        CHECK(!white_at(even_odd, {45, 45, 10, 10})); // the overlap toggles back
        CHECK(white_at(even_odd, {15, 15, 10, 10}));

        sc::path ring; // an inner square wound the other way: a hole by either rule
        ring.move_to({10, 10}).line_to({90, 10}).line_to({90, 90}).line_to({10, 90}).close();
        ring.move_to({30, 30}).line_to({30, 70}).line_to({70, 70}).line_to({70, 30}).close();
        sc::image holed{sc::size_i{100, 100}, sc::color::Black};
        holed.draw(ring, sc::appearance::filled(sc::color::White));
        CHECK(!white_at(holed, {45, 45, 10, 10}));
        CHECK(white_at(holed, {12, 45, 10, 10}));

        sc::path open; // never closed: filled as if it were
        open.move_to({10, 10}).line_to({90, 10}).line_to({90, 90}).line_to({10, 90});
        sc::image filled{sc::size_i{100, 100}, sc::color::Black};
        filled.draw(open, sc::appearance::filled(sc::color::White));
        CHECK(white_at(filled, {45, 45, 10, 10}));
    }

    SECTION("A linear gradient fill: each pixel the gradient's colour where it falls");
    {
        // The colour of one pixel, as red, green and blue from 0 to 1.
        const auto colour_at = [](const sc::image &picture, const int x, const int y) {
            auto pixel = picture.cropped(sc::rect_i{x, y, 1, 1});
            pixel.generate_blob(1.0 / 255, 0, true); // RGB planes
            const float *rgb = pixel.blob();
            return std::array<double, 3>{rgb[0], rgb[1], rgb[2]};
        };
        const sc::linear_gradient red_to_blue{{0, 0}, {100, 0}, {{0, sc::color::Red}, {1, sc::color::Blue}}};
        sc::image canvas{sc::size_i{100, 40}, sc::color::Black};
        canvas.draw(sc::rect{0, 0, 100, 20}, sc::appearance::filled(red_to_blue));
        canvas.draw(sc::circle{{50, 30}, 8}, sc::appearance::filled(red_to_blue));
        const auto left = colour_at(canvas, 2, 10), middle = colour_at(canvas, 50, 10), right = colour_at(canvas, 97, 10);
        CHECK_LT(0.9, left[0]);
        CHECK_LT(left[2], 0.1);
        CHECK_NEAR(middle[0], 0.5, 0.05);
        CHECK_NEAR(middle[2], 0.5, 0.05);
        CHECK_LT(0.9, right[2]);
        const auto centre = colour_at(canvas, 50, 30); // the circle, halfway along
        CHECK_NEAR(centre[0], 0.5, 0.05);
        CHECK_NEAR(centre[2], 0.5, 0.05);
        const auto outside = colour_at(canvas, 2, 35); // below the rect, beside the circle: untouched
        CHECK_LT(outside[0] + outside[1] + outside[2], 0.05);
    }

    SECTION("Drawing as an sc::canvas");
    {
        const float blank[60 * 40]{};
        auto canvas = sc::image::from_blob(blank, 60, 40, 1);
        canvas.draw(sc::rect{5, 5, 20, 10}, sc::appearance::filled(sc::color::White)); // filled, no outline
        canvas.draw(sc::circle{{45, 25}, 8}, sc::appearance::filled(sc::color::White));
        const auto found = canvas.find_min_area_rects(20);
        CHECK_EQ(found.size(), size_t{2});
        for (const auto &shape: found) {
            if (shape.center().x() < 30) {
                CHECK_NEAR(std::max(shape.width(), shape.height()), 20.0, 2.0); // the 20 x 10 rectangle (OpenCV
                CHECK_NEAR(std::min(shape.width(), shape.height()), 10.0, 2.0); // fills both edge pixels)
            } else {
                CHECK_NEAR(shape.width(), 16.0, 2.0); // the circle's bounding square
            }
        }

        sc::canvas &any = canvas; // the same calls as on an sc::pdf
        CHECK_NOTHROW(any.draw(sc::rotated_rect{{30, 20}, {10, 4}, 30}, {sc::stroke{sc::color::Red, 2}, sc::color::Blue}));
        CHECK_NOTHROW(any.draw(sc::polygon{{1.5, 1.5}, {10.5, 1.5}, {5, 9}}));
        CHECK_NOTHROW(any.draw(sc::rounded_rect{sc::rect{2, 2, 30, 20}, 5}));
        CHECK_NOTHROW(any.draw(sc::text{"Hi", {2, 30}, sc::font::bold("serif", 12)}, sc::color::White));
        CHECK_NOTHROW(any.draw(sc::line{{0, 0}, {59, 39}}, sc::stroke{sc::color::Green, 1.5}));

        auto drawing = sc::image::from_blob(blank, 60, 40, 1);
        const auto ring = sc::drawing::from_svg(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40">)svg"
                                                R"svg(<path fill-rule="evenodd" fill="#ffffff" d="M0 0 H40 V40 H0 Z M10 10 V30 H30 V10 Z"/></svg>)svg");
        drawing.draw(ring, sc::rect{10, 0, 40, 40}); // a square with a square hole, centred
        const auto outer = drawing.find_min_area_rects(100);
        CHECK_EQ(outer.size(), size_t{1}); // one outline: the hole doesn't make a second shape
        if (!outer.empty()) CHECK_NEAR(outer[0].center().x(), 30.0, 1.5);

        sc::image empty;
        CHECK_NOTHROW(empty.draw(sc::rect{0, 0, 5, 5})); // nothing to draw on, nothing happens

        const sc::text label{"label", {0, 0}, {"sans-serif", 20}};
        const double width = canvas.text_width(label);
        CHECK_BETWEEN(width, 30.0, 80.0);
        CHECK_NEAR(canvas.text_width(sc::text{"label", {}, {"sans-serif", 40}}), 2 * width, 0.2 * width); // within 10%: strokes step in pixels
        CHECK_EQ(empty.text_width(label), width); // no pixels needed to measure
        CHECK_LT(canvas.text_x_height({"sans-serif", 20}), canvas.text_cap_height({"sans-serif", 20}));
        CHECK_BETWEEN(canvas.text_cap_height({"sans-serif", 20}), 10.0, 22.0);
    }

    TEST_SUMMARY();
}
