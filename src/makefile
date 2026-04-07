CC = gcc
FLAGS = -Wall
LIBS = -lreadline -lm

TARGETS = ep1 imesh

all: $(TARGETS)

ep1: ep1.c
	$(CC) $(FLAGS) -o ep1 ep1.c $(LIBS)

imesh: imesh.c
	$(CC) $(FLAGS) -o imesh imesh.c $(LIBS)

clean:
	rm -f $(TARGETS)