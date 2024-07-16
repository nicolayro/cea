CC:=gcc
PROGRAM:=cea
FLAGS:=-Wall -Wextra -pedantic -std=c11
FILES:=main.c

build: $(FILES)
	$(CC) $(FLAGS) -o $(PROGRAM) $(FILES)

run: build
	./$(PROGRAM)
