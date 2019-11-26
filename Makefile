CC=g++
FLAGS=-Wall -lX11

all: a.out

a.out: main.cpp
	$(CC) main.cpp $(FLAGS) -o a.out


