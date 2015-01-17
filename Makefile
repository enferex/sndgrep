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
	@./$(APP) --dtmf --search -t $(basename $(subst test,,$@)) $@
	@./$(APP) --dtmf --search -t $(basename $(subst test,,$@)) < $@

.PHONY:test
test: clean clean-tests $(APP) $(TESTS)
	
clean: clean-tests
	@$(RM) -v $(APP) $(OBJS)

clean-tests:
	@$(RM) -v $(TESTS)
