#pragma once

#include <cstdint>
#include <vector>
#include <cstring>
#include <fstream>

/*
    Karukatta ELF64 Emitter
    Produces minimal but valid ELF64 executables directly.
    No linker needed.
*/

namespace elf {

// ELF64 structures (from elf.h, redefined here to avoid system dependency)

struct Elf64_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

// Constants
static const uint16_t ET_EXEC = 2;
static const uint16_t EM_X86_64 = 0x3E;
static const uint16_t EM_AARCH64 = 0xB7;
static const uint32_t PT_LOAD = 1;
static const uint32_t PF_X = 1;
static const uint32_t PF_W = 2;
static const uint32_t PF_R = 4;

// Base virtual address for the executable
static const uint64_t BASE_VADDR = 0x400000;

class ELFEmitter {
public:
    // machine: EM_X86_64 or EM_AARCH64
    bool emit(const std::string& filename,
              const std::vector<uint8_t>& code,
              uint16_t machine = EM_X86_64) {

        // Layout:
        // [ELF Header][Program Header][Code]
        //
        // Everything is in one PT_LOAD segment for simplicity.
        // The entry point is at BASE_VADDR + header_size.

        const size_t ehdr_size = sizeof(Elf64_Ehdr);  // 64 bytes
        const size_t phdr_size = sizeof(Elf64_Phdr);   // 56 bytes
        const size_t headers_size = ehdr_size + phdr_size; // 120 bytes
        const uint64_t entry_vaddr = BASE_VADDR + headers_size;

        // ─── ELF Header ───
        Elf64_Ehdr ehdr;
        memset(&ehdr, 0, sizeof(ehdr));

        // Magic number
        ehdr.e_ident[0] = 0x7F;
        ehdr.e_ident[1] = 'E';
        ehdr.e_ident[2] = 'L';
        ehdr.e_ident[3] = 'F';
        ehdr.e_ident[4] = 2;     // ELFCLASS64
        ehdr.e_ident[5] = 1;     // ELFDATA2LSB (little-endian)
        ehdr.e_ident[6] = 1;     // EV_CURRENT
        ehdr.e_ident[7] = 0;     // ELFOSABI_NONE (System V)

        ehdr.e_type = ET_EXEC;
        ehdr.e_machine = machine;
        ehdr.e_version = 1;
        ehdr.e_entry = entry_vaddr;
        ehdr.e_phoff = ehdr_size;  // program headers right after ELF header
        ehdr.e_shoff = 0;          // no section headers
        ehdr.e_flags = 0;
        ehdr.e_ehsize = ehdr_size;
        ehdr.e_phentsize = phdr_size;
        ehdr.e_phnum = 1;
        ehdr.e_shentsize = 0;
        ehdr.e_shnum = 0;
        ehdr.e_shstrndx = 0;

        // ─── Program Header (single PT_LOAD) ───
        Elf64_Phdr phdr;
        memset(&phdr, 0, sizeof(phdr));

        phdr.p_type = PT_LOAD;
        phdr.p_flags = PF_R | PF_X;   // read + execute
        phdr.p_offset = 0;             // load from start of file
        phdr.p_vaddr = BASE_VADDR;
        phdr.p_paddr = BASE_VADDR;
        phdr.p_filesz = headers_size + code.size();
        phdr.p_memsz = headers_size + code.size();
        phdr.p_align = 0x200000;       // 2MB alignment (standard)

        // ─── Write to file ───
        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open()) return false;

        out.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
        out.write(reinterpret_cast<const char*>(&phdr), sizeof(phdr));
        out.write(reinterpret_cast<const char*>(code.data()), code.size());

        out.close();
        return true;
    }
};

} // namespace elf
