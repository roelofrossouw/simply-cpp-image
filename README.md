# simply-cpp-image

C++20 wrapper around OpenCV for loading, saving, displaying, and pre-processing images.

The public API uses the `sc` namespace and the supporting value types from `simply-cpp` (sc-core).

## Install

### Homebrew (macOS)

```bash
brew tap roelofrossouw/sc
brew install simply-cpp simply-cpp-image
```

### apt (Ubuntu)

```bash
sudo curl -fsSL https://apt.roelof.co.za/setup.sh | bash
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

`resized()` and `cropped()` return new images. `resize_to()`, `crop()`, the drawing methods, and `snap_to_size()` modify the current image.

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
