all: sshell sshell.o
 
sshell: sshell.o
	gcc -g -Wall -Werror -o sshell sshell.o

sshell.o: sshell.c
	gcc -g -Wall -Werror -c sshell.c

clean:
	rm -f *.o sshell

