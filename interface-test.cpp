#include <iostream>
#include "ocgapi.h"

int main() {
    int major = 0;
    int minor = 0;

    std::cout << "Initializing Yu-Gi-Oh! interface...\n";

    OCG_GetVersion(&major, &minor);

    std::cout << "Successfully connected to ygopro-core!\n";
    std::cout << "Core version: "
              << major << "." << minor << "\n";

    return 0;
}