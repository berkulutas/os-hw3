#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "identifier.h"
#include "ext2fs_print.h"
#include "ext2fs.h"
#include "bitmap_prints.h"

// GLOBALS
uint32_t block_size;
unsigned int group_count;

void print_all_directories(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, ext2_inode* inode, int depth);


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

void print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printf("-");
    }
    printf(" ");
}

void process_directory_entry(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, ext2_dir_entry* dir_entry, int depth) {
    std::vector<char> name(dir_entry->name_length + 1);
    fread(name.data(), dir_entry->name_length, 1, file);
    name[dir_entry->name_length] = '\0';

    if (strcmp(name.data(), ".") != 0 and strcmp(name.data(), "..") != 0) { // ignore . and ..
        print_indent(depth);
        if (dir_entry->file_type == EXT2_D_DTYPE) { // directory
            printf("%s/\n", name.data());
            ext2_inode* inode = read_inode(file, super_block, bgdt, dir_entry->inode); 
            print_all_directories(file, super_block, bgdt, inode, depth + 1);
            free(inode);
        } else { // file
            printf("%s\n", name.data());
        }
    }
}

void read_block_entries(FILE* file, unsigned int block_number, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, int depth) {
    ext2_dir_entry* dir_entry = new ext2_dir_entry;
    unsigned int offset = 0;

    while (offset < block_size) { // when exceed block size, stop reading
        fseek(file, block_size * block_number + offset, SEEK_SET);
        fread(dir_entry, sizeof(ext2_dir_entry), 1, file);
        if (dir_entry->inode == 0) { // NOTSURE from pdf: As one last thing, a 0 inode value indicates an entry which should be skipped (can be padding or pre-allocation).
            offset += dir_entry->length;
            continue;
        }
        fseek(file, block_size * block_number + offset + sizeof(ext2_dir_entry), SEEK_SET); // size varies but at least ext2_dir_entry size
        process_directory_entry(file, super_block, bgdt, dir_entry, depth); 
        offset += dir_entry->length; // adjust for next entry
    }

    free(dir_entry);
}

void print_all_directories(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, ext2_inode* inode, int depth = 1) {
    if ((inode->mode & 0xf000) != EXT2_I_DTYPE) {
        printf("Error: inode is not a directory\n"); 
        return;
    }

    if (depth == 1) { // for root dir only
        printf("- root/\n");
        depth++;
    }

    // read all direct blocks
    for (size_t i = 0; i < EXT2_NUM_DIRECT_BLOCKS; i++) {
        unsigned int block_number = inode->direct_blocks[i];
        if (block_number == 0) { 
            break;
        }
        read_block_entries(file, block_number, super_block, bgdt, depth);
    }

    // read all indirect blocks
    // single indirect block
    if (inode->single_indirect) {
        unsigned int* indirect_block = new unsigned int[block_size / sizeof(unsigned int)];
        fseek(file, block_size * inode->single_indirect, SEEK_SET);
        fread(indirect_block, sizeof(unsigned int), block_size / sizeof(unsigned int), file);

        for (size_t i = 0; i < block_size / sizeof(unsigned int); i++) {
            unsigned int block_number = indirect_block[i];
            if (block_number == 0) {
                break;
            }
            read_block_entries(file, block_number, super_block, bgdt, depth);
        }

        free(indirect_block);
    }

    // double indirect block
    if (inode->double_indirect) {
        unsigned int* double_indirect_block = new unsigned int[block_size / sizeof(unsigned int)];
        fseek(file, block_size * inode->double_indirect, SEEK_SET);
        fread(double_indirect_block, sizeof(unsigned int), block_size / sizeof(unsigned int), file);

        for (size_t i = 0; i < block_size / sizeof(unsigned int); i++) {
            unsigned int block_number = double_indirect_block[i];
            if (block_number == 0) {
                break;
            }

            unsigned int* indirect_block = new unsigned int[block_size / sizeof(unsigned int)];
            fseek(file, block_size * block_number, SEEK_SET);
            fread(indirect_block, sizeof(unsigned int), block_size / sizeof(unsigned int), file);

            for (size_t j = 0; j < block_size / sizeof(unsigned int); j++) {
                unsigned int block_number = indirect_block[j];
                if (block_number == 0) {
                    break;
                }
                read_block_entries(file, block_number, super_block, bgdt, depth);
            }

            free(indirect_block);
        }

        free(double_indirect_block);
    }

    // triple indirect block
    if (inode->triple_indirect) {
        unsigned int* triple_indirect_block = new unsigned int[block_size / sizeof(unsigned int)];
        fseek(file, block_size * inode->triple_indirect, SEEK_SET);
        fread(triple_indirect_block, sizeof(unsigned int), block_size / sizeof(unsigned int), file);

        for (size_t i = 0; i < block_size / sizeof(unsigned int); i++) {
            unsigned int block_number = triple_indirect_block[i];
            if (block_number == 0) {
                break;
            }

            unsigned int* double_indirect_block = new unsigned int[block_size / sizeof(unsigned int)];
            fseek(file, block_size * block_number, SEEK_SET);
            fread(double_indirect_block, sizeof(unsigned int), block_size / sizeof(unsigned int), file);

            for (size_t j = 0; j < block_size / sizeof(unsigned int); j++) {
                unsigned int block_number = double_indirect_block[j];
                if (block_number == 0) {
                    break;
                }

                unsigned int* indirect_block = new unsigned int[block_size / sizeof(unsigned int)];
                fseek(file, block_size * block_number, SEEK_SET);
                fread(indirect_block, sizeof(unsigned int), block_size / sizeof(unsigned int), file);

                for (size_t k = 0; k < block_size / sizeof(unsigned int); k++) {
                    unsigned int block_number = indirect_block[k];
                    if (block_number == 0) {
                        break;
                    }
                    read_block_entries(file, block_number, super_block, bgdt, depth);
                }

                free(indirect_block);
            }

            free(double_indirect_block);
        }

        free(triple_indirect_block);
    }
}


