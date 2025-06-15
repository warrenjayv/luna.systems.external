main : start

TF = $(wildcard *.exe)
TD = $(wildcard *.err)

start : di-main.cpp
	date &>> d.err
	g++ di-main.cpp &>> d.err

clean : 
	@if [ -n "$(TF)" ]; then rm $(TF); fi
	@if [ -n "$(TD)" ]; then rm $(TD); fi