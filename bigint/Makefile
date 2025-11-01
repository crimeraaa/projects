CC := clang
CC_FLAGS := -Wall -Wextra -Werror -std=c11 -g -fsanitize=address

bin/bigint: obj/bigint.o bigint_test.c | bin
	$(CC) $(CC_FLAGS) -o $@ $^

obj/%.o: %.c %.h | obj
	$(CC) $(CC_FLAGS) -c -o $@ $<

bin obj:
	mkdir -p $@

clean:
	rm -f obj/bigint.o bin/bigint
