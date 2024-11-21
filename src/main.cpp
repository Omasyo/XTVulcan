#include <iostream>
#include <glm/glm.hpp>

#include <imgui.h>

#include "application.hpp"

int main() {
    Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}