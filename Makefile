all: main.c
	gcc -g -Wall -Wextra main.c -o typewriter

clean:
	rm -rf *.o