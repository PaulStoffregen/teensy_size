#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#include "minimal_elf.h"

#define MAX_ELF_SECTIONS 1024

typedef struct {
	const unsigned char *ptr;
	uint32_t name_index;
	const char *name;
	uint32_t type;
	uint32_t flags;
	uint32_t addr;
	uint32_t offset;
	uint32_t size;
	uint32_t link;
	uint32_t info;
	uint32_t alignment;
	uint32_t entry_size;
} elf_section_t;

static uint16_t elf_segment_count=0;
static uint32_t segment_header_offset;
static uint32_t segment_header_size;
#define MAX_ELF_SEGMENTS 64
typedef struct {
        uint32_t type;
        uint32_t offset;
        uint32_t virtual_addr;
        uint32_t physical_addr;
        uint32_t file_size;
        uint32_t memory_size;
        uint32_t flags;
        uint32_t alignment;
        elf_section_t *section;
	const unsigned char *ptr;
} elf_segment_t;
static elf_segment_t elf_segments[MAX_ELF_SEGMENTS];


static const elf_section_t * find_elf_section(const char *name);

static int elf_swap_reqd=0;
static int elf_architecture=0;  // 40=ARM, 83=AVR
static uint16_t elf_section_count=0;
static uint32_t section_header_offset;		 // offset within file of section headers
static uint16_t section_header_size;
static uint16_t string_section_index;
static elf_section_t elf_sections[MAX_ELF_SECTIONS];



#define GET8(p)		get_8(&(p))
#define GET8S(p)	(int32_t)(*(signed char *)(p)++)
#define GET16(p)	get_16(&(p))
#define GET32(p)	get_32(&(p))
#define GET32S(p)	(int32_t)get_32(&(p))
#define GETSZ(p)	GET32(p)
#define BYTE_SWAP_16(n)	(((n) << 8) | ((n) >> 8))
#define BYTE_SWAP_32(n)	(((n) << 24) | (((n) << 16) & 0xFF0000) | (((n) << 8) & 0xFF00) | ((n) >> 24))

static inline uint32_t get_8(const unsigned char **ptr)
{
	uint8_t val;
	val = *(const uint8_t *)(*ptr);
	*ptr += 1;
	return val;
}

static inline uint32_t get_16(const unsigned char **ptr)
{
	uint16_t val;
	val = *(const uint16_t *)(*ptr);
	*ptr += 2;
	if (elf_swap_reqd) val = BYTE_SWAP_16(val);
	return val;
}

static inline uint32_t get_32(const unsigned char **ptr)
{
	uint32_t val;
	val = *(const uint32_t *)(*ptr);
	*ptr += 4;
	if (elf_swap_reqd) val = BYTE_SWAP_32(val);
	return val;
}




int elf_get_symbol(const char *name, uint32_t *value)
{
	static const elf_section_t *symtab_section=NULL, *strtab_section=NULL;
	const unsigned char *p, *section_begin, *section_end;
	const char *strtab;
	uint32_t st_name, st_value;
	static const char *cache_name=NULL;
	static uint32_t cache_value=0;

	if (!name) return 0;
	if (cache_name && strcmp(name, cache_name) == 0) {
		if (value) *value = cache_value;
		return 1;
	}
	if (value) *value = 0;

	if (!symtab_section) symtab_section = find_elf_section(".symtab");
	if (!strtab_section) strtab_section = find_elf_section(".strtab");
	if (!symtab_section || !strtab_section) return 0;

	section_begin = p = symtab_section->ptr;
	section_end = section_begin + symtab_section->size;
	strtab = (const char *)(strtab_section->ptr);
	while (p + 16 <= section_end) {
		st_name = GET32(p);
		st_value = GET32(p);
		p += 8; // st_size, st_info, st_other, st_shndx
		if (st_name > strtab_section->size) continue;
		if (strcmp(name, strtab + st_name) == 0) {
			cache_name = strtab + st_name;
			cache_value = st_value;
			if (value) *value = st_value;
			return 1;
		}
	}
	return 0;
}


