#include <iostream>

int main (int argc, const char * argv[]) 
{
    try
    {
        std::cout << "Hello New World\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}