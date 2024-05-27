#include "bitmap_prints.h"

void print_inode_bitmap(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt) {
    uint32_t block_size = EXT2_UNLOG(super_block->log_block_size);
    uint32_t inode_bitmap_block = bgdt->inode_bitmap;
    uint32_t inode_bitmap_offset = inode_bitmap_block * block_size;
    uint32_t inode_count = super_block->inodes_per_group;
    uint32_t inode_bitmap_size = (inode_count + 7) / 8;
    uint8_t* inode_bitmap = new uint8_t[inode_bitmap_size];
    if (inode_bitmap == NULL) {
        printf("Error: failed to allocate memory for inode bitmap\n");
        return;
    }

    fseek(file, inode_bitmap_offset, SEEK_SET);
    fread(inode_bitmap, sizeof(uint8_t), inode_bitmap_size, file);

    
    for (uint32_t i = 0; i < inode_count; i++) {
        if (i % 8 == 0) {
            printf("\n");
        }
        printf("%d ", (inode_bitmap[i / 8] >> (i % 8)) & 1);
    }
    printf("\n");

    delete[] inode_bitmap;
}

void print_block_bitmap(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt) {
    uint32_t block_size = EXT2_UNLOG(super_block->log_block_size);
    uint32_t block_bitmap_block = bgdt->block_bitmap;
    uint32_t block_bitmap_offset = block_bitmap_block * block_size;
    uint32_t block_count = super_block->blocks_per_group;
    uint32_t block_bitmap_size = (block_count + 7) / 8;
    uint8_t* block_bitmap = new uint8_t[block_bitmap_size];
    if (block_bitmap == NULL) {
        printf("Error: failed to allocate memory for block bitmap\n");
        return;
    }

    fseek(file, block_bitmap_offset, SEEK_SET);
    fread(block_bitmap, sizeof(uint8_t), block_bitmap_size, file);

    for (uint32_t i = 0; i < block_count; i++) {
        if (i % 8 == 0) {
            printf("\n");
        }
        printf("%d ", (block_bitmap[i / 8] >> (i % 8)) & 1);
    }
    printf("\n");

    delete[] block_bitmap;
}

void print_all_bitmaps(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, size_t group_count) {
    for (size_t i = 0; i < group_count; i++) {
        printf("Inode bitmap for block group %ld:", i);
        print_inode_bitmap(file, super_block, &bgdt[i]);
        printf("Block bitmap for block group %ld:", i);
        print_block_bitmap(file, super_block, &bgdt[i]);
    }
}