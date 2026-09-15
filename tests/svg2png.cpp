#include <image.h>

using namespace std;

int main() {
    auto png = sc::svg2png::FromFile("resource/logo18.svg");
    std::cout << "Got " << png.length() << " bytes for the png" << std::endl;
    std::cout << "Check signature: " << png.substr(1, 3) << std::endl;
    sc::file_put_contents("logo18.png", png);
    return 0;
}
