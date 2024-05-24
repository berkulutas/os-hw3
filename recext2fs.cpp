#include <stdio.h>
#include <stdlib.h>

#include "identifier.h"
#include "ext2fs_print.h"
#include "ext2fs.h"

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

ext2_block_group_descriptor* read_block_group_descriptor(FILE* file, uint8_t* identifier) {
    ext2_block_group_descriptor* block_group_descriptor = new ext2_block_group_descriptor;
    if (block_group_descriptor == NULL) {
        printf("Error: failed to allocate memory for block group descriptor\n");
        return NULL;
    }

    // block group descriptor table is located after super block
    // super block size is 1024 bytes
    fseek(file, EXT2_SUPER_BLOCK_POSITION + EXT2_SUPER_BLOCK_SIZE, SEEK_SET);
    fread(block_group_descriptor, sizeof(ext2_block_group_descriptor), 1, file);

    return block_group_descriptor;
}

int main(int argc, char* argv[]) {
    uint8_t* identifier = parse_identifier(argc, argv);
    if (identifier == NULL) { // identifier is invalid
        free(identifier);
        return 1;
    }

    char* file_handle = argv[1];
    FILE* file = fopen(file_handle, "r");
    if (file == NULL) {
        printf("Error: file not found\n");
        free(identifier);
        return 1;
    }

    ext2_super_block* super_block = read_super_block(file, identifier);
    if (super_block == NULL) {
        fclose(file);
        free(identifier);
        return 1;
    }

    print_super_block(super_block);

    // block group descriptor table is located after super block
    // super block size is 1024 bytes
    ext2_block_group_descriptor* block_group_descriptor = read_block_group_descriptor(file, identifier);
    if (block_group_descriptor == NULL) {
        fclose(file);
        free(identifier);
        return 1;
    }

    print_group_descriptor(block_group_descriptor);

    // block size
    uint32_t block_size = EXT2_UNLOG(super_block->log_block_size);
    printf("Block size: %u\n", block_size);

    


    free(super_block);
    free(identifier);
    return 0;
}