void inode_bitmap_recover(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, unsigned int inode_bitmap_block, unsigned int inode_table_block, int group_num, bool first_ten_done = false) {
    auto inode_bitmap_size = (super_block->inodes_per_group + 7) / 8;
    // read inode bitmap
    fseek(file, block_size * inode_bitmap_block, SEEK_SET);
    uint8_t* inode_bitmap = new uint8_t[block_size];
    fread(inode_bitmap, sizeof(uint8_t), block_size, file);
    // printf("recovery starting for group %d\n", group_num);
    for (unsigned int j = group_num*super_block->inodes_per_group; j < group_num*super_block->inodes_per_group + super_block->inodes_per_group; j++) {
        // first 10 inodes are reserved for system
        if (!first_ten_done) {
            // mark first 10 inodes as used in inode bitmap
            for (unsigned int i = 0; i < 10; i++) {
                inode_bitmap[i / 8] |= 1 << (i % 8);
            }
            first_ten_done = true;
        }
        ext2_inode* inode = read_inode(file, super_block, bgdt, j + 1);
        // print_inode(inode, j + 1);
        if (inode->link_count != 0 and inode->deletion_time == 0) {
            // printf("inode %d is in use\n", j + 1);
            // mark inode as used in inode bitmap
            // normalize j to be in range 0 to super_block->inodes_per_group
            auto normalized_j = j % super_block->inodes_per_group;
            inode_bitmap[normalized_j / 8] |= 1 << (normalized_j % 8);
        }
        free(inode);
    }
    // //print old inode bitmap 
    // print_inode_bitmap(file, super_block, &bgdt[group_num]);
    // // print new inode bitmap
    // for (unsigned int i = 0; i < super_block->inodes_per_group; i++) {
    //     if (i % 8 == 0) {
    //         printf("\n");
    //     }
    //     printf("%d ", (inode_bitmap[i / 8] >> (i % 8)) & 1);
    // }
    // write inode bitmap back to disk
    fseek(file, block_size * inode_bitmap_block, SEEK_SET);
    fwrite(inode_bitmap, sizeof(uint8_t), block_size, file);

    free(inode_bitmap);
}

