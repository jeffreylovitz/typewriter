all: convert.c
	gcc -g -Wall -Wextra json.c convert.c -o typewriter

clean:
	rm -rf *.o