// inspect the symbol table, counting the interrupt vectors
int elf_teensy_model_id(const unsigned char *elf_unused)
{
	uint32_t id;
	int num;
	uint64_t mask=0;
	uint32_t stack=0;

	if (elf_get_symbol("_teensy_model_identifier", &id)) return id;
	//printf("_teensy_model_identifier not found, looking at other info...\n");

	if (elf_architecture == 83) { // 83=AVR
		if (!elf_get_symbol("__stack", &stack)) return 0;
		for (num=0; num < 64; num++) {
			char buf[64];
			snprintf(buf, sizeof(buf), "__vector_%d", num);
			if (elf_get_symbol(buf, NULL)) {
				mask |= ((uint64_t)1 << num);
			}
		}
		//printf("elf: stack=%04X, vectors=%16llX\n", stack, (long long int)mask);
		if (stack == 0x02FF && mask == 0x00001FFFFFFEll) return 0x19; // Teensy 1.0
		if (stack == 0x0AFF && mask == 0x07FFFFFFFFFEll) return 0x1B; // Teensy 2.0
		if (stack == 0x10FF && mask == 0x003FFFFFFFFEll) return 0x1A; // Teensy++ 1.0
		if (stack == 0x20FF && mask == 0x003FFFFFFFFEll) return 0x1C; // Teensy++ 2.0
	}
	if (elf_architecture == 40) { // 40=ARM
		if (!elf_get_symbol("_estack", &stack)) return 0;
		if (stack == 0x20002000) return 0x1D;  // Teensy 3.0
		if (stack == 0x20008000) return 0x21;  // Teensy 3.1 or 3.2
		if (stack == 0x20020000) return 0x1F;  // Teensy 3.5 (K64), TD 1.41
		if (stack == 0x2002FFFC) return 0x1F;  // Teensy 3.5 (K64), TD 1.42-beta4
		if (stack == 0x2002FFF8) return 0x1F;  // Teensy 3.5 (K64), TD 1.42+
		if (stack == 0x20030000) return 0x22;  // Teensy 3.6 (K66)
		if (stack == 0x20001800) return 0x20;  // Teensy-LC
	}
	return 0; // board unknown
}



static const elf_section_t * find_elf_section(const char *name)
{
	const elf_section_t *section;
	int i;
	for (i=0,section=elf_sections; i<elf_section_count; i++,section++) {
		if (strcmp(section->name, name) == 0) return section;
	}
	return NULL;
}

#if 0
static int is_binary_section(const elf_section_t *section)
{
	if ((section->flags & 2) == 0) return 0; // not allocated in memory
	if (strcmp(section->name, ".eeprom") == 0) return 0;
	if (strcmp(section->name, ".fuse") == 0) return 0;
	if (strcmp(section->name, ".lock") == 0) return 0;
	if (strcmp(section->name, ".signature") == 0) return 0;
	return 1;
}
#endif

int is_elf_binary(uint32_t addr, unsigned int len)
{
	const elf_segment_t *segment;
	uint32_t begin, end;
	int i;

	for (i=0,segment=elf_segments; i<elf_segment_count; i++,segment++) {
		if (segment->type != 1) continue;
		if (segment->file_size == 0) continue;
		begin = segment->physical_addr;
		end = segment->physical_addr + segment->file_size;
		if (begin >= addr + len) continue; // section after requested range
		if (end <= addr) continue; // section before range
		return 1;
	}
	return 0;
}

