#include "any.h"


void TestAny::test()
{
    std::cout<< "test common any" << std::endl;

    printObj("chips",std::string("chip-sss"));
    
    Any anyA(std::string("any"));
    std::string a = any_cast<std::string>(anyA);
    std::cout<< "a=" << a << std::endl;

    Any anyB(1);
    int b = any_cast<int>(anyB);
    std::cout<< "b=" << b << std::endl;

    Any anyC("any");
    const char * c = any_cast<const char*>(anyC);
    std::cout<< "c=" << c << std::endl;

    Any anyD(1.);
    double d = any_cast<double>(anyD);
    std::cout<< "d=" << d << std::endl;

    Any anyE(anyD);
    double e = any_cast<double>(anyE);
    std::cout<< "e=" << e << std::endl;

    Any anyF = anyD;
    double f = any_cast<double>(anyF);
    std::cout<< "f=" << f << std::endl;
 
}

void TestAny::printObj(std::string name,Any any)
{
    if(name == "chips")
    {
        std::string str = any_cast<std::string>(any);
        std::cout << "str:" << str << std::endl;
    }
    std::cout << "printObj" << std::endl;
}










