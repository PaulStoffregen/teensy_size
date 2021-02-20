#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "elf.h"
#include "teensy_info.h"

void die(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

unsigned char *filedata = NULL;
FILE *fp = NULL;

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

	print_elf_info();


	int model = elf_teensy_model_id(filedata);
	if (!model) die("Can't determine Teensy model from %s\n", filename);
	printf("Teensy Model is %02X (%s)\n", model, model_name(model));


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

