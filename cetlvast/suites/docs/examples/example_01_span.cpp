#include "cetl/span.h"
#include <iostream>
#include <cstring>

int main()
{
//! [helloworld]
    const char* greeting = "Hello World.";
    cetl::span<const char> dynamic{greeting, std::strlen(greeting)};
    std::cout << dynamic.data() << std::endl;
    return 0;
//! [helloworld]
}
