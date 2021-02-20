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
	if (argc < 2) die("usage: teensy_size <file.elf>\n");
	const char *filename = argv[1];
	fp = fopen(filename, "rb");
	if (!fp) die("Unable to open for reading %s\n", filename);
	fseek(fp, 0, SEEK_END);
	size_t filesize = ftell(fp);
	filedata = malloc(filesize);
	if (!filedata) die("unable to allocate %ld bytes\n", filesize);
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

		uint32_t text_progmem = elf_section_size(".text.progmem");
		uint32_t text_itcm = elf_section_size(".text.itcm");
		uint32_t arm_exidx = elf_section_size(".ARM.exidx");
		uint32_t data = elf_section_size(".data");
		uint32_t bss = elf_section_size(".bss");
		uint32_t bss_dma = elf_section_size(".bss.dma");
		uint32_t text_csf = elf_section_size(".text.csf");

		uint32_t flash_total = text_progmem + text_itcm + arm_exidx + data + text_csf;
		//float flash_percent = (float)(flash_total * 100) / (float)flash_size(model);
		uint32_t flash_headers = 4140 + text_csf;
		uint32_t flash_code = text_progmem - 4140 + text_itcm + arm_exidx;
		uint32_t flash_data = data;

		uint32_t itcm = text_itcm + arm_exidx;
		uint32_t itcm_blocks = (itcm + 0x7FFF) >> 15;
		uint32_t itcm_total = itcm_blocks * 32768;
		uint32_t dtcm = data + bss;
		uint32_t ram2 = bss_dma;

		printf(" FLASH: ");
		printf("code:%u, ", flash_code);
		printf("data:%u, ", flash_data);
		printf("headers:%u  ", flash_headers);
		//printf(" (%.2f%% used)\n", flash_percent);
		printf(" free for files:%u", flash_size(model) - flash_total);
		printf("\n");

		printf("  RAM1: ");
		printf("code:%u, ", itcm_total);
		printf("variables:%u  ", dtcm);
		printf(" free for local variables:%u", 512*1024 - itcm_total - dtcm);
		printf("\n");

		printf("  RAM2: ");
		printf("variables:%u  ", ram2);
		printf(" free for malloc/new:%u", 512*1024 - ram2);
		printf("\n");

		if (model == 0x25) {
			uint32_t bss_extram = elf_section_size(".bss.extram");
			if (bss_extram > 0) printf("EXTRAM: variables:%u\n", bss_extram);
		}
	}

	free(filedata);
	return 0;
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

