main : compile

EXES = $(wildcard *.exe)
OS   = $(wildcard *.o)

compile : probe.cpp target.cpp
	@date &>> d.err
	@g++ -g -c probe.cpp  -o probe.exe  &>> d.err 
	@g++ -g -c target.cpp -o target.exe &>> d.err
	@echo -e "\033[32mcompile completed.\033[37m"

clean :
	@if [ -n "$(EXES)" ]; then rm "$(EXES)"; fi
	@if [ -n "$(OS)" ]; then rm "$(OS)"; fi
	@if [ -n "$(wildcard *.err)" ]; then rm "$(wildcard *.err)"; fi
	@echo -e "\033[32mclean complete.\033[37m"