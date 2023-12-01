EXE = hello
SRC = $(addsuffix .c, $(EXE))
CC = clang-cl
# Not needed in x64 dev shell, doesn't work when in x86 anyway lmao
LDFLAGS = /ENTRY:main /SUBSYTEM:console /OUT:$(EXE).exe
# Not needed in x64 dev shell, in x86 causes mismatch with target arch anyway
LDLIBS = kernel32.lib legacy_stdio_definitions.lib msvcrt.lib

all: $(EXE)

$(EXE): $(SRC)
	$(CC) $(SRC)

clean:
	$(RM) $(EXE)

.PHONY: all clean
