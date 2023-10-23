#include <iostream>

#include "application.hpp"

int main() {
    Application app;
    // glfwCreateWindowSurface();

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}