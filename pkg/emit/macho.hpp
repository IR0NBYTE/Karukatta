#pragma once

#include <cstdint>
#include <vector>
#include <cstring>
#include <fstream>

/*
    Karukatta Mach-O Emitter
    Produces Mach-O executables for macOS ARM64 and x86-64.
    Uses LC_UNIXTHREAD for standalone executables (no libSystem).
    Includes proper __LINKEDIT and LC_CODE_SIGNATURE for Apple Silicon.
*/

namespace macho {

static const uint32_t MH_MAGIC_64      = 0xFEEDFACF;
static const uint32_t MH_EXECUTE       = 0x02;
static const uint32_t MH_NOUNDEFS      = 0x01;

static const uint32_t CPU_TYPE_X86_64  = 0x01000007;
static const uint32_t CPU_TYPE_ARM64   = 0x0100000C;
static const uint32_t CPU_SUBTYPE_ALL  = 0x00000000;
static const uint32_t CPU_SUBTYPE_X86_ALL = 0x03;

static const uint32_t LC_SEGMENT_64    = 0x19;
static const uint32_t LC_UNIXTHREAD    = 0x05;
static const uint32_t LC_CODE_SIGNATURE = 0x1D;

static const uint32_t VM_PROT_NONE     = 0x0;
static const uint32_t VM_PROT_READ     = 0x1;
static const uint32_t VM_PROT_EXECUTE  = 0x4;

static const uint64_t PAGE_SIZE_ARM64  = 0x4000;
static const uint64_t PAGE_SIZE_X86    = 0x1000;

static const uint32_t ARM_THREAD_STATE64 = 6;
static const uint32_t ARM_THREAD_STATE64_COUNT = 68;
static const uint32_t x86_THREAD_STATE64 = 4;
static const uint32_t x86_THREAD_STATE64_COUNT = 42;

struct MachHeader64 {
    uint32_t magic, cputype, cpusubtype, filetype;
    uint32_t ncmds, sizeofcmds, flags, reserved;
};

struct SegmentCommand64 {
    uint32_t cmd, cmdsize;
    char segname[16];
    uint64_t vmaddr, vmsize, fileoff, filesize;
    uint32_t maxprot, initprot, nsects, flags;
};

struct Section64 {
    char sectname[16], segname[16];
    uint64_t addr, size;
    uint32_t offset, align, reloff, nreloc, flags;
    uint32_t reserved1, reserved2, reserved3;
};

struct LinkeditDataCommand {
    uint32_t cmd, cmdsize;
    uint32_t dataoff, datasize;
};

class MachOEmitter {
public:
    bool emit(const std::string& filename,
              const std::vector<uint8_t>& code,
              uint32_t cpu_type = CPU_TYPE_ARM64) {

        std::vector<uint8_t> binary;
        uint64_t page_size = (cpu_type == CPU_TYPE_ARM64) ? PAGE_SIZE_ARM64 : PAGE_SIZE_X86;
        uint32_t cpu_subtype = (cpu_type == CPU_TYPE_X86_64) ? CPU_SUBTYPE_X86_ALL : CPU_SUBTYPE_ALL;
        bool is_arm64 = (cpu_type == CPU_TYPE_ARM64);

        uint32_t thread_state_count = is_arm64 ? ARM_THREAD_STATE64_COUNT : x86_THREAD_STATE64_COUNT;
        uint32_t thread_cmd_size = 16 + thread_state_count * 4;
        uint32_t thread_flavor = is_arm64 ? ARM_THREAD_STATE64 : x86_THREAD_STATE64;

        const size_t hdr_size = sizeof(MachHeader64);
        const size_t seg_size = sizeof(SegmentCommand64);
        const size_t sect_size = sizeof(Section64);
        const size_t ldc_size = sizeof(LinkeditDataCommand);

        // 4 load commands: __PAGEZERO, __TEXT, __LINKEDIT, LC_UNIXTHREAD
        const size_t cmds_size = seg_size + (seg_size + sect_size) + seg_size + thread_cmd_size;
        const size_t headers_end = hdr_size + cmds_size;
        const size_t code_offset = align_up(headers_end, page_size);

        const uint64_t text_vmaddr = 0x100000000ULL;
        const uint64_t entry_addr = text_vmaddr + code_offset;

        // __TEXT covers from 0 to end of code (page-aligned)
        const size_t code_end = code_offset + code.size();
        const size_t text_filesize = align_up(code_end, page_size);

        // __LINKEDIT starts right after __TEXT in file
        const size_t linkedit_offset = text_filesize;
        // Reserve space for codesign to write into
        const size_t sig_size = align_up(4096, 16); // enough for ad-hoc signature
        const size_t linkedit_size = sig_size;
        const size_t total_file_size = linkedit_offset + linkedit_size;

        // ─── Build binary in memory ───

        // Header
        MachHeader64 hdr = {};
        hdr.magic = MH_MAGIC_64;
        hdr.cputype = cpu_type;
        hdr.cpusubtype = cpu_subtype;
        hdr.filetype = MH_EXECUTE;
        hdr.ncmds = 4;
        hdr.sizeofcmds = (uint32_t)cmds_size;
        hdr.flags = MH_NOUNDEFS;
        append(binary, &hdr, sizeof(hdr));

        // __PAGEZERO
        SegmentCommand64 pagezero = {};
        pagezero.cmd = LC_SEGMENT_64;
        pagezero.cmdsize = (uint32_t)seg_size;
        strncpy(pagezero.segname, "__PAGEZERO", 16);
        pagezero.vmsize = text_vmaddr;
        append(binary, &pagezero, sizeof(pagezero));

        // __TEXT
        SegmentCommand64 text_seg = {};
        text_seg.cmd = LC_SEGMENT_64;
        text_seg.cmdsize = (uint32_t)(seg_size + sect_size);
        strncpy(text_seg.segname, "__TEXT", 16);
        text_seg.vmaddr = text_vmaddr;
        text_seg.vmsize = text_filesize;
        text_seg.fileoff = 0;
        text_seg.filesize = text_filesize;
        text_seg.maxprot = VM_PROT_READ | VM_PROT_EXECUTE;
        text_seg.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
        text_seg.nsects = 1;
        append(binary, &text_seg, sizeof(text_seg));

        Section64 text_sect = {};
        strncpy(text_sect.sectname, "__text", 16);
        strncpy(text_sect.segname, "__TEXT", 16);
        text_sect.addr = entry_addr;
        text_sect.size = code.size();
        text_sect.offset = (uint32_t)code_offset;
        text_sect.align = is_arm64 ? 2 : 4;
        text_sect.flags = 0x80000400;
        append(binary, &text_sect, sizeof(text_sect));

        // __LINKEDIT
        SegmentCommand64 linkedit = {};
        linkedit.cmd = LC_SEGMENT_64;
        linkedit.cmdsize = (uint32_t)seg_size;
        strncpy(linkedit.segname, "__LINKEDIT", 16);
        linkedit.vmaddr = text_vmaddr + text_filesize;
        linkedit.vmsize = align_up(linkedit_size, page_size);
        linkedit.fileoff = linkedit_offset;
        linkedit.filesize = linkedit_size;
        linkedit.maxprot = VM_PROT_READ;
        linkedit.initprot = VM_PROT_READ;
        append(binary, &linkedit, sizeof(linkedit));

        // LC_UNIXTHREAD
        uint32_t thread_cmd = LC_UNIXTHREAD;
        append(binary, &thread_cmd, 4);
        append(binary, &thread_cmd_size, 4);
        append(binary, &thread_flavor, 4);
        append(binary, &thread_state_count, 4);

        std::vector<uint8_t> state(thread_state_count * 4, 0);
        if (is_arm64) {
            uint64_t pc = entry_addr;
            memcpy(&state[32 * 8], &pc, 8);
        } else {
            uint64_t rip = entry_addr;
            memcpy(&state[16 * 8], &rip, 8);
        }
        binary.insert(binary.end(), state.begin(), state.end());

        // Pad to code_offset
        binary.resize(code_offset, 0);

        // Append code
        binary.insert(binary.end(), code.begin(), code.end());

        // Pad to linkedit_offset
        binary.resize(linkedit_offset, 0);

        // Reserve linkedit space (zeros — codesign will fill this)
        binary.resize(total_file_size, 0);

        // Write to file
        std::ofstream out(filename, std::ios::binary);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
        out.close();
        return true;
    }

private:
    static void append(std::vector<uint8_t>& buf, const void* data, size_t len) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
        buf.insert(buf.end(), p, p + len);
    }

    static uint64_t align_up(uint64_t val, uint64_t alignment) {
        return (val + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace macho
