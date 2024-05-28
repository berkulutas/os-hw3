#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "identifier.h"
#include "ext2fs_print.h"
#include "ext2fs.h"
#include "bitmap_prints.h"

// GLOBALS
uint32_t block_size;
unsigned int group_count;


ext2_super_block* read_super_block(FILE* file, uint8_t* identifier) {
    ext2_super_block* super_block = new ext2_super_block;
    if (super_block == NULL) {
        printf("Error: failed to allocate memory for super block\n");
        return NULL;
    }

    fseek(file, EXT2_SUPER_BLOCK_POSITION, SEEK_SET);
    fread(super_block, sizeof(ext2_super_block), 1, file);

    return super_block;
}

ext2_block_group_descriptor* read_block_group_descriptor_table(FILE* file, ext2_super_block* super_block) {
    ext2_block_group_descriptor* bgdt = new ext2_block_group_descriptor[group_count];
    if (bgdt == NULL) {
        printf("Error: failed to allocate memory for block group descriptor table\n");
        return NULL;
    }
    fseek(file, EXT2_SUPER_BLOCK_POSITION + EXT2_SUPER_BLOCK_SIZE, SEEK_SET);
    fread(bgdt, sizeof(ext2_block_group_descriptor), group_count, file);

    return bgdt;
}

void print_block_group_descriptor_table(ext2_block_group_descriptor* bgdt, unsigned int group_count) {
    printf("Block group descriptor table:\n");
    for (unsigned int i = 0; i < group_count; i++) {
        printf("Block group %d:\n", i);
        print_group_descriptor(&bgdt[i]);
    }
}



ext2_inode* read_inode(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, unsigned int inode_number) {
    ext2_inode* inode = new ext2_inode;
    if (inode == NULL) {
        printf("Error: failed to allocate memory for inode\n");
        return NULL;
    }

    unsigned int group_number = (inode_number - 1) / super_block->inodes_per_group;
    unsigned int inode_index = (inode_number - 1) % super_block->inodes_per_group;
    unsigned int inode_table_block = bgdt[group_number].inode_table;
    unsigned int inode_offset = inode_index * super_block->inode_size;
    
    fseek(file, block_size * inode_table_block + inode_offset, SEEK_SET);
    fread(inode, sizeof(ext2_inode), 1, file);

    return inode;
}

void print_all_directories(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, ext2_inode* inode, int depth = 1) {
    if ((inode->mode & 0xf000) != EXT2_I_DTYPE) {
        printf("Error: inode is not a directory\n");
        return;
    }

    // if depth 1 print root directory
    if (depth == 1) {
        printf("- root/\n");
    }

    ext2_dir_entry* dir_entry = new ext2_dir_entry;
    // read direct blocks
    for (size_t i = 0; i < EXT2_NUM_DIRECT_BLOCKS; i++) {
        unsigned int block_number = inode->direct_blocks[i];
        if (block_number == 0) {
            break;
        }
        unsigned int offset = 0;
        while (offset < block_size) {
            fseek(file, block_size * block_number + offset, SEEK_SET);
            fread(dir_entry, sizeof(ext2_dir_entry), 1, file);
            if (dir_entry->inode == 0) {
                break;
            }
            char* name = new char[dir_entry->name_length + 1];
            fseek(file, block_size * block_number + offset + sizeof(ext2_dir_entry), SEEK_SET);
            fread(name, dir_entry->name_length, 1, file);
            name[dir_entry->name_length] = '\0';
            if (!(strcmp(name, ".") == 0 or strcmp(name, "..") == 0)) {
                for (int i = 0; i < depth+1; i++) {
                    printf("-");
                }
                printf(" ");
                if (dir_entry->file_type == 2) {
                    printf("%s/\n", name);
                    print_all_directories(file, super_block, bgdt, read_inode(file, super_block, bgdt, dir_entry->inode), depth + 1);
                } else {
                    printf("%s\n", name);
                }
            }
            offset += dir_entry->length;
            free(name);
        }
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
    block_size = EXT2_UNLOG(super_block->log_block_size);

    group_count = (super_block->inode_count + super_block->inodes_per_group - 1) / super_block->inodes_per_group;

    // bgdt is block group descriptor table
    // bgdt is located after super block
    ext2_block_group_descriptor* bgdt = read_block_group_descriptor_table(file, super_block);
    // if (bgdt == NULL) {
    //     fclose(file);
    //     free(super_block);
    //     free(identifier);
    //     return 1;
    // }
    print_block_group_descriptor_table(bgdt, group_count);

    print_all_bitmaps(file, super_block, bgdt, group_count);

    // root inode is always 2
    ext2_inode* root_inode = read_inode(file, super_block, bgdt, EXT2_ROOT_INODE);
    // read all directories in root inode
    print_all_directories(file, super_block, bgdt, root_inode);


    free(root_inode);
    free(bgdt);
    free(super_block);
    fclose(file);
    free(identifier);    
    return 0;
}