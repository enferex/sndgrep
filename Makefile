APP=sndgrep
OBJS=main.o
CC=gcc
CFLAGS=-Wall -g3 -O0
CLIBS=-lfftw3 -lm

%.o: %.c
	$(CC) -c $^ $(CFLAGS)

$(APP): $(OBJS)
	$(CC) -o $@ $^ $(CLIBS)
	
clean:
	$(RM) $(APP) $(OBJS)
