all: recext2fs.cpp identifier.cpp ext2fs_print.c
	g++ -g -o recext2fs recext2fs.cpp identifier.cpp ext2fs_print.c bitmap_prints.cpp

leak-check:
	valgrind --leak-check=full ./recext2fs ./testcases/example-baseline.img 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

clean:
	rm -f recext2fs