void get_elf_binary(uint32_t addr, int len, unsigned char *buffer)
{
	const elf_segment_t *segment;
	uint32_t begin, end;
	uint32_t src_offset, dest_offset, copy_len;
	int i;

	memset(buffer, 0xFF, len);

	//printf("request: %x to %x\n", addr, addr + len - 1);
	for (i=0,segment=elf_segments; i<elf_segment_count; i++,segment++) {
		if (segment->type != 1) continue;
		if (segment->file_size == 0) continue;
		begin = segment->physical_addr;
		end = segment->physical_addr + segment->file_size;
		if (begin >= addr + len) continue; // section after requested range
		if (end <= addr) continue; // section before range

		if (begin >= addr) {
			src_offset = 0;
			dest_offset = begin - addr;
			copy_len = len - dest_offset;
		} else {
			src_offset = addr - begin;
			dest_offset = 0;
			copy_len = len;
		}
		if ((end - (begin + src_offset)) < copy_len) {
			copy_len = (end - (begin + src_offset));
		}

		//printf("  segment %x to %x,", begin, end - 1);
		//printf("  src_offset = %x, dest_offset = %x,", src_offset, dest_offset);
		//printf("  len = %x\n", copy_len);
		memcpy(buffer + dest_offset, segment->ptr + src_offset, copy_len);
	}
}

elf_section_t * elf_find_section_by_segment(const elf_segment_t *segment)
{
        elf_section_t *section;
        int i;

        for (i=0,section=elf_sections; i<elf_section_count; i++,section++) {
                //if (segment->offset == section->offset &&
                //  segment->file_size == section->size) {
                if (segment->offset == section->offset) {
                        return section;
                }
        }
        return NULL;
}

int get_elf_eeprom(uint8_t *buffer, int bufsize)
{
	const elf_section_t *section;
	int i, len;

	for (i=0,section=elf_sections; i<elf_section_count; i++,section++) {
		if ((section->flags & 2) == 0) continue; // not allocated in memory
		if (strcmp(section->name, ".eeprom") != 0) continue; // not eeprom
		//printf("eeprom section, addr=%x, offset=%x, len=%d\n",
			//section->addr, section->offset, section->size);
		len = section->size;
		if (len > bufsize) len = bufsize;
		memcpy(buffer, section->ptr, len);
		return len;
	}
	return 0;
}


