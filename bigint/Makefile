CC := clang
CC_FLAGS := -Wall -Wextra -Werror -std=c11 -g -fsanitize=address

bin/bigint: obj/bigint.o | bin
	$(CC) $(CC_FLAGS) -o $@ $<

obj/bigint.o: bigint.c bigint.h | obj
	$(CC) $(CC_FLAGS) -c -o $@ $<

bin obj:
	mkdir -p $@

clean:
	rm -f obj/bigint.o bin/bigint
