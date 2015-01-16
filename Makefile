APP=sndgrep
OBJS=main.o
CC=gcc
CFLAGS=-Wall -g3 -O0 $(EXTRA_CFLAGS)
CLIBS=-lfftw3 -lm -lasound
TESTS_KEYS=$(shell seq 0 10)
TESTS=$(TESTS_KEYS:%=test%.dat)

%.o: %.c
	$(CC) -c $^ $(CFLAGS)

$(APP): $(OBJS)
	$(CC) -o $@ $^ $(CLIBS)

%.dat:
	@./$(APP) --dtmf --generate -t  $(basename $(subst test,,$@)) -d 1 $@
	@./$(APP) --dtmf --search -t $(basename $(subst test,,$@)) $@
	@./$(APP) --dtmf --search -t $(basename $(subst test,,$@)) < $@

.PHONY:test
test: clean clean-tests $(APP) $(TESTS)
	
clean:
	$(RM) $(APP) $(OBJS)

clean-tests:
	$(RM) $(TESTS)
