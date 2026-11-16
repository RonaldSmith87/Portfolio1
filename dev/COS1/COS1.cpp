#include <iostream>
#include "Console.h"

int main() {
    try {
        Console app("models");
        app.run();
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}