#include "test.h"
const int width = 1280;
const int height = 800;
const std::string title = "Vulkan Test";

int main() {
    WindowInfo info(width, height, title);
    Test test(info);
    test.Run();
    return 0;
}
