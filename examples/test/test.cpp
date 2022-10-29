#include <memory>
#include <iostream>
#include "inc.hpp"


int main()
{
    std::shared_ptr<int> s_ptr{new int(123)};
    std::weak_ptr<int>   w_ptr = s_ptr;

    std::cout << w_ptr.use_count() << std::endl;

    s_ptr.reset();

    std::cout << w_ptr.use_count() << std::endl;



    int a = 123;
}