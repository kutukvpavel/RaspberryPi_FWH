#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <wiringPi.h>

#define MAX_BLOCK_LEN 128u
#define FLASH_SELECT_ADDR 0x400000 //Bit 22 directs reads to flash (not registers)

const int rstPin = 17; //hd 11
const int lad0Pin = 22; //hd 15
const int lad1Pin = 23; //hd 16
const int lad2Pin = 24; //hd 18
const int lad3Pin = 25; //hd 22
const int lframePin = 27; //hd 13
const int lclkPin = 18; //hd 12
const int wrPin = 4; //hd 7

bool dbg = false;
int fileHandle = -1;

void safeExit(int code)
#ifdef __GNUC__
__attribute__((noreturn));
#else
;
#endif

void enableWrite(bool);
void setLADInputZ(bool);

typedef struct
{
	const char* Name;
	const unsigned char ManufacturerID;
	const unsigned char ChipID;
	const bool WriteOneshot;
	const bool ReadOneshot;
	const unsigned char WriteSCSCycles;
	const unsigned char ReadSCSCycles;
	const unsigned char* WriteCommand;
	const unsigned long* WriteAddress;
	const unsigned char* ReadCommand;
	const unsigned long* ReadAddress;
} Device;

const unsigned long SST49LF004B_WriteAddr[] = { 0x75555, 0x72AAA, 0x75555 };
const unsigned char SST49LF004B_WriteCmd[] = { 0xAA, 0x55, 0xA0 };

const Device SST49LF004B =
{
	.Name = "SST49LF004B",
	.ManufacturerID = 0xBF,
	.ChipID = 0x60,
	.WriteOneshot = true,
	.ReadOneshot = false,
	.WriteSCSCycles = 3,
	.ReadSCSCycles = 0,
	.WriteCommand = SST49LF004B_WriteCmd,
	.WriteAddress = SST49LF004B_WriteAddr,
	.ReadCommand = NULL,
	.ReadAddress = NULL
};

#define SUPPORTED_DEV_NUMBER 1
const Device SupportedDevices[] = { SST49LF004B };