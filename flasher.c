/*

	Raspberry Pi FWH flasher. Original source code taken from: http://ponyservis.blogspot.com/p/programming-lpc-flash-using-raspberry-pi.html
	Modified by Kutukov Pavel 2020 for SST49LF004B.
	TODO (to make this tool more-ore-less universal):
	- add an option to control delays;
	- add an option to control IDSEL;
	- add SCS table for common ICs to enable fast-write and erase operations;
	- implement automatic IC detection;

*/

#include "flasher.h"

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

//Convention: code 0 is OK, code 1 is ERROR, code 2 is Bad Input
void safeExit(int code)
{
	digitalWrite(rstPin, LOW);
	pinMode(rstPin, OUTPUT);
	enableWrite(false);
	setLADInputZ(true);
	pinMode(wrPin, INPUT);
	pinMode(lframePin, INPUT);
	pinMode(lclkPin, INPUT);
	if (fileHandle != -1) close(fileHandle);
	exit(code);
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
	//Data is latched on rising edge (clock has to be PCI-compliant). Timings should be well in-spec:
	//LCLK minimum half-period is 11ns, while RPi is only capable of ~100nS minimum pulse width with wiringPi)
	//Therefore I really don't understand why LCLK is driven high before the actual writing to LAD[3:0]
	//But changing the order results in garbage being received (data stream gets shifted by a nibble and is misinterpreted).
	digitalWrite(lclkPin, HIGH);
	if (dbg) printf("Previous (?) LAD+LFRAME written, CLK high. Writing new (?) value: 0x%hhx", data);
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
	//My setup uses a breadboard and some long-ish wires, therefore I've uncommented all delays.
	usleep(100);
	digitalWrite(lclkPin, LOW);
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

void enableWrite(bool value)
{
	if (value)
	{
		digitalWrite(wrPin, LOW);
	}
	else
	{
		digitalWrite(wrPin, HIGH);
	}
}

void preparePinMode(void) {
	digitalWrite(rstPin, LOW);
	digitalWrite(lclkPin, LOW);
	digitalWrite(lframePin, HIGH);
	enableWrite(false);
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

//According to SST49LF016C datasheet
unsigned int len2mSizeRead(unsigned int len) {
	switch(len) {
		case 1: return 0;
		case 2: return 1;
		case 4: return 2;
		case 16: return 4;
		case 128: return 7;
		default: 
			printf("Bad block length specified!\n");
			safeExit(2);
	}
}

//According to SST49LF016C datasheet
unsigned int len2mSizeWrite(unsigned int len) {
	switch(len) {
		case 1: return 0;
		case 2: return 1;
		case 4: return 2;
		default: 
			printf("Bad block length specified!\n");
			safeExit(2);
	}
}

//Should be suitable for all FWH chips now.
bool readCycle(unsigned char *buffer, unsigned long startAddr, unsigned int len) {
	bool ret = false; //Returns true if generates a warning
	dbgPrint("Read cycle begins.");
	unsigned int addr;
	setLADOutput();
	dbgPrint("Write start nibble.");
	writeLAD(0x0d, 1); //Start mem read
	dbgPrint("Write IDSEL.");
	writeLAD(0x0, 0); //IDSEL=0000 (internally pulled low)
	//7 Addr cycles
	dbgPrint("Write address (7 nibbles).");
	writeLAD((startAddr >> 24) & 0xF, 0);
	writeLAD((startAddr >> 20) & 0xF, 0);
	writeLAD((startAddr >> 16) & 0xF, 0);
	writeLAD((startAddr >> 12) & 0xF, 0);
	writeLAD((startAddr >> 8) & 0xF, 0);
	writeLAD((startAddr >> 4) & 0xF, 0);
	writeLAD((startAddr >> 0) & 0xF, 0);
	//MSIZE, 49lf004b and similar ones only support single-byte mode (0000).
	dbgPrint("Write MSIZE.");
	writeLAD(len2mSizeRead(len), 0);
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
		if (!dbg) safeExit(1);
	}
	//DATA fetching
	addr = 0;
	for(addr = 0; addr < len; addr++) {
		if (dbg) printf("Byte %u:\n", addr);
		dbgPrint("Reading lower nibble...");
		d = readLAD();
		dbgPrint("Reading higher nibble...");
		d |= readLAD() << 4;
		buffer[addr] = d;
	}
	dbgPrint("Reading TAR0...");
	if((d = readLAD()) != 0xF) {
		printf("\nTAR0 not all ones: %01x\n", d); //\n is a workaround for progress display (see main)
		ret = true;
		//exit(1); This is not critical, the chip holds the bus high only for 28nS, if I'm not mistaken.
		//The value we read here is going to depend on stray capacitance and pin impedance.
	}
	//TAR1 - regain control over the bus.
	dbgPrint("Not a read: clock pulse for TAR1 (regaining control)...");
	readLAD();
	return ret;
}

//Should be suitable for all FWH chips now.
void writeCycle(unsigned char *buffer, unsigned long startAddr, unsigned int len) {
	unsigned int addr;
	unsigned char d;
	setLADOutput();
	writeLAD(0x0e, 1);
	writeLAD(0x0, 0); //IDSEL=0000 (internally pulled low)
	//7 Addr cycles
	writeLAD((startAddr >> 24) & 0xF, 0);
	writeLAD((startAddr >> 20) & 0xF, 0);
	writeLAD((startAddr >> 16) & 0xF, 0);
	writeLAD((startAddr >> 12) & 0xF, 0);
	writeLAD((startAddr >> 8) & 0xF, 0);
	writeLAD((startAddr >> 4) & 0xF, 0);
	writeLAD((startAddr >> 0) & 0xF, 0);
	//MSIZE
	writeLAD(len2mSizeWrite(len), 0);
	//Data
	for(addr = 0; addr < len; addr++) {
		writeLAD(buffer[addr] & 0xF , 0);
		writeLAD((buffer[addr] >> 4) & 0xF , 0);
	}
	//TAR0
	writeLAD(0xF, 0);
	setLADInput();
	usleep(1000);
	//TAR1
	readLAD();
	//RSYNC
	if(readLAD() != 0) {
		printf("RSYNC not zero!\n");
		safeExit(1);
	}
	//TAR0
	if((d = readLAD()) != 0xF) {
		printf("TAR0 not all ones during a write cycle: 0x%hhx!\n", d);
		safeExit(1);
	}
	//TAR1
	readLAD();
}

unsigned char readStatusRegister() {
	unsigned char buffer[1];
	buffer[0] = 0x70;
	writeCycle(buffer, 0x0ffc0000, 1);
	readCycle(buffer, 0x0ffc0000, 1);
	return buffer[0];
}

void waitForWriteComplete() {
	while((readStatusRegister() & 0x01) == 0x01) {
		usleep(10);
	}
}

void readIDs(unsigned char* ret)
{
	unsigned char buffer[3];
	printf("Reading manufacturer ID...\n");
	readCycle(buffer, 0xFFBC0000, 1); //From SST49LF004B datasheet. JEDEC-defined registers should be the same for all FWH chips.
	printf("Manufacturer ID: 0x%hhx\n", buffer[0]);
	ret[0] = buffer[0];
	printf("Reading chip ID...\n");
	readCycle(buffer, 0xFFBC0001, 1);
	printf("Chip ID: 0x%hhx\n", buffer[0]);
	ret[1] = buffer[0];
}

void eraseChip(void)
{
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

void flashChip(void)
{
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

void verifyChip(unsigned long seek, unsigned long start, unsigned long length, unsigned int len,
	unsigned char* buffer, unsigned char* buffer2)
{
	unsigned long addr, end = start + length;
	unsigned int readLen;
	//buffer[0] = cmdR; - these Software Commands are not implemented in 49lf004b
	//writeCycle(buffer, 0x0ffc0000, 1);
	printf("Verifying...\n");
	if (fileHandle != -1) {
		lseek(fileHandle, seek, SEEK_SET);
		for (addr = start; addr < end; addr += len) {
			readCycle(buffer, addr | FLASH_SELECT_ADDR, len);
			printf("%08lx\r", addr);
			readLen = read(fileHandle, buffer2, len);
			if (readLen != len)
			{
				printf("Read wrong block size from the file: expected = %u; read = %u.", len, readLen);
			}
			if (memcmp(buffer, buffer2, len) != 0) {
				for (unsigned int i = 0; i < len; i++) {
					if (buffer[i] != buffer2[i]) {
						printf("Verify error at address %08lx R:%02x F:%02x\n", addr + i, buffer[i], buffer2[i]);
					}
				}
				safeExit(1);
			}
		}
	}
}

void readChip(unsigned long start, unsigned long length, unsigned int len, unsigned char* buffer)
{
	unsigned long addr, end;
	//buffer[0] = cmdR; - these Software Commands are not implemented in 49lf004b
	//writeCycle(buffer, 0x0ffc0000, 1);
	printf("Reading...\n");
	end = start + length;
	for (addr = start; addr < end; addr += len) {
		if (fileHandle != -1)
		{
			//Display progress indicator (useful for large reads that are usually saved into a file)
			printf("\r%2d%%", (int)((100 * (addr - start)) / length));
			//Returns true if it had printed a warning.
			if (readCycle(buffer, addr | FLASH_SELECT_ADDR, len))
			{
				printf("The warning was generated at address 0x%lx\n", addr);
			}
			write(fileHandle, buffer, len);
		}
		else
		{
			//Display the contents in real time (useful for short reads)
			printf("%08lx: ", addr);
			readCycle(buffer, addr | FLASH_SELECT_ADDR, len); //Bit 22 directs reads to flash (not registers)
			unsigned int i;
			for (i = 0; i < len; i++) {
				printf("%02x ", buffer[i]);
			}
			for (i = 0; i < len; i++) {
				if (buffer[i] >= 32 && buffer[i]<127) {
					printf("%c", buffer[i]);
				}
				else {
					printf(".");
				}
			}
			printf("\n");
		}
	}
	printf("\n");
}

void compatibleEraseChip(unsigned long start, unsigned long length, unsigned int len, unsigned char* buffer)
{
	enableWrite(true);
	unsigned long addr, end = length + start;
	buffer[0] = 0;
	printf("Writing zeros...\n");
	for (addr = start; addr < end; addr += len)
	{
		printf("\r%2x%%", (int)((100 * (addr - start)) / length));
		writeCycle(buffer, addr | FLASH_SELECT_ADDR, 1);
	}
	enableWrite(false);
	printf("\n");
	usleep(1000);
}

void compatibleFlashChip(unsigned long seek, unsigned long start, unsigned long length, unsigned int len, unsigned char* buffer)
{
	enableWrite(true);
	unsigned long addr, end = length + start;
	unsigned int readLen;
	printf("Writing...\n");
	if ((lseek(fileHandle, 0, SEEK_END)) % len != 0) {
		printf("File size is not multiple of block size!\n");
		safeExit(2);
	}
	lseek(fileHandle, seek, SEEK_SET);
	for (addr = start; addr < end; addr += len) {
		printf("%08lx\r", addr);
		readLen = read(fileHandle, buffer, len);
		if (readLen == 0) {
			printf("Unexpected end of file!\n");
			safeExit(2);
		}
		if (readLen != len) {
			printf("Can not read block at 0x%lx from the file!\n", addr);
			safeExit(1);
		}
		writeCycle(buffer, addr | FLASH_SELECT_ADDR, len);
	}
	enableWrite(false);
	printf("\n");
	usleep(1000);
}

void executeSCS(const Device* dev, bool w)
{
	unsigned char buf[1];
	if (w)
	{
		for (unsigned char i = 0; i < dev->WriteSCSCycles; i++)
		{
			*buf = dev->WriteCommand[i];
			writeCycle(buf, dev->WriteAddress[i], 1);
		}
	}
	else
	{
		for (unsigned char i = 0; i < dev->ReadSCSCycles; i++)
		{
			*buf = dev->ReadCommand[i];
			writeCycle(buf, dev->ReadAddress[i], 1);
		}
	}
}

const Device* findDevice(unsigned char* ID)
{
	unsigned int i;
	for (i = 0; i < SUPPORTED_DEV_NUMBER; i++)
	{
		if ((SupportedDevices[i].ManufacturerID == ID[0]) && (SupportedDevices[i].ChipID == ID[1])) break;
	}
	if (i == SUPPORTED_DEV_NUMBER) return NULL;
	printf("This is %s\n", SupportedDevices[i].Name);
	return &(SupportedDevices[i]);
}

int main( int argc, char **argv )
{
	unsigned long start, length, seek;
	unsigned int len, i;
	unsigned char buffer[MAX_BLOCK_LEN], ids[2];
	unsigned char buffer2[MAX_BLOCK_LEN];
	//unsigned char cmdW, cmdR;
	char flash, erase, readF, verify, id, /*compatible,*/ silent, defaults = 0;
	char *fileName;

	//These are mode switches.
	//Multiple modes can be selected simultaneously, they are executed in a consistent order (argument order does not matter).
	id = 0; //Read manufacturer + chip ID from the register space (TESTED)
	erase = 0; //Erase chip (NOT IMPLEMENTED).
	flash = 0; //Write to the flash memory space (NOT IMPLEMENTED)
	//compatible = 0; //Flash/erase the chip using only standard single-byte writes.
	readF = 0; //Read the flash memory (TESTED)
	verify = 0; //Verify the flash memory contents against the specified file (NOT TESTED).
	//These are programmer settings
	start = 0x0; //Start address (in the memory map of the device) for reading and writing
	length = 0x80000; //R/W length
	len = 0x1; //R/W block size. Changed default to 1, because 49lf004b and similar ones don't support multiple-byte R/W operations.
	fileName = 0; //For reading into or writing from (or "reading from" for verification mode).
	//cmdR = 0xff; //Software Command for reading (NOT IMPLEMENTED), defaults to the one suitable for SST49LF016C.
	//cmdW = 0x10; //Software Command for writing (NOT IMPLEMENTED), defaults to the one suitable for SST49LF016C.
	seek = 0; //Start address (in the file) for R/W
	silent = 0; //Don't ask for confirmation of default values

	//Parsing arguments.
	i = 1; //Index for parsing of command line arguments
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
		/*else if ((strcmp(argv[i], "-c") == 0)) {
			compatible = 1;
		}*/
		else if ((strcmp(argv[i], "-m") == 0)) {
			silent = 1;
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
			defaults++;
		}
		else if((strcmp(argv[i], "-o") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%lx", &seek);
			printf("Seek in input file 0x%lx\n", seek);
		}
		else if((strcmp(argv[i], "-l") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%lx", &length);
			defaults++;
		}
		else if((strcmp(argv[i], "-b") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%x", &len);
			defaults++;
		}
		/*else if((strcmp(argv[i], "-cW") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%hhx", &cmdW);
		}
		else if((strcmp(argv[i], "-cR") == 0) && (i+1 < argc)) {
			sscanf(argv[++i], "%hhx", &cmdR);
		}*/
		else if(strcmp(argv[i], "-d") == 0)
		{
			dbg = true;
		}
		else {
			printf("SST49LF016C flash programmer *modified to read SST49LF004B*\n");
			printf("Usage: %s parameters\n", argv[0]);
			printf("Parameters:\n");
			//printf(" -c                Compatibility mode: single-byte operation only.\n");
			printf(" -w                Write to flash (flash is not erased)\n");
			printf(" -e                Erase flash \n");
			printf(" -r                Read the flash\n");
			printf(" -v                Verify the flash\n");
			printf(" -f  filename      Specifies file for writing, reading, verifying\n");
			printf(" -s  hex (32-bit)  Sets start address (hex, default = 0x0)\n");
			printf(" -o  hex (32-bit)  Offset in file - Seeks in input file before operation\t\n");
			printf(" -l  hex (32-bit)  R/W Length (default = 0x80000)\t\n");
			printf(" -b  hex (8-bit)   Block size (check the datasheet for your IC, default if 0x1)\n");
			//printf(" -cW hex (8-bit)   Chip Command for writing (default 0x40)\n");
			//printf(" -cR hex (8-bit)   Chip Command for reading (default 0xff)\n");
			printf(" -d                Debug mode (verbose output + each step requires confirmation)\n");
			printf(" -m                Silent (don't ask for any confirmations, except debug mode)\n");
			exit(0);
		}
		i++;
	}

	//Check if a file had to be specified
	if(flash || verify) {
		if(fileName) fileHandle = open(fileName, O_RDONLY);
	} else {
		if(fileName) fileHandle = open(fileName, O_WRONLY | O_CREAT);
	}
	if(flash && (fileHandle == -1)) {
		printf("Cannot program flash without file (use -f)\n");
		exit(2);
	}
	//Confirm the values that are not required
	if (len > MAX_BLOCK_LEN)
	{
		printf("Maximum block size is %d bytes!\n", MAX_BLOCK_LEN);
		safeExit(2);
	}
	if (silent)
	{
		printf("Starting address 0x%lx\n", start);
		printf("Length 0x%lx\n", length);
		printf("Block size 0x%x\n", len);
		//printf("Command for Writing 0x%x\n", cmdW);
		//printf("Command for Reading 0x%x\n", cmdR);
	}
	else
	{
		if (erase && (defaults == 0))
		{
			printf("Block size 0x%x\n", len);
			printf("Press any key to confirm default value...\n");
			getchar();
		}
		if ((readF || flash || verify) && (defaults < 3))
		{
			printf("Starting address 0x%lx\n", start);
			printf("Length 0x%lx\n", length);
			printf("Block size 0x%x\n", len);
			//printf("Command for Writing 0x%x\n", cmdW);
			//printf("Command for Reading 0x%x\n", cmdR);
			printf("Press any key to confirm possible default values...\n");
			getchar();
		}
	}

	wiringPiSetupGpio();
	preparePinMode();
	printf("Pin preparation successful.\n");

	if (id) readIDs(ids);

	if (readF)
	{
		readIDs(ids);
		const Device* dev = findDevice(ids);
		if (dev != NULL)
		{
			executeSCS(dev, false);
			if (seek > 0)
			{
				for (i = 0; i < MAX_BLOCK_LEN; i++) buffer[i] = 0;
				if (seek > MAX_BLOCK_LEN)
				{
					unsigned long s;
					for (s = 0; s < seek; s += MAX_BLOCK_LEN) write(fileHandle, buffer, MAX_BLOCK_LEN);
					write(fileHandle, buffer, seek - s + MAX_BLOCK_LEN);
				}
				else
				{
					write(fileHandle, buffer, seek);
				}
			}
			readChip(start, length, len, buffer);
		}
		else
		{
			printf("This device is not supported. Use -c if you are sure.\n");
			safeExit(1);
		}
	}

	if (erase) {
		readIDs(ids);
		const Device* dev = findDevice(ids);
		if (dev != NULL)
		{
			executeSCS(dev, true);
			compatibleEraseChip(start, length, len, buffer);
		}
		else
		{
			printf("This device is not supported. Use -c if you are sure.\n");
			safeExit(1);
		}
	}

	if (flash) {
		readIDs(ids);
		const Device* dev = findDevice(ids);
		if (dev != NULL)
		{
			executeSCS(dev, true);
			compatibleFlashChip(seek, start, length, len, buffer);
		}
		else
		{
			printf("This device is not supported. Use -c if you are sure.\n");
			safeExit(1);
		}
	}

	if (verify)
	{
		readIDs(ids);
		const Device* dev = findDevice(ids);
		if (dev != NULL)
		{
			executeSCS(dev, false);
			verifyChip(seek, start, length, len, buffer, buffer2);
		}
		else
		{
			printf("This device is not supported. Use -c if you are sure.\n");
			safeExit(1);
		}
	}

	printf("Finished.\n");
	safeExit(0);
}