#include <string>
#include <iostream>
#include <sys/unistd.h>
#include <sstream>
#include <unistd.h>
using namespace std;

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

class seer 
{
  public: 
    static void see( string &a, int& b, sexm& c )
    {
       std::stringstream ss;
       ss << &a; ss << " ";
       ss << &b; ss << " ";
       ss << &c;   

       std::cout << ss.str( ) << std::endl; 
    }

    seer() = delete;
};

int main ( )
{ 

   // process id 
   pid_t pid = getpid();
   std::cout << "process id: " << pid << std::endl;    

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
  
   // reveal member addresses 
   seer::see(test->name, test->age, test->sex); 
  
   do
   {
     sleep(3);
     sleep(1);
   } while (true);
}
