CC=gcc -Wall -O2
OBJ=rain.o net.o log.o

rain:$(OBJ)
	$(CC) -g -o  $@ $(OBJ)

$(OBJ):%.o:%.c
	$(CC) -g -c $< -o $@

clean:
	rm -rf *.o
