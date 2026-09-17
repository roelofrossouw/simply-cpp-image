#include <image.h>

using namespace std;

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Warning: no image filename was passed as a parameter." << endl;
        return 0;
    }

    const sc::image demo(argv[1]);
    cout << "Loaded image size: " << demo.size() << endl;
    return 0;
}
