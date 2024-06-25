/*  dummy process
 * 
 */
#include <string>
using namespace std;

enum sexm { male, female };

class dummy
{
     public: 
        string name;
        int    age;
        sexm   sex;
        
        dummy ( ) { }
};

int main ( )
{ 
     dummy a1 = dummy( );

    do
    {
        
    } while (true);
}