#ifndef UTIL_WRITER_H
#define UTIL_WRITER_H

#pragma once

#include <iostream>
#include <string>

namespace util
{
  class col
  {
    public:
      static const char* black;
      static const char* red;
      static const char* green;
      static const char* yellow;
      static const char* blue;
      static const char* mag;
      static const char* cyan;
      static const char* white;
      static const char* def;
      /*
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
        def    = "\033[37m]" ;
      }
      */
  };

  const char* util::col::black  = "\033\[0m" ;
  const char* util::col::red    = "\033\[31m";
  const char* util::col::green  = "\033\[32m";
  const char* util::col::yellow = "\033\[33m";
  const char* util::col::blue   = "\033\[34m";
  const char* util::col::mag    = "\033\[35m";
  const char* util::col::cyan   = "\033\[36m";
  const char* util::col::white  = "\033\[37m";
  const char* util::col::def    = "\033\[37m" ;
     

  class aut 
  {
     public:
        static void write( char* msg, const char* color )
        {
            
          std::cout << color << msg << col::def  << std::endl;
        }

        static void write( char* a, char* b, const char* color )
        {
          std::cout << color << a << b << col::def  << std::endl;
        }
  };
};

# endif