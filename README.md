# simply-cpp-image

C++20 wrapper around OpenCV for loading, saving, displaying, and pre-processing images.

The public API uses the `sc` namespace and the supporting value types from `simply-cpp` (sc-core).

## Install

### Homebrew (macOS)

```bash
curl -fsSL https://apt.roelof.co.za/setup.sh | bash # taps roelofrossouw/sc - same command as the apt one below
brew install simply-cpp simply-cpp-image
```

### apt (Ubuntu)

```bash
curl -fsSL https://apt.roelof.co.za/setup.sh | bash # registers the apt repo - same command as the brew one above
sudo apt -y install simply-cpp-dev simply-cpp-image-dev
```

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
        sc-image
        GIT_REPOSITORY https://github.com/roelofrossouw/simply-cpp-image.git
        GIT_TAG main # or a specific tag, e.g. v1.0.2, to stay stable
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(sc-image)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE sc::sc-image)
```

sc-core is fetched automatically as part of this if it isn't already available - no separate step needed. Pass `-DFETCH_SC=ON` to always build it from source instead of using an installed one (useful when developing against an unreleased sc-core).

### Git submodule

```bash
git submodule add https://github.com/roelofrossouw/simply-cpp-image.git third_party/sc-image
```

```cmake
add_subdirectory(third_party/sc-image)
target_link_libraries(myapp PRIVATE sc::sc-image)
```

## Dependencies

- **simply-cpp (sc-core)** - sc-image's CMake package depends on it, so `find_package(sc-image CONFIG REQUIRED)` needs `simply-cpp` installed too. Neither the Homebrew formula nor the apt package currently pulls it in automatically, so install both explicitly (see above). FetchContent and the git submodule route fetch/build it automatically instead.
- **OpenCV** - `libopencv-dev` on apt, `opencv` on brew; installed automatically if missing when building from source. It's dynamically linked, so it must also be present on whichever machine runs a binary linked against sc-image - the build does not bundle it.
- **LunaSVG** - fetched and built from source automatically; nothing to install for it.

## Usage

```cmake
find_package(sc-image CONFIG REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE sc::sc-image)
```

### Loading and editing images

```cpp
#include <image.h>

sc::image image{"input.jpg"};
if (image.empty()) {
    // Handle an empty/default image.
}

