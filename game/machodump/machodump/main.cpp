//
//  main.cpp
//  machodump
//
//  Created by everettjf on 2016/11/30.
//  Copyright © 2016年 everettjf. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <mach-o/loader.h>
#include <mach-o/swap.h>
#include <iostream>

using namespace std;


// I/O
void *load_bytes(FILE *obj_file, int offset, int size){
    void *buf = calloc(1, size);
    fseek(obj_file, offset, SEEK_SET);
    fread(buf, size, 1, obj_file);
    return buf;
}


// Magic
bool is_fat(uint32_t magic){
    return magic == FAT_MAGIC || magic == FAT_CIGAM;
}

bool should_swap_bytes(uint32_t magic){
    return magic == MH_CIGAM || magic == MH_CIGAM_64 || magic == FAT_CIGAM;
}

bool is_magic_64(uint32_t magic){
    return magic == MH_MAGIC_64 || magic == MH_CIGAM_64;
}

uint32_t read_magic(FILE *obj_file, int offset){
    uint32_t magic;
    fseek(obj_file, offset, SEEK_SET);
    fread(&magic, sizeof(uint32_t),1,obj_file);
    return magic;
}

// cpu name
const char * cpu_type_name(cpu_type_t cpu_type){
    if(cpu_type == CPU_TYPE_I386) return "i386";
    if(cpu_type == CPU_TYPE_X86_64) return "x86_64";
    if(cpu_type == CPU_TYPE_ARM) return "arm";
    if(cpu_type == CPU_TYPE_ARM64) return "arm64";
    
    return "unknown";
}

// segments
void dump_segment_commands(FILE *obj_file, int offset, int is_swap, uint32_t ncmds){
    int actual_offset = offset;
    for(int i = 0; i < ncmds; ++i){
        load_command *cmd = (load_command*)load_bytes(obj_file, actual_offset, sizeof(load_command));
        if(is_swap) swap_load_command(cmd, NX_UnknownByteOrder);
        
        if(cmd->cmd == LC_SEGMENT_64){
            segment_command_64 *segment = (segment_command_64*)load_bytes(obj_file, actual_offset, sizeof(segment_command_64));
            if(is_swap)swap_segment_command_64(segment, NX_UnknownByteOrder);
            
            cout << "segname:" << segment->segname <<endl;
            
            free(segment);
        }else if(cmd->cmd == LC_SEGMENT){
            segment_command *segment = (segment_command*)load_bytes(obj_file, actual_offset, sizeof(segment_command));
            if(is_swap)swap_segment_command(segment, NX_UnknownByteOrder);
            
            cout << "segname:" << segment->segname <<endl;
            
            free(segment);
        }
        
        actual_offset += cmd->cmdsize;
        
        free(cmd);
    }
}

// header
void dump_mach_header(FILE *obj_file, int offset, int is_64, int is_swap){
    uint32_t ncmds = 0;
    int load_commands_offset = offset;
    
    if(is_64){
        int header_size = sizeof(mach_header_64);
        mach_header *header = (mach_header*)load_bytes(obj_file, offset, header_size);
        if(is_swap) swap_mach_header(header, NX_UnknownByteOrder);
        
        ncmds = header->ncmds;
        load_commands_offset += header_size;
        
        cout << "CPU:" << cpu_type_name(header->cputype) << endl;
        
        free(header);
    }else{
        int header_size = sizeof(mach_header);
        mach_header_64 *header = (mach_header_64*)load_bytes(obj_file, offset, header_size);
        if(is_swap) swap_mach_header_64(header, NX_UnknownByteOrder);
        
        ncmds = header->ncmds;
        load_commands_offset += header_size;
        
        cout << "CPU:" << cpu_type_name(header->cputype) << endl;
        
        free(header);
    }
    
    dump_segment_commands(obj_file, load_commands_offset, is_swap, ncmds);
    
}

// fat
void dump_fat_header(FILE *obj_file,bool is_swap){
    int header_size = sizeof(fat_header);
    int arch_size = sizeof(fat_arch);
    
    fat_header *header = (fat_header*)load_bytes(obj_file, 0, header_size);
    if(is_swap) swap_fat_header(header, NX_UnknownByteOrder);
    
    int arch_offset = header_size;
    for(int i = 0; i < header->nfat_arch; ++i){
        fat_arch* arch = (fat_arch*)load_bytes(obj_file, arch_offset, arch_size);
        if(is_swap) swap_fat_arch(arch, 1, NX_UnknownByteOrder);
        
        int mach_header_offset = arch->offset;
        free(arch);
        
        arch_offset += arch_size;
        
        uint32_t magic = read_magic(obj_file, mach_header_offset);
        int is_64 = is_magic_64(magic);
        int is_swap_mach = should_swap_bytes(magic);
        dump_mach_header(obj_file, mach_header_offset, is_64, is_swap_mach);
    }
    
    free(header);
}


void dump_segments(FILE *obj_file){
    uint32_t magic = read_magic(obj_file, 0);
    bool fat = is_fat(magic);
    bool is_swap = should_swap_bytes(magic);
    
    if(fat){
        dump_fat_header(obj_file, is_swap);
    }else{
        bool is_64 = is_magic_64(magic);
        dump_mach_header(obj_file, 0, is_64, is_swap);
    }
    
}

int main(int argc, const char * argv[]) {
    const char *filename = argv[1];
    cout << filename <<endl;
    FILE *obj_file = fopen(filename, "rb");
    dump_segments(obj_file);
    fclose(obj_file);
    
    return 0;
}
