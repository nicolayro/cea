CC:=gcc
PROGRAM:=cea
FLAGS:=-Wall -Wextra -pedantic -std=c11
FILES:=main.c

build: $(FILES)
	$(CC) $(FLAGS) -o $(PROGRAM) $(FILES)

run: build
	./$(PROGRAM) $(ARGS)

debug: build
	./$(PROGRAM) $(ARGS) 2>> log

test: build
	$(CC) $(FLAGS) -o test test.c && ./test
	
