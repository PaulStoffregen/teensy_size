#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "minimal_elf.h"

void die(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void line(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
const char * get_output();
const char *prefix = NULL;



unsigned char *filedata = NULL;
FILE *fp = NULL;

struct {
	const char *name;
	uint32_t size;
	uint32_t max_size;
} json_sections[4];

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
	if (model == 0x19) return 15872;    // Teensy 1.0
	if (model == 0x1A) return 64512;    // Teensy++ 1.0
	if (model == 0x1B) return 32256;    // Teensy 2.0
	if (model == 0x1C) return 130048;   // Teensy++ 2.0
	if (model == 0x1D) return 131072;   // Teensy 3.0
	if (model == 0x1E) return 262144;   // Teensy 3.1
	if (model == 0x1F) return 524288;   // Teensy 3.5
	if (model == 0x20) return 63488;    // Teensy LC
	if (model == 0x21) return 262144;   // Teensy 3.2
	if (model == 0x22) return 1048576;  // Teensy 3.6
	if (model == 0x23) return 1572864;  // Teensy 4-Beta1
	if (model == 0x24) return 2031616;  // Teensy 4.0
	if (model == 0x25) return 8126464;  // Teensy 4.1
	if (model == 0x26) return 16515072; // MicroMod
	return 0;
}

uint32_t ram_size(int model)
{
	if (model == 0x19) return 512;     // Teensy 1.0
	if (model == 0x1A) return 4096;    // Teensy++ 1.0
	if (model == 0x1B) return 2560;    // Teensy 2.0
	if (model == 0x1C) return 8192;    // Teensy++ 2.0
	if (model == 0x1D) return 16384;   // Teensy 3.0
	if (model == 0x1E) return 65536;   // Teensy 3.1
	if (model == 0x1F) return 262144;  // Teensy 3.5
	if (model == 0x20) return 8192;    // Teensy LC
	if (model == 0x21) return 65536;   // Teensy 3.2
	if (model == 0x22) return 262144;  // Teensy 3.6
	return 0;
}



void usage()
{
	die("usage: teensy_size [--json] <file.elf>\n");
}

int main(int argc, char **argv)
{
	int retval = 0;
	int json = 0;
	unsigned int arduino_cli = 0;
	unsigned int arduino_ide = 0;
	memset(json_sections, 0, sizeof(json_sections));
	FILE *fout = stdout;

	// parse command line
	if (argc < 2) usage();
	const char *filename = argv[1];
	if (strcmp(filename, "--json") == 0 && argc > 2) {
		json = 1;
		filename = argv[2];
	}

	// detect Arduino version info
	const char *arduino = getenv("ARDUINO_USER_AGENT");
	if (arduino) {
		//printf("ARDUINO_USER_AGENT = %s\n", arduino);
		int n1=0, n2=0, n3=0;
		const char *cli = strstr(arduino, "arduino-cli/");
		if (cli && sscanf(cli+12, "%d.%d.%d", &n1, &n2, &n3) == 3 &&
		  n1 > 0 && n1 < 256 && n2 > 0 && n2 < 256 && n3 > 0 && n3 < 256) {
			//printf("CLI: %d %d %d\n", n1, n2, n3);
			arduino_cli = (n1 << 16) | (n2 << 8) | n3;
		}
		n1=0, n2=0, n3=0;
		const char *ide = strstr(arduino, "arduino-ide/");
		if (ide && sscanf(ide+12, "%d.%d.%d", &n1, &n2, &n3) == 3 &&
		  n1 > 0 && n1 < 256 && n2 > 0 && n2 < 256 && n3 > 0 && n3 < 256) {
			//printf("CLI: %d %d %d\n", n1, n2, n3);
			arduino_ide = (n1 << 16) | (n2 << 8) | n3;
		}
	}

	// decide how to print output
	if (json) {
		fout = stdout;
	} else {
		if (arduino_cli > 0 && arduino_ide == 0) {
			fout = stdout;
		}
		if (arduino_ide > 0x20000) {
			// Arduino 2.x.x only shows output if sterrr
			fout = stderr;
		}
		if (arduino_cli == 0 && arduino_ide == 0) {
			// Arduino 1.8.x discards info unless stderr
			fout = stderr;
			prefix = "teensy_size: "; // trick to print in white text
		}
	}

	// read and parse ELF data
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
	int r = parse_elf(filedata);
	if (r != 0) die("Unable to parse %s, err = %d\n", filename, r);


	int model = elf_teensy_model_id(filedata);
	if (!model) die("Can't determine Teensy model from %s\n", filename);

	//print_elf_info();
	//printf("Teensy Model is %02X (%s)\n", model, model_name(model));
	line("Memory Usage on %s:", model_name(model));

	if (model == 0x24 || model == 0x25 || model == 0x26) {

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
		uint32_t itcm_padding = itcm_total - itcm;
		uint32_t dtcm = data + bss;
		uint32_t ram2 = bss_dma;

		uint32_t bss_extram = 0;
		if (model == 0x25) bss_extram = elf_section_size(".bss.extram");

		int32_t free_flash = (int32_t)flash_size(model) - (int32_t)flash_total;
		int32_t free_for_local = 512*1024 - (int32_t)itcm_total - (int32_t)dtcm;
		int32_t free_for_malloc = (int32_t)512*1024 - (int32_t)ram2;

		if ((free_flash < 0) || (free_for_local <= 0) || (free_for_malloc < 0)) retval = -1;

		line("  FLASH: code:%u, data:%u, headers:%u   free for files:%d",
			flash_code, flash_data, flash_headers, free_flash);
		json_sections[0].name = "FLASH";
		json_sections[0].size = flash_total;
		json_sections[0].max_size = flash_size(model);
		line("   RAM1: variables:%u, code:%u, padding:%u   free for local variables:%d",
			dtcm, itcm, itcm_padding, free_for_local);
		json_sections[1].name = "RAM1";
		json_sections[1].size = itcm_total + dtcm;
		json_sections[1].max_size = 512*1024;
		line("   RAM2: variables:%u  free for malloc/new:%d",
			ram2, free_for_malloc);
		json_sections[2].name = "RAM2";
		json_sections[2].size = ram2;
		json_sections[2].max_size = 512*1024;
		if (bss_extram > 0) {
			line(" EXTRAM: variables:%u", bss_extram);
			json_sections[3].name = "EXTRAM";
			json_sections[3].size = bss_extram;
			json_sections[3].max_size = 32*1024*1024;
		}
	}
	else if (model >= 0x1D && model <= 0x22) { // Teensy 3.x and Teensy LC

		uint32_t data = elf_section_size(".data");
		uint32_t text = elf_section_size(".text");
		uint32_t fini = elf_section_size(".fini");
		uint32_t arm_exidx = elf_section_size(".ARM.exidx");
		uint32_t bss = elf_section_size(".bss");
		uint32_t noinit = elf_section_size(".noinit");
		uint32_t usbdesc = elf_section_size(".usbdescriptortable");
		uint32_t dmabuffers = elf_section_size(".dmabuffers");
		uint32_t usbbuffers = elf_section_size(".usbbuffers");
		//uint32_t = elf_section_size(".");
		uint32_t flash = text + data + fini + arm_exidx;
		uint32_t ram = data + bss + noinit + usbdesc + dmabuffers + usbbuffers;
		if (flash > flash_size(model) || ram > ram_size(model)) retval = -1;
		line("  Program uses %u bytes of flash storage. Maximum is %u bytes.",
			flash, flash_size(model));
		json_sections[0].name = "FLASH";
		json_sections[0].size = flash;
		json_sections[0].max_size = flash_size(model);
		line("  Variables use %u bytes dynamic memory, leaving %d bytes for local variables. Maximum is %u bytes.",
			ram, ram_size(model) - ram, ram_size(model));
		json_sections[1].name = "RAM";
		json_sections[1].size = ram;
		json_sections[1].max_size = ram_size(model);
	}
	else if (model >= 0x19 && model <= 0x1C) { // Teensy 2.0 and 1.0

		uint32_t data = elf_section_size(".data");
		uint32_t text = elf_section_size(".text");
		uint32_t bss = elf_section_size(".bss");
		uint32_t noinit = elf_section_size(".noinit");
		uint32_t flash = text + data;
		uint32_t ram = data + bss + noinit;
		if (flash > flash_size(model) || ram > ram_size(model)) retval = -1;
		line("  Program uses %u bytes of flash storage. Maximum is %u bytes.",
			flash, flash_size(model));
		json_sections[0].name = "FLASH";
		json_sections[0].size = flash;
		json_sections[0].max_size = flash_size(model);
		line("  Variables use %u bytes dynamic memory, leaving %d bytes for local variables. Maximum is %u bytes.",
			ram, ram_size(model) - ram, ram_size(model));
		json_sections[1].name = "RAM";
		json_sections[1].size = ram;
		json_sections[1].max_size = ram_size(model);
	}

	if (json) {
		printf("{\n");
		printf(" \"output\": \"");
		const char *p = get_output();
		while (*p) {
			if (*p == '\n') {
				printf("\\n");
			} else {
				printf("%c", *p);
			}
			p++;
		}
		printf("\",\n");
		if (retval == 0) {
			printf(" \"severity\": \"info\",\n");
		} else {
			printf(" \"severity\": \"error\",\n");
			printf(" \"error\": \"Exceeds memory limit\",\n");
		}
		printf(" \"sections\": [\n");
		int i=0, last=0;
		while (1) {
			if (i == 3 || json_sections[i+1].name == NULL) last = 1;
			printf("  { \"name\": \"%s\", \"size\": %u, \"max_size\": %u }%s\n",
				json_sections[i].name, json_sections[i].size,
				json_sections[i].max_size, ((last) ? "" : ","));
			if (last) break;
			i++;
		}
		printf(" ]\n");
		printf("}\n");
	} else {
		fprintf(fout, "%s", get_output());
		if (retval != 0) {
			fprintf(fout,"Error program exceeds memory space\n");
		}
		fflush(fout);
	}

	free(filedata);
	return retval;
}

size_t output_len = 0;
char output_buffer[8192];

void line(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int avail = sizeof(output_buffer) - output_len;
	if (avail < 100) return;
	if (prefix) {
		strcpy(output_buffer + output_len, prefix);
		output_len += strlen(prefix);
	}
	int n = vsnprintf(output_buffer + output_len, avail, format, args);
	if (n > avail) n = avail;
	output_len += n;
	output_buffer[output_len++] = '\n';
}

const char * get_output()
{
	output_buffer[output_len] = 0;
	return output_buffer;
}

void die(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	fprintf(stderr, "teensy_size: ");
	vfprintf(stderr, format, args);
	va_end(args);
	if (fp) fclose(fp);
	if (filedata) free(filedata);
	exit(1);
}

