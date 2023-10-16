#include <iostream>

#include "application.hpp"

int main(int argc, const char *argv[])
{
    Application app;

    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}