#include <stdio.h>
#include <stdlib.h>

#include "identifier.h"
#include "ext2fs_print.h"
#include "ext2fs.h"
#include "bitmap_prints.h"

ext2_super_block* read_super_block(FILE* file, uint8_t* identifier) {
    ext2_super_block* super_block = new ext2_super_block;
    if (super_block == NULL) {
        printf("Error: failed to allocate memory for super block\n");
        return NULL;
    }

    fseek(file, EXT2_SUPER_BLOCK_POSITION, SEEK_SET);
    fread(super_block, sizeof(ext2_super_block), 1, file);


    // TODO delete
    // if (super_block->magic != EXT2_SUPER_MAGIC) {
    //     printf("Error: magic number is not EXT2_SUPER_MAGIC\n");
    //     free(super_block);
    //     return NULL;
    // }

    return super_block;
}

ext2_block_group_descriptor* read_block_group_descriptor_table(FILE* file, ext2_super_block* super_block, unsigned int group_count) {
    ext2_block_group_descriptor* bgdt = new ext2_block_group_descriptor[group_count];
    if (bgdt == NULL) {
        printf("Error: failed to allocate memory for block group descriptor table\n");
        return NULL;
    }
    fseek(file, EXT2_SUPER_BLOCK_POSITION + EXT2_SUPER_BLOCK_SIZE, SEEK_SET);
    fread(bgdt, sizeof(ext2_block_group_descriptor), group_count, file);

    return bgdt;
}

void print_block_group_descriptor_table(ext2_block_group_descriptor* bgdt, size_t group_count) {
    printf("Block group descriptor table:\n");
    for (size_t i = 0; i < group_count; i++) {
        printf("Block group %ld:\n", i);
        print_group_descriptor(&bgdt[i]);
    }
}




int main(int argc, char* argv[]) {
    uint8_t* identifier = parse_identifier(argc, argv);
    if (identifier == NULL) { // identifier is invalid
        free(identifier);
        return 1;
    }

    char* file_handle = argv[1];
    FILE* file = fopen(file_handle, "r");
    // if (file == NULL) {
    //     printf("Error: file not found\n");
    //     free(identifier);
    //     return 1;
    // }

    ext2_super_block* super_block = read_super_block(file, identifier);
    if (super_block == NULL) {
        fclose(file);
        free(identifier);
        return 1;
    }
    print_super_block(super_block);

    unsigned int group_count = (super_block->inode_count + super_block->inodes_per_group - 1) / super_block->inodes_per_group;
    printf("Group count: %d\n", group_count);

    // bgdt is block group descriptor table
    // bgdt is located after super block
    ext2_block_group_descriptor* bgdt = read_block_group_descriptor_table(file, super_block, group_count);
    // if (bgdt == NULL) {
    //     fclose(file);
    //     free(super_block);
    //     free(identifier);
    //     return 1;
    // }
    print_block_group_descriptor_table(bgdt, group_count);

    print_all_bitmaps(file, super_block, bgdt, group_count);


    free(bgdt);
    free(super_block);
    fclose(file);
    free(identifier);    
    return 0;
}