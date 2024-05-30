all: recext2fs.cpp identifier.cpp ext2fs_print.c
	g++ -g -o recext2fs recext2fs.cpp identifier.cpp ext2fs_print.c bitmap_prints.cpp

leak-check:
	valgrind --leak-check=full ./recext2fs ./testcases/example-blockbitmap.img 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00

renew-testcases:
	unzip testcases.zip -d testcases
	unzip testcases1.zip -d testcases1
	unzip testcases2.zip -d testcases2

clean:
	rm -f recext2fs