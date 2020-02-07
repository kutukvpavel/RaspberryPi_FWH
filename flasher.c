#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <wiringPi.h> // Include WiringPi library!

const int rstPin = 17; //hd 11
const int lad0Pin = 22; //hd 15
const int lad1Pin = 23; //hd 16
const int lad2Pin = 24; //hd 18
const int lad3Pin = 25; //hd 22
const int lframePin = 27; //hd 13
const int lclkPin = 18; //hd 12
const int wrPin = 4; //hd 7

bool dbg = false;

void dbgPause(void)
{
	if (dbg)
	{
		printf("Press any key to continue...\n");
		getchar();
	}
}

void dbgPrint(char* str)
{
	if (dbg)
	{
		printf(str);
		printf("\n");
	}
}


void setLADOutputZ(bool zeroOut) {
	if (zeroOut)
	{
		digitalWrite(lad0Pin, LOW);
		digitalWrite(lad1Pin, LOW);
		digitalWrite(lad2Pin, LOW);
		digitalWrite(lad3Pin, LOW);
		dbgPrint("LAD GPIO zeroed out.");
	}
	pinMode(lad0Pin, OUTPUT);
	pinMode(lad1Pin, OUTPUT);
	pinMode(lad2Pin, OUTPUT);
	pinMode(lad3Pin, OUTPUT);
	dbgPrint("LAD GPIO switched to OUTput.");
	dbgPause();
}

void setLADOutput(void)
{
	setLADOutputZ(false);
}

void setLADInputZ(bool zeroOut) {
	pinMode(lad0Pin, INPUT);
	pinMode(lad1Pin, INPUT);
	pinMode(lad2Pin, INPUT);
	pinMode(lad3Pin, INPUT);
	dbgPrint("LAD GPIO switched to INput.");
	if (zeroOut)
	{
		digitalWrite(lad0Pin, LOW);
		digitalWrite(lad1Pin, LOW);
		digitalWrite(lad2Pin, LOW);
		digitalWrite(lad3Pin, LOW);
		dbgPrint("LAD GPIO zeroed out.");
	}
	dbgPause();
}

void setLADInput(void)
{
	setLADInputZ(false);
}


void writeLAD(unsigned char data, unsigned char startFrame) {
	digitalWrite(lclkPin, HIGH); //Data is latched on rising edge. Timings should be well in-spec.
	dbgPrint("LAD+LFRAME written, CLK high.");
	dbgPause();
	if(data & 0x1) {
		digitalWrite(lad0Pin, HIGH);
	} else {
		digitalWrite(lad0Pin, LOW);
	}
	if(data & 0x2) {
		digitalWrite(lad1Pin, HIGH);
	} else {
		digitalWrite(lad1Pin, LOW);
	}
	if(data & 0x4) {
		digitalWrite(lad2Pin, HIGH);
	} else {
		digitalWrite(lad2Pin, LOW);
	}
	if(data & 0x8) {
		digitalWrite(lad3Pin, HIGH);
	} else {
		digitalWrite(lad3Pin, LOW);
	}
	if (startFrame) {
		digitalWrite(lframePin, LOW);
	}
	else {
		digitalWrite(lframePin, HIGH);
	}
	usleep(100);
	digitalWrite(lclkPin, LOW); //(LCLK minimum half-period is 11ns, while RPi is only capable of ~100nS with wiringPi)
	usleep(100);
}

unsigned char readLAD(void) {
	unsigned char data;
	usleep(100);
	digitalWrite(lclkPin, HIGH);
	dbgPrint("Reading data: clock is high.");
	dbgPause();
	usleep(100);
	data = 0;
	if(digitalRead(lad0Pin)) data |= 0x01;
	if(digitalRead(lad1Pin)) data |= 0x02;
	if(digitalRead(lad2Pin)) data |= 0x04;
	if(digitalRead(lad3Pin)) data |= 0x08;
	if (dbg)
	{
		printf("Read nibble: 0x%hhx\n", data);
	}
	digitalWrite(lclkPin, LOW);
	return data;
}

void preparePinMode(void) {
	digitalWrite(rstPin, LOW);
	digitalWrite(lclkPin, LOW);
	digitalWrite(lframePin, HIGH);
	digitalWrite(wrPin, LOW);
	digitalWrite(lad0Pin, LOW);
	digitalWrite(lad1Pin, LOW);
	digitalWrite(lad2Pin, LOW);
	digitalWrite(lad3Pin, LOW);
	dbgPrint("preparePinMode phase 1");
	dbgPause();
	setLADInput();
	pinMode(rstPin, OUTPUT);
	pinMode(wrPin, OUTPUT);
	pinMode(lframePin, OUTPUT);
	pinMode(lclkPin, OUTPUT);
	dbgPrint("preparePinMode phase 2");
	dbgPause();
	usleep(2000);
	digitalWrite(rstPin, HIGH);
	usleep(1000);
	dbgPrint("preparePinMode finished. Reset is high.");
	dbgPause();
}

