CC = gcc

TARGET 	= mysh
SRC	= mysh.c
OBJ	= mysh.o

RM = rm -f

$(TARGET): $(OBJ)
	$(CC) -o $@ $^

.c.o:
	$(CC) -c $<


clean:
	$(RM) $(OBJ)
clean_target:
	$(RM) $(TARGET)
clean_all:
	$(RM) $(TARGET) $(OBJ)


