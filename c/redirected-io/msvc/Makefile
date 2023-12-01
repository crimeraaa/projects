# Use Nmake.exe for this.
EXE = parent child
SRC = $(patsubst %,%.c,$(EXE))
CC = cl.exe
CFLAGS = /nologo /EHsc
# LDFLAGS = /entry:wmain
LDLIBS = user32.lib shell32.lib
 
 # /link /entry:wmain $(LDFLAGS) $(LDLIBS)
 # /link /entry:main $(LDFLAGS) $(LDLIBS)
all: parent.c child.c
	$(CC) $(CFLAGS) parent.c 
	$(CC) $(CFLAGS) child.c 

# https://learn.microsoft.com/en-us/cpp/build/reference/inference-rules?view=msvc-170
# {.\}.c{.\}.exe:
#	$(CC) $(CFLAGS) $@ /link $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(EXE)
	
.PHONY: all clean
