DIR=.
SRC = $(wildcard ./*.c)
OBJ = $(patsubst %.c, %.o, $(SRC))
CFLAG = -Wall -g -I./ -T../inc
LFLAG = -L../lib -lrt -pthread -lconfig -lzlog
BIN = ./test

all:$(BIN)
$(DIR)/%.o:$(DIR)/%.c
	gcc -c $< -o $@ $(CFLAG)
$(BIN):$(OBJ)
	gcc -o $@ $^ $(LFLAG) 
.PHONY:clean rebuild
clean:
	rm -rf $(BIN) $(OBJ)
rebuild: clean all
