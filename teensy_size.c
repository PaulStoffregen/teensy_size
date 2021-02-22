#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "minimal_elf.h"

void die(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

unsigned char *filedata = NULL;
FILE *fp = NULL;


const char * model_name(int num)
{
	switch (num) {
		case 0x19: return "Teensy 1.0";
		case 0x1A: return "Teensy++ 1.0";
		case 0x1B: return "Teensy 2.0";
		case 0x1C: return "Teensy++ 2.0";
		case 0x1D: return "Teensy 3.0";
		case 0x1E: return "Teensy 3.1";
		case 0x1F: return "Teensy 3.5";
		case 0x20: return "Teensy LC";
		case 0x21: return "Teensy 3.2";
		case 0x22: return "Teensy 3.6";
		case 0x23: return "Teensy 4-Beta1";
		case 0x24: return "Teensy 4.0";
		case 0x25: return "Teensy 4.1";
		case 0x26: return "Teensy MicroMod";
	}
	return "Teensy";
}

uint32_t flash_size(int model)
{
	if (model == 0x24) return 2031616;  // Teensy 4.0
	if (model == 0x25) return 8126464;  // Teensy 4.1
	if (model == 0x26) return 16515072; // MicroMod
	return 0;
}


int main(int argc, char **argv)
{
	int retval = 0;
	if (argc < 2) die("usage: teensy_size <file.elf>\n");
	const char *filename = argv[1];
	fp = fopen(filename, "rb");
	if (!fp) die("Unable to open for reading %s\n", filename);
	fseek(fp, 0, SEEK_END);
	size_t filesize = ftell(fp);
	filedata = malloc(filesize);
	if (!filedata) die("unable to allocate %ld bytes\n", (long)filesize);
	rewind(fp);
	if (fread(filedata, 1, filesize, fp) != filesize)  die("Unable to read %s\n", filename);
	fclose(fp);
	fp = NULL;
	if (parse_elf(filedata) != 0) die("Unable to parse %s\n", filename);


	int model = elf_teensy_model_id(filedata);
	if (!model) die("Can't determine Teensy model from %s\n", filename);

	//print_elf_info();
	//printf("Teensy Model is %02X (%s)\n", model, model_name(model));

	if (model == 0x24 || model == 0x25 || model == 26) {

		uint32_t text_headers = elf_section_size(".text.headers");
		uint32_t text_code = elf_section_size(".text.code");
		uint32_t text_progmem = elf_section_size(".text.progmem");
		uint32_t text_itcm = elf_section_size(".text.itcm");
		uint32_t arm_exidx = elf_section_size(".ARM.exidx");
		uint32_t data = elf_section_size(".data");
		uint32_t bss = elf_section_size(".bss");
		uint32_t bss_dma = elf_section_size(".bss.dma");
		uint32_t text_csf = elf_section_size(".text.csf");

		uint32_t flash_total = text_headers + text_code + text_progmem
			+ text_itcm + arm_exidx + data + text_csf;
		uint32_t flash_headers = text_headers + text_csf;
		uint32_t flash_code = text_code + text_itcm + arm_exidx;
		uint32_t flash_data = text_progmem + data;

		uint32_t itcm = text_itcm + arm_exidx;
		uint32_t itcm_blocks = (itcm + 0x7FFF) >> 15;
		uint32_t itcm_total = itcm_blocks * 32768;
		uint32_t dtcm = data + bss;
		uint32_t ram2 = bss_dma;

		int32_t free_flash = (int32_t)flash_size(model) - (int32_t)flash_total;
		int32_t free_for_local = 512*1024 - (int32_t)itcm_total - (int32_t)dtcm;
		int32_t free_for_malloc = (int32_t)512*1024 - (int32_t)ram2;

		const char *prefix = "teensy_size: ";
		if ((free_flash < 0) || (free_for_local <= 0) || (free_for_malloc < 0)) retval = -1;

		fprintf(stderr,
			"%sMemory Usage on %s:\n", prefix, model_name(model));
		fprintf(stderr,
			"%s  FLASH: code:%u, data:%u, headers:%u   free for files:%d\n", (free_flash < 0) ? "" : prefix,
			flash_code, flash_data, flash_headers, free_flash);
		fprintf(stderr,
			"%s   RAM1: code:%u, variables:%u   free for local variables:%d\n",	(free_for_local <= 0) ? "" : prefix,
			 itcm_total, dtcm, free_for_local);
		fprintf(stderr,
			"%s   RAM2: variables:%u  free for malloc/new:%d\n", (free_for_malloc < 0) ? "" : prefix,
			ram2, free_for_malloc);
		if (model == 0x25) {
			uint32_t bss_extram = elf_section_size(".bss.extram");
			if (bss_extram > 0) {
				fprintf(stderr,
					"%s EXTRAM: variables:%u\n", prefix, bss_extram);
			}
		}
		if (retval != 0) {
			fprintf(stderr,"Error program exceeds memory space\n");
		}
		fflush(stderr);
	}

	free(filedata);
	return retval;
}



void die(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	fprintf(stderr, "mktinyfat: ");
	vfprintf(stderr, format, args);
	va_end(args);
	if (fp) fclose(fp);
	if (filedata) free(filedata);
	exit(1);
}

