EXES = $(wildcard *.exe)
OS   = $(wildcard *.o)
TGT  = "main"

compile : main.cpp cas.cpp
	@date &>> d.err
	@g++ -g main.cpp -o $(TGT) &>> d.err 
	@echo -e "\033[32mcompile completed.\033[37m"

clean :
	@if [ -n "$(wildcard *.err)" ]; then rm "$(wildcard *.err)"; fi
	@rm -f $(EXES)
	@if [ -n "$(OS)" ]; then rm "$(OS)"; fi
	@echo -e "\033[32mclean complete.\033[37m"
