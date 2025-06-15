main : start

TF = $(wildcard *.exe)
TD = $(wildcard *.err)

DI  = C:\WINDOWS\System32\dinput.dll
DI8 = C:\WINDOWS\System32\dinput8.dll

start : di-main.cpp
	@date &>> d.err
	@g++ -g di-main.cpp &>> d.err
	@echo -e "\033[32mcompilation completed.\033[37m"

clean : 
	@if [ -n "$(TF)" ]; then rm $(TF); fi
	@if [ -n "$(TD)" ]; then rm $(TD); fi
	@echo -e "\033[32mclean complete.\033[37m"

