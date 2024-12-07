CC = gcc
SOURCE = main.c
EXEC = pomo

$(EXEC): $(SOURCE)
	${CC} $(SOURCE) -lncurses -o $(EXEC)

clean:
	rm -f $(EXEC)