const auto cropped = image.cropped({0, 0, 320, 240});
const auto resized = cropped.resized({160, 120});
resized.text("preview", {10, 24});
resized.save("output.jpg");
```

`resized()`, `cropped()`, and `side_by_side()` return new images. The two images passed to `side_by_side()` must have the same height. `resize_to()`, `crop()`, the drawing methods, and `snap_to_size()` modify the current image.

Geometry can be drawn one shape at a time or as a vector; rectangles and rotated rectangles are converted to polygons, while circles use their center and radius:

```cpp
sc::image canvas{"input.jpg"};
canvas.draw(sc::rect{10, 10, 80, 40});
canvas.draw(sc::rotated_rect{{50, 50}, {80, 40}, 15});
canvas.draw(sc::polygon{{10, 10}, {90, 10}, {90, 50}, {10, 50}});
canvas.draw(sc::circle{{50, 50}, 20});
canvas.draw(std::vector<sc::circle>{{{50, 50}, 20}, {{100, 50}, 12}});
```

`rect()`, `circle()`, and `text()` are the same drawing methods under a shorter name for the common cases - `canvas.rect(box)` and `canvas.draw(box)` render identically, likewise for a circle:

```cpp
canvas.rect(sc::rect{10, 10, 80, 40});
canvas.circle({50, 50}, 20);
canvas.text("label", {10, 24});
```

To display two equal-height images together:

```cpp
sc::image left_face{"left.jpg"};
sc::image right_face{"right.jpg"};
const auto comparison = sc::image::side_by_side(left_face, right_face);
comparison.show(0, "Face comparison");
```

### Displaying an image

```cpp
sc::image image{"input.jpg"};
if (!image.show(1000, "Preview")) {
    // The user pressed Escape.
}
```

On Apple platforms, `show()` returns `false` when the Escape key closes the preview after a non-negative wait. Its return value may be intentionally ignored. On other platforms the current implementation does not open a window and returns `true`.

### DNN preprocessing

Call `generate_blob()` before accessing `blob()`, `blob_size()`, or the blob shape methods. The returned pointers refer to storage owned by the `image` object and can be invalidated by subsequent image processing.

```cpp
image.generate_blob(1.0 / 255.0, 0.0, true);
const float* data = image.blob();
const size_t count = image.blob_size();
```

To create an image from a float blob, provide contiguous RGB planes in `[1, 3, height, width]` order. For a `[0, 1]` blob:

```cpp
sc::image image = sc::image::from_blob(data, width, height);
```

To reverse `generate_blob()` normalization (including centered ranges such as `[-0.5, 0.5]`), pass the same scale, mean, and channel-swap arguments used to generate it:

```cpp
sc::image image = sc::image::from_blob(data, width, height, 1.0 / 255.0, 127.5, true);
```

## Demo

`sc-image-demo` renders a built-in SVG to PNG, then loads, resizes, draws on and
saves it, showing each call with what it returned and writing only to the
temporary directory. Pass an image file to start from that instead. It is
installed with the runtime package (`simply-cpp-image`), so it also shows an
installation works without the `-dev` package. It is a demonstration, not a
test, so CTest doesn't run it:

```bash
sc-image-demo
sc-image-demo photo.jpg
```

Its source is `examples/sc-image-demo.cpp`; the code below is copied from it at
configure time, so it always matches code that compiles:

<!-- sc-example: examples/sc-image-demo.cpp -->
```cpp
if (source.empty()) {
    sc::console::heading("Rendering an SVG to PNG (lunasvg)");
    const std::string svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="320" height="200">)"
                            R"(<rect width="320" height="200" fill="#2a6f97"/>)"
                            R"(<circle cx="160" cy="100" r="70" fill="#f4d35e"/></svg>)";
    const auto png = sc::svg2png::FromString(svg);
    sc::console::show_text("sc::svg2png::FromString(svg)", std::to_string(png.size()) + " bytes of PNG");
    source = (folder / "sc-image-demo.png").string();
    std::ofstream{source, std::ios::binary} << png;
}

sc::console::heading("Loading and editing (OpenCV)");
const sc::image image{source};
sc::console::step("const sc::image image{source};   // " + source);
SC_SHOW(image.size());
auto preview = image.resized({160, 100});
sc::console::step("auto preview = image.resized({160, 100});");
SC_SHOW(preview.size());
SC_STEP(preview.rect(sc::rect{10, 10, 140, 80}));
SC_STEP(preview.text("simply-cpp", {20, 56}));

sc::console::heading("Saving");
SC_SHOW(preview.save(output));
```
<!-- /sc-example -->

More demos, installed alongside it, each runnable without arguments:

| Demo | Shows |
|---|---|
| `sc-image-shapes` | finding outlines (`find_contours`) and fitted rotated rectangles (`find_min_area_rects`) in rendered shapes, and drawing sc-core geometry back on |
| `sc-image-transform [image]` | resizing, cropping, rotating, straightening a tilted area (`deskewed`), grayscale, `side_by_side`, and preparing a network's input (`snap_to_size`, `generate_blob`, `from_blob`) |

## Requirements

- CMake 3.22 or newer
- A C++20 compiler
- OpenCV with the `core`, `ml`, `imgproc`, `imgcodecs`, `dnn`, `highgui`, and (on non-Apple platforms) `calib3d` components - install it yourself before configuring if you're not using the Homebrew/apt package (see Dependencies above)

## Building and testing

```bash
cmake -B build -S .
cmake --build build -j
ctest --test-dir build --output-on-failure
```
