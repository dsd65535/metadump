.SUFFIXES:

.PHONY: all clean

all: metadump

metadump: main.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

main.o: main.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o metadump
