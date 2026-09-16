#include <image.h>

#include <filesystem>
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
        // Nothing has been letterboxed yet.
        CHECK_EQ(original.cropped_size(), original.size());
        CHECK_EQ(original.padding(), sc::point(0, 0));
        sc::image mutable_copy{original};
        CHECK(mutable_copy.getFeatures() != nullptr); // getFeatures() is non-const

        const sc::image blank;
        CHECK(blank.empty());
        CHECK_EQ(blank.size(), sc::size_i(0, 0));

        // A file that is not there is an error, not an empty image.
        CHECK_THROWS_AS(sc::image{"resource/no-such-image.jpg"}, runtime_error);
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

        CHECK_THROWS_AS(original.resized({0, 10}), invalid_argument);
        CHECK_THROWS_AS(original.resized({-5, 10}), invalid_argument);
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
        CHECK_EQ(shape[0], 1L);                                    // one image
        CHECK_EQ(shape[1], 3L);                                    // three channels
        CHECK_EQ(shape[2], static_cast<int64_t>(blob_source.size().height()));
        CHECK_EQ(shape[3], static_cast<int64_t>(blob_source.size().width()));
        // The buffer holds exactly the product of its shape.
        CHECK_EQ(blob_source.blob_size(),
                 static_cast<size_t>(shape[0] * shape[1] * shape[2] * shape[3]));
        CHECK(blob_source.blob() != nullptr);
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
