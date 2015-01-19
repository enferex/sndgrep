APP=sndgrep
OBJS=main.o
CC=gcc
CFLAGS=-Wall -g3 -O0 $(EXTRA_CFLAGS)
CLIBS=-lfftw3 -lm
TESTS_KEYS=$(shell seq 0 9)
TESTS=$(TESTS_KEYS:%=test%.dat)

%.o: %.c
	$(CC) -c $^ $(CFLAGS)

$(APP): $(OBJS)
	$(CC) -o $@ $^ $(CLIBS) $(EXTRA_CFLAGS)

%.dat:
	@./$(APP) --dtmf --generate -t $(basename $(subst test,,$@)) -d 1 $@
	@./$(APP) --dtmf --search $@
	@./$(APP) --dtmf --search < $@

.PHONY:test
test: clean clean-tests $(APP) $(TESTS) long-stream-test

long-stream-test: $(APP)
	@./$(APP) --dtmf --generate -t 5 -d 3 test.dat
	@./$(APP) --dtmf --search test.dat
	@./$(APP) --dtmf --search < test.dat
	
clean: clean-tests
	@$(RM) -v $(APP) $(OBJS)

clean-tests:
	@$(RM) -v $(TESTS) test.dat