unsigned int len2mSizeRead(unsigned int len) {
	switch(len) {
		case 1: return 0;
		case 2: return 1;
		case 4: return 2;
		case 16: return 4;
		case 128: return 7;
		default: 
			printf("Bad len");
			exit(0);
	}
}

unsigned int len2mSizeWrite(unsigned int len) {
	switch(len) {
		case 1: return 0;
		case 2: return 1;
		case 4: return 2;
		default: 
			printf("Bad len");
			exit(0);
	}
}

bool readCycle(unsigned char *buffer, unsigned long startAddr, unsigned int len) {
	bool ret = false; //Returns true if generates a warning
	dbgPrint("Read cycle begins.");
	//unsigned int addr;
	setLADOutput();
	dbgPrint("Write start nibble.");
	writeLAD(0x0d, 1); //Start mem read
	dbgPrint("Write IDSEL.");
	writeLAD(0x0, 0); //IDSEL=0000 (internally pulled low)
	//7 Addr cycles
	dbgPrint("Write address (7 nibbles).");
	//writeLAD((startAddr >> 28) & 0xF, 0); WTF? IDSEL encoded into start address? Not needed.
	writeLAD((startAddr >> 24) & 0xF, 0);
	writeLAD((startAddr >> 20) & 0xF, 0);
	writeLAD((startAddr >> 16) & 0xF, 0);
	writeLAD((startAddr >> 12) & 0xF, 0);
	writeLAD((startAddr >> 8) & 0xF, 0);
	writeLAD((startAddr >> 4) & 0xF, 0);
	writeLAD((startAddr >> 0) & 0xF, 0);
	//MSIZE, 49lf004b only supports single-byte mode (0000).
	dbgPrint("Write MSIZE.");
	writeLAD(0x0, 0);
	//TAR0 "turnaround cycle" start (1111)
	dbgPrint("Start turnaround cycle.");
	writeLAD(0xF, 0);
	setLADInputZ(true); //No clock here
	//TAR1: Float to 1111: do not sample
	usleep(100);
	dbgPrint("Not a read: clock pulse for TAR1 float-to-1111 transition.");
	readLAD();
	usleep(100);
	//RSYNC
	dbgPrint("Reading RSYNC...");
	unsigned char d = readLAD();
	if(d != 0) {
		printf("RSYNC not zero: %01x\n", d);
		if (!dbg) exit(1);
	}
	//DATA fetching
	//49lf004b does not support multiple-byte reads
	/*
	addr = 0;
	for(addr = 0; addr < len; addr++) {
		d = readLAD();
		d |= readLAD() << 4;
		buffer[addr] = d;
	}*/
	dbgPrint("Reading lower nibble...");
	d = readLAD();
	dbgPrint("Reading higher nibble...");
	d |= readLAD() << 4;
	buffer[0] = d;
	dbgPrint("Reading TAR0...");
	if((d = readLAD()) != 0xF) {
		printf("\nTAR0 not all ones: %01x\n", d); //\n is a workaround for progress display (see main)
		ret = true;
		//exit(1); This is not critical, the chip holds the bus high only for 28nS.
		//The value we read here is going to depend on stray capacitance and pin impedance.
	}
	//TAR1 - regain control over the bus.
	dbgPrint("Not a read: clock pulse for TAR1 (regaining control)...");
	readLAD();
	return ret;
}

void writeCycle(unsigned char *buffer, unsigned long startAddr, unsigned int len) {
	unsigned int addr;
	unsigned char d;
	setLADOutput();
	writeLAD(0x0e, 1);
	writeLAD((startAddr >> 28) & 0xF, 0);
	writeLAD((startAddr >> 24) & 0xF, 0);
	writeLAD((startAddr >> 20) & 0xF, 0);
	writeLAD((startAddr >> 16) & 0xF, 0);
	writeLAD((startAddr >> 12) & 0xF, 0);
	writeLAD((startAddr >> 8) & 0xF, 0);
	writeLAD((startAddr >> 4) & 0xF, 0);
	writeLAD((startAddr >> 0) & 0xF, 0);
	writeLAD(len2mSizeWrite(len), 0);
	for(addr = 0; addr < len; addr++) {
		writeLAD(buffer[addr] & 0xF , 0);
		writeLAD((buffer[addr] >> 4) & 0xF , 0);
	}
	writeLAD(0xF, 0);
	setLADInput();
	usleep(1000);
	readLAD();
	if(readLAD() != 0) {
		printf("RSYNC not zero\n");
		exit(1);
	}
	if((d = readLAD()) != 0xF) {
		printf("TAR0 not all ones %01x\n", d);
		exit(1);
	}
}

