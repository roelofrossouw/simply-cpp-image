# simply-cpp-image

A small C++20 wrapper around OpenCV for loading, saving, displaying, and
pre-processing images.

The public API uses the `sc` namespace and the supporting value types from
`simply-cpp`:

```cpp
#include <image.h>

sc::image image{"photo.jpg"};
const auto resized = image.resized({640, 480});
resized.save("photo-small.jpg");
```

## Usage

### CMake

Add this repository to an existing CMake project and link the `sc-image`
target:

```cmake
add_subdirectory(third_party/simply-cpp-image)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE sc-image)
```

The target uses C++20 and exposes the required `simply-cpp` include paths
through its dependencies.

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

`resized()` and `cropped()` return new images. `resize_to()`, `crop()`, the
drawing methods, and `snap_to_size()` modify the current image.

### Displaying an image

```cpp
sc::image image{"input.jpg"};
if (!image.show(1000, "Preview")) {
    // The user pressed Escape.
}
```

On Apple platforms, `show()` returns `false` when the Escape key closes the
preview after a non-negative wait. Its return value may be intentionally
ignored. On other platforms the current implementation does not open a
window and returns `true`.

### DNN preprocessing

Call `generate_blob()` before accessing `blob()`, `blob_size()`, or the blob
shape methods. The returned pointers refer to storage owned by the `image`
object and can be invalidated by subsequent image processing.

```cpp
image.generate_blob(1.0 / 255.0, 0.0, true);
const float* data = image.blob();
const size_t count = image.blob_size();
```

## Requirements

### Build requirements

1. CMake 3.28 or newer.
1. A C++20 compiler.
1. OpenCV with the `core`, `ml`, `imgproc`, `imgcodecs`, `dnn`, `highgui`,
   and (on non-Apple platforms) `calib3d` components.

OpenCV must be installed before configuring the project. On macOS, the
project's initial setup can install it with Homebrew; on Debian-based Linux
systems it can install the development package with `apt`. This setup code
only runs when the build directory is initialized and depends on the
platform package manager being available.

For a manual installation, examples are:

```bash
brew install opencv
# Debian/Ubuntu:
sudo apt install libopencv-dev
```

### Runtime requirements

OpenCV is dynamically linked. It must also be installed and discoverable on
the target machine where an application using `sc-image` runs. The CMake
setup can install OpenCV for the build environment, but it does not bundle,
copy, or automatically distribute OpenCV binaries with the application.
