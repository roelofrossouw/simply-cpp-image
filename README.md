# simply-cpp-image

C++20 wrapper around OpenCV for loading, saving, displaying, drawing on and pre-processing images,
plus SVG rendering (`sc::svg2png`: SVG read by sc-graphics' `sc::drawing`, drawn with OpenCV) and
`svg2img`, a tool that turns SVG files into PNG, JPEG, WebP, BMP or TIFF images.

**Documentation:** the [simply-cpp wiki](https://github.com/roelofrossouw/simply-cpp/wiki) has an [images guide](https://github.com/roelofrossouw/simply-cpp/wiki/Images) and the [sc-image reference](https://github.com/roelofrossouw/simply-cpp/wiki/Reference-sc-image), plus [getting started](https://github.com/roelofrossouw/simply-cpp/wiki/Getting-Started) for the whole suite.

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

## Dependencies

- **simply-cpp (sc-core)** - sc-image's CMake package depends on it, so `find_package(sc-image CONFIG REQUIRED)` needs `simply-cpp` installed too. Neither the Homebrew formula nor the apt package currently pulls it in automatically, so install both explicitly (see above).
- **OpenCV** - `libopencv-dev` on apt, `opencv` on brew; installed automatically if missing when building from source. It's dynamically linked, so it must also be present on whichever machine runs a binary linked against sc-image - the build does not bundle it.
- **simply-cpp-graphics (sc-graphics)** - the canvas sc::image implements, and the SVG reader
  `sc::svg2png` uses.

## Usage

```cmake
find_package(sc-image CONFIG REQUIRED)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE sc::sc-image)
```

### Loading and editing images

```cpp
#include <image.h>

sc::image image{"input.jpg"}; // throws std::runtime_error if it can't be read

const auto cropped = image.cropped({0, 0, 320, 240});
auto resized = cropped.resized({160, 120}); // not const: drawing changes it
resized.draw(sc::text{"preview", {10, 24}, {"sans-serif", 16}}, sc::color::White);
resized.save("output.jpg");
```

`resized()`, `cropped()`, and `side_by_side()` return new images. The two images passed to `side_by_side()` must have the same height. `resize_to()`, `crop()`, the drawing methods, and `snap_to_size()` modify the current image.

An `sc::image` is an `sc::canvas` (sc-graphics), so shapes and text are drawn on it with the same
calls as on an `sc::pdf`: `draw(shape, appearance)`, in pixels. An `sc::appearance` is a stroke (a
colour and a width; black, 1 pixel by default) and a fill (a colour; none by default): a colour
converts to an outline, `{stroke, fill}` gives both, `sc::appearance::filled(fill)` only the inside.
Points are rounded to whole pixels, edges antialiased, and clip paths honoured. Colours are painted opaque: a partly
transparent one is drawn solid (only the PDF engines honour alpha), a fully transparent one not at all:

```cpp
sc::image canvas{"input.jpg"};
canvas.draw(sc::rect{10, 10, 80, 40});                                       // black outline, 1 pixel
canvas.draw(sc::rounded_rect{sc::rect{10, 10, 80, 40}, 6}, sc::stroke{sc::color::Red, 2}); // rounded corners
canvas.draw(sc::rotated_rect{{50, 50}, {80, 40}, 15}, sc::color::Blue);       // a colour: a stroke 1 wide
canvas.draw(sc::polygon{{10, 10}, {90, 10}, {50, 50}}, {sc::color::Black, sc::color{"gold"}});
canvas.draw(sc::circle{{50, 50}, 20}, sc::appearance::filled(sc::color::White)); // filled only
canvas.draw(sc::text{"label", {10, 24}, {"sans-serif", 16}}, sc::color::White); // baseline at y = 24
canvas.draw(sc::line{{0, 0}, {100, 100}}, sc::stroke{sc::color::Red, 2});
for (const auto &contour: canvas.find_contours()) canvas.draw(contour, sc::stroke{sc::color::Blue, 2});
```

Text uses OpenCV's Hershey fonts: `sans-serif` (the default) or `serif`, bold from weight 600,
italic, with `font.size` as the text's height in pixels.

`find_contours()` gives the outline of each white (non-zero) area as an `sc::polygon_i`, drawn like
any other polygon.

To display two equal-height images together:

```cpp
sc::image left_face{"left.jpg"};
sc::image right_face{"right.jpg"};
const auto comparison = sc::image::side_by_side(left_face, right_face);
comparison.save("comparison.jpg");
```

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

### SVG to images

```cpp
const std::string png = sc::svg2png::FromFile("logo.svg");                        // its own size
const std::string big = sc::svg2png::FromString(svg, 800, 0, sc::color::White);  // 800 wide, on white
const sc::size own = sc::svg2png::Size(svg);                                     // its width and height
sc::image{big}.save("logo.jpg", 90);                                             // JPEG at quality 90
```

A width or height of 0 keeps the aspect ratio; both 0 is the SVG's own size. It draws what
sc-graphics reads and OpenCV draws: paths, the basic shapes, groups, transforms, flat colours and
strokes, with text in OpenCV's Hershey fonts.

Clip paths are drawn, smooth-edged, and so is a white mask (read as a clip); paths fill by their
fill rule (nonzero or even-odd), open outlines as if closed.

Images (PNG or JPEG, an `sc::bitmap`, an SVG `<image>`'s data: URI) are decoded and resampled into
place, their alpha honoured. Linear gradients are drawn too. TODO: radial gradients (drawn in their middle colour), masks by
their luminance, opacity (a transparent background comes out white) and text in real fonts.

## svg2img

`svg2img`, installed with the runtime package, renders SVG files from the command line:

```bash
svg2img logo.svg                         # logo.png next to it, at the SVG's own size
svg2img logo.svg -t jpg -s 2             # logo.jpg, twice the size, on white
svg2img logo.svg thumb.webp -W 120       # 120 wide, the height in proportion
svg2img logo.svg -H 300 -b "#ffeedd"     # 300 high, on a cream background
```

The type comes from `--type` (png, jpg, webp, bmp, tiff), else the output's extension, else PNG;
without an output the input's name gets the type's extension. `--quality` (1 to 100, 95 by
default) sets JPEG and WebP quality. Images go on white unless `--background` gives another colour.
It draws what `sc::svg2png` does (see the TODO above). `--help` lists the options.

## Demo

`sc-image-demo` renders a built-in SVG to PNG, then loads, resizes, draws on and
saves it, showing each call with what it returned and writing only to the
temporary directory. Pass an image file to start from that instead; `--help` lists the options. It is
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
    sc::console::heading("Rendering an SVG to PNG (sc::svg2png)");
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
SC_STEP(preview.draw(sc::rect{10, 10, 140, 80}, sc::stroke{sc::color::White, 2}));
SC_STEP(preview.draw(sc::text{"simply-cpp", {20, 56}, sc::font::bold("sans-serif", 18)}, sc::color::White));

sc::console::heading("Saving");
SC_SHOW(preview.save(output));
```
<!-- /sc-example -->

More demos, installed alongside it, each runnable without arguments (`sc-image-transform` also takes `--help`):

| Demo | Shows |
|---|---|
| `sc-image-shapes` | finding outlines (`find_contours`) and fitted rotated rectangles (`find_min_area_rects`) in rendered shapes, and drawing sc-core geometry back on |
| `sc-image-transform [image]` | resizing, cropping, rotating, straightening a tilted area (`deskewed`), grayscale, `side_by_side`, and preparing a network's input (`snap_to_size`, `generate_blob`, `from_blob`) |

## Requirements

- CMake 3.22 or newer
- A C++20 compiler
- OpenCV with the `core`, `ml`, `imgproc`, `imgcodecs`, `dnn`, and (on non-Apple platforms) `calib3d` components - install it yourself before configuring if you're not using the Homebrew/apt package (see Dependencies above)

## Building and testing

```bash
cmake -B build -S .
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Other ways to use it

The packages above are the simplest route. sc-image can also be built from source, with
CMake's `FetchContent` or as a git submodule (`add_subdirectory`), from
https://github.com/roelofrossouw/simply-cpp-image. It builds sc-core too when it isn't installed (`-DFETCH_SC=ON` always does).
