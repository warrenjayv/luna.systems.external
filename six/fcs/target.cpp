/*  dummy process
 * 
 */
#include <string>
#include <iostream>

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
    do
    {
        
    } while (true);
}