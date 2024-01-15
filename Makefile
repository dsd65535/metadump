.SUFFIXES:

.PHONY: all clean

LDLIBS += -lcrypto

all: metadump parse draw_tree

metadump: main.o common.o statx-wrapper.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

main.o: main.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

statx-wrapper.o: statx-wrapper.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

parse: parse.o common.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

parse.o: parse.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

draw_tree: draw_tree.o common.o
	$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@

draw_tree.o: draw_tree.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

common.o: common.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o metadump parse draw_tree
