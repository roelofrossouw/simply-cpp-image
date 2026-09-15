#include <image.h>
#include <algorithm>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    const fs::path source = "resource/test.jpg";
    sc::image original{source.string()};
    assert(!original.empty());
    assert(original.size().width() > 0);
    assert(original.size().height() > 0);
    std::cout << "Loaded " << source << " => " << original.size() << std::endl;
    if (!original.show()) return 0;

    const auto resized = original.resized({64, 48});
    const sc::size_i expected_size{64, 48};
    assert(resized.size() == expected_size);
    assert(original.size() != resized.size());
    if (!resized.show()) return 0;

    const auto cropped = original.cropped({
        0, 0,
        std::min(132, original.size().width()),
        std::min(132, original.size().height())
    });
    assert(cropped.size().width() > 0);
    assert(cropped.size().height() > 0);
    if (!cropped.show()) return 0;

    const auto output = fs::temp_directory_path() / "simply-cpp-image-test.png";
    assert(resized.save(output.string()));
    assert(fs::exists(output));
    fs::remove(output);
}