unsigned char readStatusRegister() {
	unsigned char buffer[1];
	buffer[0] = 0x70;
	writeCycle(buffer, 0x0ffc0000, 1);
	readCycle(buffer, 0x0ffc0000, 1);
	return buffer[0];
}

void waitForWriteComplete() {
	while(readStatusRegister() & 0x01 == 0x01) {
		usleep(10);
	}
}

int main( int argc, char **argv )
{
	unsigned long addr, start, length, seek;
	unsigned int len, i, readLen;
	unsigned char buffer[128];
	unsigned char buffer2[128], cmdW, cmdR;
	char flash, erase, readF, verify, id;
	int fileHandle;
	char *fileName;

	id = 0;
	start = 0x0;
	length = 0x80000;
	len = 0x1;
	fileName = 0;
	cmdR = 0xff;
	cmdW = 0x10;
	seek = 0;
	erase = 0;
	flash = 0;
	readF = 0;
	verify = 0;
	i = 1;
	if(argc == 1) {
		i = 0;
	}
	while(i < argc) {
		if((strcmp(argv[i], "-w") == 0)) {
			flash = 1;
		}
		else if((strcmp(argv[i], "-e") == 0)) {
			erase = 1;
		}
		else if((strcmp(argv[i], "-r") == 0)) {
			readF = 1;
		}
		else if((strcmp(argv[i], "-v") == 0)) {
			verify = 1;
		}
		else if(strcmp(argv[i], "-i") == 0)
		{
			id = 1;
		}
		else  if((strcmp(argv[i], "-f") == 0) && (i+1 < argc)) {
			fileName = argv[++i];
			printf("Writing (reading) to file %s\n", fileName);
		}
		else if((strcmp(argv[i], "-s") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%lx", &start);
		}
		else if((strcmp(argv[i], "-o") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%lx", &seek);
			printf("Seek in input file 0x%lx\n", seek);
		}
		else if((strcmp(argv[i], "-l") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%lx", &length);
		}
		else if((strcmp(argv[i], "-b") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%x", &len);
		}
		else if((strcmp(argv[i], "-cW") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%hhx", &cmdW);
		}
		else if((strcmp(argv[i], "-cR") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%hhx", &cmdR);
		}
		else if(strcmp(argv[i], "-d") == 0)
		{
			dbg = true;
		}
		else {
			printf("SST49LF016C flash programmer *modified to read SST49LF004B*\n");
			printf("Usage: %s parameters\n", argv[0]);
			printf("Parameters:\n");
			//printf(" -w                Write to flash (flash is not erased)\n");
			//printf(" -e                Erase flash sectors before writing (sector size is 4KB, erases all sectors in writing range)\n");
			printf(" -r                Read the flash\n");
			printf(" -v                Verify the flash\n");
			printf(" -f  filename      Specifies file for writing, reading, verifying\n");
			printf(" -s  hex (32-bit)  Sets start address (hex, default = 0x0)\n");
			printf(" -o  hex (32-bit)  Offset in file - Seeks in input file before operation\t\n");
			printf(" -l  hex (32-bit)  R/W Length (default = 0x80000)\t\n");
			//printf(" -b  hex (8-bit)   Block size (allowed sizes are RW:0x1,0x2,0x4; R:0x10,0x80\n");
			//printf(" -cW hex (8-bit)   Chip Command for writing (default 0x40)\n");
			//printf(" -cR hex (8-bit)   Chip Command for reading (default 0xff)\n");
			printf(" -d                LED-Debug mode\n");
			exit(0);
		}
		i++;
	}
	printf("Starting address 0x%lx\n", start);
	printf("Length 0x%lx\n", length);
	printf("Block size 0x%x\n", len);
	//printf("Command for Writing 0x%x\n", cmdW);
	//printf("Command for Reading 0x%x\n", cmdR);

	fileHandle = -1;
	if(flash || verify) {
		if(fileName) fileHandle = open(fileName, O_RDONLY);
	} else {
		if(fileName) fileHandle = open(fileName, O_WRONLY | O_CREAT);
	}
	if(flash && fileHandle == -1) {
		printf("Cannot program flash without file (use -f)\n");
		exit(1);
	}

	printf("Preparing pins\n");
	wiringPiSetupGpio();
	preparePinMode();

	if (id)
	{
		printf("Reading manufacturer ID...\n");
		readCycle(buffer, 0xFFBC0000, 1);
		printf("Manufacturer ID: 0x%hhx\n", buffer[0]);
		printf("Reading chip ID...\n");
		readCycle(buffer, 0xFFBC0001, 1);
		printf("Chip ID: 0x%hhx\n", buffer[0]);
	}

	if(flash || erase) {
		printf("Erase SCS not implemented!");
		/*
		buffer[0] = 0;
		for(addr = 0x0fa00002; addr <= 0x0fbf0002; addr += 0x00010000) {
			writeCycle(buffer, addr, 1);
		};
		writeCycle(buffer, 0x0fbf8002, 1); - these Software Commands are not implemented in 49lf004b
		writeCycle(buffer, 0x0fbfa002, 1);
		writeCycle(buffer, 0x0fbfc002, 1);
		if(erase) {
			for(addr = start; addr < start + length; addr += 0x1000) {
				printf("Erasing sector %lx\n", addr);
				buffer[0] = 0x30;
				writeCycle(buffer, addr, 1);
				buffer[0] = 0xd0;
				writeCycle(buffer, addr, 1);
				usleep(50000);
			}
			addr = 0x5ff980;
			if(start + length > addr) {
				printf("Erasing sector %lx\n", addr);
				buffer[0] = 0x30;
				writeCycle(buffer, addr, 1);
				buffer[0] = 0xd0;
				writeCycle(buffer, addr, 1);
				usleep(50000);
			}
		}*/
	}

	if(flash) {
		printf("Write SCS not implemented!");
		/*
		printf("Writing\n");
		if((lseek(fileHandle, 0, SEEK_END)) % len != 0) {
			printf("File size is not multiple of block length\n");
			exit(1);
		}
		lseek(fileHandle, seek, SEEK_SET);
		for(addr = start; addr < start+length; addr += len) {
			printf("%08lx\r", addr);
			buffer[0] = cmdW;
			writeCycle(buffer, 0x0ffc0000, 1); - these Software Commands are not implemented in 49lf004b
			readLen = read(fileHandle, buffer, len);
			if(readLen != len) {
				printf("Not whole block was read\n");
				exit(1);
			}
			if(readLen == 0) {
				printf("Unexpected end of file");
				exit(1);
			}
			writeCycle(buffer, addr, readLen); 
			waitForWriteComplete();
		}
		printf("\n");
		usleep(1000);
		*/
	}

	if(verify) {
		//buffer[0] = cmdR; - these Software Commands are not implemented in 49lf004b
		//writeCycle(buffer, 0x0ffc0000, 1);
		printf("Verifying\n");
		if(fileHandle != -1) {
			lseek(fileHandle, seek, SEEK_SET);
			for(addr = start; addr < start+length; addr += len) {
				readCycle(buffer, addr | 0x400000, len); //Bit 22 directs reads to flash (not registers)
				printf("%08lx\r", addr);
				readLen = read(fileHandle, buffer2, len);
				if(memcmp(buffer, buffer2, len) != 0) {
					for(i = 0; i < len; i++) {
						if(buffer[i] != buffer2[i]) {
							printf("Verify error at addr %08lx R:%02x F:%02x\n", addr + i, buffer[i], buffer2[i]);
						}
					}
					exit(1);
				}
			}
		}
	}
	if(readF) {
		//buffer[0] = cmdR; - these Software Commands are not implemented in 49lf004b
		//writeCycle(buffer, 0x0ffc0000, 1);
		printf("Reading...\n");
		for(addr = start; addr < start+length; addr += len) {
			printf("\r%2d%%", (int)((100 * (addr - start)) / length));
			if (readCycle(buffer, addr | 0x400000, len)) //Bit 22 directs reads to flash (not registers)
			{
				printf("Warning was generated at address 0x%lx\n", addr);
			}
			if(fileHandle != -1) {
				write(fileHandle, buffer, len);
			}
			//printf("%08lx: ", addr);
			/*for(i = 0; i < len; i++) {
				printf("%02x ", buffer[i]);
			}*/
			/*for(i = 0; i < len; i++) {
				if(buffer[i] >= 32 && buffer[i]<127) {
					printf("%c", buffer[i]);
				} else {
					printf(".");
				}
			}*/
			//printf("\n");
		}
		printf("\n");
	}
	close(fileHandle);
	return 0;
}