int parse_elf(const unsigned char *elf)
{
	const unsigned char *p, *q;
	uint16_t type;
	elf_section_t *section;
	elf_segment_t *segment;
	unsigned int i, len, size;

	//printf("parse_elf begin\n");
	if (elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F')
		return -1;	// missing ELF magic number
	if (elf[4] != 1) {
		return -2;	// not 32 bit format
	}
	if (elf[5] == 1) {
		// file is little endian format
		//elf_swap_reqd = (__BYTE_ORDER == __BIG_ENDIAN);
		elf_swap_reqd = (BYTE_ORDER == BIG_ENDIAN);
	} else if (elf[5] == 2) {
		// file is big endian format
		//elf_swap_reqd = (__BYTE_ORDER == __LITTLE_ENDIAN);
		elf_swap_reqd = (BYTE_ORDER == LITTLE_ENDIAN);
	} else {
		return -3;	// file is unknown format
	}

	// read file header
	p = elf + 16;

	type = GET16(p);			// type
	elf_architecture = GET16(p);		// architecture
	GET32(p);				// version
	GETSZ(p);				// entry point
	segment_header_offset = GETSZ(p);	// offset of segment header
	section_header_offset = GETSZ(p);	// offset of section header
	GET32(p);				// flags
	size = GET16(p);			// header size
	segment_header_size = GET16(p);
	elf_segment_count = GET16(p);
	section_header_size = GET16(p);
	elf_section_count = GET16(p);
	string_section_index = GET16(p);
	if (type != 2) 	return -4;		// type, only executable image allowed
	if (size != 52) return -5;		// header must be exactly 52 bytes
	if (section_header_size != 40) return -6; // section headers must be 40 bytes
	if (segment_header_size != 32) return -7; // section headers must be 32 bytes

	// read section headers
	if (elf_section_count > MAX_ELF_SECTIONS) elf_section_count = MAX_ELF_SECTIONS;
	q = elf + section_header_offset;
	section = elf_sections;
	for (i=0; i<elf_section_count; i++) {
		p = q;
		section->name_index = GET32(p);
		section->type = GET32(p);
		section->flags = GETSZ(p);
		section->addr = GETSZ(p);
		section->offset = GETSZ(p);
		section->size = GETSZ(p);
		section->link = GET32(p);
		section->info = GET32(p);
		section->alignment = GETSZ(p);
		section->entry_size = GETSZ(p);
		section->ptr = elf + section->offset;
		section->name = "";
		q += section_header_size;
		section++;
	}

	// fill in the section name fields with pointers to string segment
	if (string_section_index > 0
	  && string_section_index < elf_section_count
	  && elf_sections[string_section_index].size > 0) {
		q = elf_sections[string_section_index].ptr;
		section = elf_sections;
		len = elf_sections[string_section_index].size;
		for (i=0; i<elf_section_count; i++) {
			if (section->name_index < len) {
				section->name = (const char *)q + section->name_index;
			}
			section++;
		}
	}

	// read segment headers
	if (elf_segment_count > MAX_ELF_SEGMENTS) elf_segment_count = MAX_ELF_SEGMENTS;
	q = elf + segment_header_offset;
	segment = elf_segments;
	for (i=0; i<elf_segment_count; i++) {
		p = q;
                segment->type = GET32(p);
                segment->offset = GET32(p);
                segment->virtual_addr = GET32(p);
                segment->physical_addr = GET32(p);
                segment->file_size = GET32(p);
                segment->memory_size = GET32(p);
                segment->flags = GET32(p);
                segment->alignment = GET32(p);
		segment->ptr = elf + segment->offset;
		if (segment->file_size > 0) {
			section = elf_find_section_by_segment(segment);
			if (!section) return -8;
		}
		q += segment_header_size;
		segment++;
	}
	//print_elf_info();
	return 0;
}

uint32_t elf_section_size(const char *name)
{
	elf_section_t *section = elf_sections;
	int i;
	for (i=0; i < elf_section_count; i++) {
		if (strcmp(name, section->name) == 0) return section->size;
		section++;
	}
	return 0;
}

#if 1
void print_elf_info(void)
{
	elf_section_t *section;
	int i, len;

	// print elf file header info, similar to "readelf -h"
	printf("  Start of section headers:          %d\n", section_header_offset);
	printf("  Size of section headers:           %d\n", section_header_size);
	printf("  Number of section headers:         %d\n", elf_section_count);
	printf("  Section header string table index: %d\n", string_section_index);
	printf("  Architecture:                      %d\n", elf_architecture);
	printf("\n");

	// print the section headers, same format as "readelf -S"
	printf("  [Nr] Name              Type            Addr");
	printf("     Off    Size   ES Flg Lk Inf Al\n");
	//for (i=0,section=elf_sections; i<elf_section_count; i++,section++) {
	for (i=0,section=elf_sections; i<elf_section_count; i++,section++) {
		printf("  [%2u] %-17.17s ", i, section->name);
		switch (section->type) {
		  // only a tiny fraction of types known to readelf
		  case 0: printf("NULL            "); break;
		  case 1: printf("PROGBITS        "); break;
		  case 2: printf("SYMTAB          "); break;
		  case 3: printf("STRTAB          "); break;
		  case 0x70000001: printf("ARM_EXIDX       "); break;
		  case 0x70000002: printf("ARM_PREEMPTMAP  "); break;
		  case 0x70000003: printf("ARM_ATTRIBUTES  "); break;
		  default: printf("                "); break;
		}
		printf("%08x %06x ", section->addr, section->offset);
		printf("%06x %02x ", section->size, section->entry_size);
		len = 4;
		if (section->flags & 0x00000001)  // writable
			printf("W"), len--;
		if (section->flags & 0x00000002)  // allocated in memory
			printf("A"), len--;
		if (section->flags & 0x00000004)  // executable
			printf("X"), len--;
		if (section->flags & 0x00000010)  // can be merged
			printf("M"), len--;
		if (section->flags & 0x00000020)  // strings
			printf("S"), len--;
		if (section->flags & 0x00000040)  // info, section header table index
			printf("I"), len--;
		if (section->flags & 0x00000080)  // preserve link order
			printf("L"), len--;
		while (len--) printf(" ");
		printf("%2u %3u %2u\n", section->link, section->info, section->alignment);
	}
	printf("\n");
}
#endif


