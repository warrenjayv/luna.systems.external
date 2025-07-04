main : start

TF = $(wildcard *.exe)
TD = $(wildcard *.err)
TO = $(wildcard *.o)

DI  = C:\WINDOWS\System32\dinput.dll
DI8 = C:\WINDOWS\System32\dinput8.dll
DIn = dinput8

start : di-main.cpp
	@date &>> d.err
	@g++ -g -c src/di-error.cpp      &>> d.err
	@g++ -g -c src/di-mouse.cpp      &>> d.err
	@g++ -g -c di-main.cpp &>> d.err &>> d.err
	@g++  di-error.o di-mouse.o di-main.o  -L$(DI8) -l$(DIn) &>> d.err
	@echo -e "\033[32mcompilation completed.\033[37m"

clean : 
	@if [ -n "$(TF)" ]; then rm $(TF); fi
	@if [ -n "$(TD)" ]; then rm $(TD); fi
	@if [ -n "$(TO)" ]; then rm $(TO); fi
	@echo -e "\033[32mclean complete.\033[37m"

