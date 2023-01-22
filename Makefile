.SUFFIXES:

.PHONY: all clean

all: metadump parse draw_tree

metadump: main.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

main.o: main.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

parse: parse.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

parse.o: parse.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

draw_tree: draw_tree.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

draw_tree.o: draw_tree.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o metadump parse draw_tree
