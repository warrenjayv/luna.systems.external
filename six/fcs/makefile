main : compile

EXES = $(wildcard *.exe)
OS   = $(wildcard *.o)

compile : probe.cpp target.cpp
	@date &>> d.err
	@g++ -g probe.cpp  -o probe.exe &>> d.err 
	@g++ -g target.cpp -o target.exe &>> d.err
	@echo -e "\033[32mcompile completed.\033[37m"

clean :
	@if [ -n "$(wildcard *.err)" ]; then rm "$(wildcard *.err)"; fi
	@rm *exe
	@if [ -n "$(OS)" ]; then rm "$(OS)"; fi
	@echo -e "\033[32mclean complete.\033[37m"
