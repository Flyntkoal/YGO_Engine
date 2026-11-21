#include <iostream>
#include "ocgapi.h"

int main() {
    int major = 0;
    int minor = 0;

    OCG_GetVersion(&major, &minor);

    std::cout << "Core version: " << major << "." << minor << "\n";

    return 0;
}