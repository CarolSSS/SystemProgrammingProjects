output: shell_main.o format.o shell.o vector.o callbacks.o sstring.o
	gcc shell_main.o format.o shell.o vector.o callbacks.o sstring.o -o shell

shell_main.o: shell_main.c
	gcc -c shell_main.c
	
format.o: format.c format.h
	gcc -c format.c

shell.o: shell.c shell.h
	gcc -c shell.c

vector.o: vector.c vector.h
	gcc -c vector.c

callbacks.o: callbacks.c callbacks.h
	gcc -c callbacks.c

sstring.o: sstring.c sstring.h
	gcc -c sstring.c