void all_inodes_bitmap_recover(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt) {
    // for each block group send inode bitmap and inode table to inode_bitmap_recover
    for (unsigned int i = 0; i < group_count; i++) {
        unsigned int inode_bitmap_block = bgdt[i].inode_bitmap;
        unsigned int inode_table_block = bgdt[i].inode_table;
        inode_bitmap_recover(file, super_block, bgdt, inode_bitmap_block, inode_table_block, i, i != 0);
    }
}

void print_all_inodes(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt) {
    for (unsigned int i = 0; i < group_count; i++) {
        print_inode_bitmap(file, super_block, &bgdt[i]);
        for (unsigned int j = i*super_block->inodes_per_group; j < i*super_block->inodes_per_group + super_block->inodes_per_group; j++) {
            ext2_inode* inode = read_inode(file, super_block, bgdt, j + 1);
            print_inode(inode, j + 1);
            free(inode);
        }
    }
}

void print_all_blocks_bitmap(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt) {
    for (unsigned int i = 0; i < group_count; i++) {
        print_block_bitmap(file, super_block, &bgdt[i]);
    }
}

void block_bitmap_recover(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt, unsigned int block_bitmap_block, int group_num) {
    // printf("recovering block bitmap for group %d\n", group_num);
    // read block bitmap
    fseek(file, block_size * block_bitmap_block, SEEK_SET);
    uint8_t* block_bitmap = new uint8_t[block_size];
    fread(block_bitmap, sizeof(uint8_t), block_size, file);
    if (bgdt[group_num].free_block_count == 0) {
        // printf("no free block exits mark all 1 group %d\n", group_num);
        for (unsigned int i = 0; i < block_size; i++) {
            block_bitmap[i] = 0xff;
        }
    }
    else {
        // printf("free block exits group %d\n", group_num);
        // printf("free block count %d\n", bgdt[group_num].free_block_count);
        // printf("traverse all blocks\n");
        for (unsigned int i = 0; i < super_block->blocks_per_group; i++) {
            // read a block from start not from data blocks
            fseek(file, group_num*(block_size* super_block->blocks_per_group) + block_size * i, SEEK_SET);
            uint8_t* block = new uint8_t[block_size];
            fread(block, sizeof(uint8_t), block_size, file);
            // check if block is free
            bool is_free = true;
            for (unsigned int j = 0; j < block_size; j++) {
                if (block[j] != 0) {
                    is_free = false;
                    break;
                }
            }
            // if block is not free mark it as used in block bitmap
            if (!is_free) {
                block_bitmap[i / 8] |= 1 << (i % 8);
            }
            free(block);

        }

    }

    // write block bitmap back to disk
    fseek(file, block_size * block_bitmap_block, SEEK_SET);
    fwrite(block_bitmap, sizeof(uint8_t), block_size, file);

    free(block_bitmap);
}

void all_blocks_bitmap_recover(FILE* file, ext2_super_block* super_block, ext2_block_group_descriptor* bgdt) {
    // for each block group send block bitmap and block table to block_bitmap_recover
    for (unsigned int i = 0; i < group_count; i++) {
        unsigned int block_bitmap_block = bgdt[i].block_bitmap;
        // unsigned int block_table_block = bgdt[i].block_table;
        block_bitmap_recover(file, super_block, bgdt, block_bitmap_block, i);
    }
}


int main(int argc, char* argv[]) {
    uint8_t* identifier = parse_identifier(argc, argv);
    if (identifier == NULL) { // identifier is invalid
        free(identifier);
        return 1;
    }

    char* file_handle = argv[1];
    FILE* file = fopen(file_handle, "r+");
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
    // print_super_block(super_block);
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

    // debug prints
    // print_block_group_descriptor_table(bgdt, group_count);
    // print_all_bitmaps(file, super_block, bgdt, group_count);

    // part 1 code
    // print_all_inodes(file, super_block, bgdt);
    all_inodes_bitmap_recover(file, super_block, bgdt); 
    // print_all_inodes(file, super_block, bgdt);

    // print_all_blocks_bitmap(file, super_block, bgdt);
    all_blocks_bitmap_recover(file, super_block, bgdt);
    // printf("after\n");
    // print_all_blocks_bitmap(file, super_block, bgdt);

    // part 3 code 
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