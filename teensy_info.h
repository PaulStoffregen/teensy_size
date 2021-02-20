#ifndef _teensy_info_h_
#define _teensy_info_h_

#include <stdint.h>

int is_teensy(int vid, int pid, int ver);
const char *teensy_pid_to_name(int vid, int pid, int ver);
int serialemu_inface(int vid, int pid, int ver);
const char * model_name(int num);
int model_num_from_ver(int vid, int pid, int ver);
int is_hidreport_serialemu(const void *data);
int is_hidreport_bootloader(const void *data);
uint32_t teensy_serial_number(const char *ser, int vid, int pid, int ver);

#endif
