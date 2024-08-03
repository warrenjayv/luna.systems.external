/*  dummy process
 * 
 */
#include <string>
#include <iostream>
#include <sys/unistd.h>
#include <sstream>
#include <unistd.h>

enum sexm { male, female };

class dummy
{
     public: 
        std::string name;
        int    age;
        sexm   sex;
        
        dummy ( ) { }

        dummy ( std::string _n, int _a, sexm _s)
        {
           name = _n; 
           age  = _a;
           sex  = _s;
        }

        void tostring ( ) {
      
          std::cout << "dummy generated: " + name + ", age: " + std::to_string(age) << std::endl; 
        }
};

int main ( )
{ 
   dummy a1 = dummy( "alyssa", 24, female );
    a1.tostring( );
    a1.tostring( );
    a1.tostring( );
    // int *x = &dummy::tostring;
    void (dummy::* f)() = &dummy::tostring;
    dummy *test = &a1;
    std::stringstream ss;
    ss << test;
    std::cout << "address: " + ss.str() << std::endl;
    do
    {
      sleep(3);
      sleep(1);
    } while (true);
}