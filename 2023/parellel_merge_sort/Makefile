compile: 
	gcc -g -o psort psort.c -Wall -Werror -pthread -O

t:
	./sort test-large2/test-large2.in 1.out 16

c:
	./rcheck 1.out test-large2/test-large2.out

clean: 
	rm output
	rm sort
	clear
