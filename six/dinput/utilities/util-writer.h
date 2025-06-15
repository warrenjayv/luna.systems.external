#include <iostream>
#include <string>

namespace util
{
  class col
  {
    public:
      static char* black;
      static char* red;
      static char* green;
      static char* yellow;
      static char* blue;
      static char* mag;
      static char* cyan;
      static char* white;
      static char* def;

      static void initialize( )
      {
        black  = "\033[0m]" ;
        red    = "\033[31m]";
        green  = "\033[32m]";
        yellow = "\033[33m]";
        blue   = "\033[34m]";
        mag    = "\033[35m]";
        cyan   = "\033[36m]";
        white  = "\033[37m]";
        def    = "\033[0m]" ;
      }

      /*
      
black: \033[30m
red: \033[31m
green: \033[32m
yellow: \033[33m
blue: \033[34m
magenta: \033[35m
cyan: \033[36m
white: \033[37m*/
    
  };

  class aut 
  {
     static void write( std::string msg, int color )
     {
         
          /*
          #include <windows.h>
          #include <iostream>

           int main() {
          // get a handle to the console output
          HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

          // write "hello, world!" in red
          SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
          std::cout << "hello, world!" << std::endl;

          // write "goodbye, world!" in blue
          SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);
          std::cout << "goodbye, world!" << std::endl;

          // reset the console text attribute to the default
          SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

              return 0;
          }*/
     }
  };
};

/***
 * here's a list of escape sequences for all primary colors in c++:

black: \033[30m
red: \033[31m
green: \033[32m
yellow: \033[33m
blue: \033[34m
magenta: \033[35m
cyan: \033[36m
white: \033[37m

these escape sequences can be used to set the text color in c++. the escape sequence starts with \033, which is the escape character, and is followed by [30m, [31m, etc., which specify the color. the m at the end of the escape sequence indicates that the color should be applied to the text.

to reset the text color back to the default, you can use the escape sequence \033[0m. this should be placed after the text to reset the color.

note that these escape sequences only work on systems that support the vt100 terminal, such as unix and linux. if you are using a windows system, you will need to use the windows.h library to color the text.
 */