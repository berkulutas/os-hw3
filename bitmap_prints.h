#ifndef BITMAP_PRINTS_H
#define BITMAP_PRINTS_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "ext2fs.h"

void print_inode_bitmap(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt);

void print_block_bitmap(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt);

void print_all_bitmaps(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, size_t group_count);

#endif  // BITMAP_PRINTS_H