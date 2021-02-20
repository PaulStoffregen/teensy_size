#include <string.h>
#include <stdio.h>
#include "teensy_info.h"

int is_teensy(int vid, int pid, int ver)
{
	if (vid != 0x16C0) return 0;
	if (pid < 0x0474 || pid > 0x04D7) return 0;
	return 1;
}

const char *teensy_pid_to_name(int vid, int pid, int ver)
{
	switch (pid) {
	  case 0x0478: return "Bootloader";
	  case 0x0483: return "Serial"; // USB_SERIAL
	  case 0x04D0: return "Keyboard"; // USB_KEYBOARDONLY
	  case 0x0482: return "Keyboard+Mouse+Joystick"; // USB_HID
	  case 0x0487: return "Serial+Keyboard+Mouse+Joystick"; // USB_SERIAL_HID
	  case 0x04D3: return "Keyboard+TouchScreen"; // USB_TOUCHSCREEN
	  case 0x04D4: return "Keyboard+Mouse+TouchScreen"; // USB_HID_TOUCHSCREEN
	  case 0x0485: // USB MIDI
		if (ver == 0x0212) return "MIDIx16";
		if (ver == 0x0211) return "MIDIx4";
		return "MIDI";
	  case 0x0489: // USB_MIDI_SERIAL
		if (ver == 0x0212) return "Serial+MIDIx16";
		if (ver == 0x0211) return "Serial+MIDIx4";
		return "Serial+MIDI";
	  case 0x0486: return "RawHID"; // USB_RAWHID
	  case 0x0488: return "FlightSim"; // USB_FLIGHTSIM
	  case 0x04D9: return "FlightSim+Joystick"; // USB_FLIGHTSIM_JOYSTICK
	  case 0x04D1: return "MTPDisk"; // USB_MTPDISK
	  case 0x04D2: return "Audio"; // USB_AUDIO
	  case 0x048A: // USB_MIDI_AUDIO_SERIAL
		if (ver == 0x0212) return "Serial+MIDIx16+Audio";
		return "Serial+MIDI+Audio";
	  case 0x048B: return "Dual Serial"; // USB_DUAL_SERIAL
	  case 0x048C: return "Triple Serial"; // USB_TRIPLE_SERIAL
	  case 0x0476:
		return "Everything"; // USB_EVERYTHING
	}
	return "Unknown";
}

int serialemu_inface(int vid, int pid, int ver)
{
	if (is_teensy(vid, pid, ver)) {
		switch (pid) {
		  case 0x04D0: return 1; // USB_KEYBOARDONLY
		  case 0x0482: return 2; // USB_HID
		  case 0x04D3: return 1; // USB_TOUCHSCREEN
		  case 0x04D4: return 2; // USB_HID_TOUCHSCREEN
		  case 0x0485: return 1; // USB MIDI
		  case 0x0486: return 1; // USB_RAWHID
		  case 0x0488: return 1; // USB_FLIGHTSIM
		  case 0x04D9: return 1; // USB_FLIGHTSIM_JOYSTICK
		  case 0x04D1: return 1; // USB_MTPDISK
		  case 0x04D2: return 0; // USB_AUDIO
		  case 0x0484: return 2; // USB_DISK (Teensy 2.0)
		}
	}
	return -1; // this PID-VER does not use HID for serial emulation
}

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

int model_num_from_ver(int vid, int pid, int ver)
{
	if (vid == 0x16C0 && pid >= 0x0474 && pid <= 0x04DF) {
		if (ver == 0x0271) return 0x1B; // Teensy 2.0
		if (ver == 0x0272) return 0x1C; // Teensy++ 2.0
		if (ver == 0x0273) return 0x20; // Teensy LC
		if (ver == 0x0274) return 0x1D; // Teensy 3.0
		if (ver == 0x0275) return 0x21; // Teensy 3.2 (or 3.1)
		if (ver == 0x0276) return 0x1F; // Teensy 3.5
		if (ver == 0x0277) return 0x22; // Teensy 3.6
		if (ver == 0x0278) return 0x23; // Teensy 4-Beta1
		if (ver == 0x0279) return 0x24; // Teensy 4.0
		if (ver == 0x0280) return 0x25; // Teensy 4.1
		if (ver == 0x0281) return 0x26; // Teensy MicroMod
	}
	return 0;
}

#define DEBUG_TX_SIZE 64
#define DEBUG_RX_SIZE 32
static const char reportdesc[] = {
	0x06, 0xC9, 0xFF,			// Usage Page 0xFFC9 (vendor defined)
	0x09, 0x04,				// Usage 0x04
	0xA1, 0x5C,				// Collection 0x5C
	0x75, 0x08,				// report size = 8 bits (global)
	0x15, 0x00,				// logical minimum = 0 (global)
	0x26, 0xFF, 0x00,			// logical maximum = 255 (global)
	0x95, DEBUG_TX_SIZE,			// report count (global)
	0x09, 0x75,				// usage (local)
	0x81, 0x02,				// Input
	0x95, DEBUG_RX_SIZE,			// report count (global)
	0x09, 0x76,				// usage (local)
	0x91, 0x02,				// Output
	0x95, 0x04,				// report count (global)
	0x09, 0x76,				// usage (local)
	0xB1, 0x02,				// Feature
	0xC0
};

int is_hidreport_serialemu(const void *data)
{
	if (memcmp(data, reportdesc, 9) == 0) return 1;
	return 0;
}


int is_hidreport_bootloader(const void *data)
{
	const unsigned char *p;
	p = (const unsigned char *)data;
	if (p[0] == 0x06 && p[1] == 0x9C && p[2] == 0xFF
	  && p[3] == 0x09 && p[5] == 0xA1 && p[6] == 0x88
	  && p[7] == 0x75 && p[8] == 0x08 && p[9] == 0x15
	  && p[4] >= 0x19 && p[4] <= 0x2C) {
		return p[4];
	}
	return 0;
}

uint32_t teensy_serial_number(const char *ser, int vid, int pid, int ver)
{
	uint32_t num=0;

	if (!ser || *ser == 0) return 0;
	if (vid == 0x16C0 && pid == 0x0478) {
		if (sscanf(ser, "%x", &num) != 1) return 0;
		return num;
	}
	if (sscanf(ser, "%u", &num) != 1) return 0;
	if (num == 12345) return 0; // fixed number in Teensy 2.0
	if (num == 0xFFFFFFFF) return 0; // no serial # in custom boards
	if (num <= 99999990 && (num % 10) == 0) return num / 10;
	if (num >= 10000000) return num;
	return 0;
}





