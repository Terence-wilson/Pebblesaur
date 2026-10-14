CFLAGS = -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I.

test: test_model
	./test_model

test_model: test_model.c ../../src/c/model.c ../../src/c/model.h
	$(CC) $(CFLAGS) -o $@ test_model.c ../../src/c/model.c

clean:
	rm -f test_model

.PHONY: test clean
