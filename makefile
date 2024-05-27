all: recext2fs.cpp identifier.cpp ext2fs_print.c
	g++ -o recext2fs recext2fs.cpp identifier.cpp ext2fs_print.c bitmap_prints.cpp

clean:
	rm -f recext2fs