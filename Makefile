CC = gcc
OUTPUT=run_server
FILES=main.c
FLAG=

compile:
	$(CC) $(FILES) $(FLAG) -o $(OUTPUT)
run:
	./$(OUTPUT)
