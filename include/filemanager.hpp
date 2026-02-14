/*******************************************************************************
* This file is part of splasher, see the public GitHub for more information:
* https://github.com/ADBeta/splasher
*
* Splasher is a Raspberry Pi Program to flash, dump, clone and empty a large
* selection of NAND or NOR Flash chips, including the 24 & 25 Series. 
* With the ability to support many protocols, including SPI, DSPI, QSPI, I2C,
* and custom non-standard protocols certain manufacturers use.
*
* (c) ADBeta
*******************************************************************************/
#include <iostream>
#include <fstream>

#ifndef FILEMAN_H
#define FILEMAN_H

//Object containing the filename, data pointers, functions etc for binary files
class BinFile {
	public:
	//Constructor sets the filename string, creates the byteArray heap alloc
	//and opens the input file as input or output
	//mode:   'r' read file          'w' write file
	BinFile(const char *inptFN, const char mode);
	
	//Destructor flushes the byteArray to the file, deletes the byteArray and
	//closes the file
	~BinFile();
	
	/*** File Metadata Functions **********************************************/
	std::string getFilename();
	
	/*** File Reading (for dump: push bytes to file) **************************/
	void pushByteToArray(const char byte);
	int flushArrayToFile();
	
	/*** File Reading (for flash: pull bytes from file) ***********************/
	// Returns true and sets byte if a byte was read; false on EOF.
	bool pullByteFromFile(char &byte);
	
	// Whether the file was opened for reading (mode 'r')
	bool isReadMode() const { return readMode; }

	private:
	std::fstream file;
	char *filename;
	#define MAX_RAM_BYTES 10485760
	char *byteArrayPtr;
	unsigned int byteArrayPos = 0;
	bool readMode = false;
	// For read mode: bytes currently in buffer (0 when buffer exhausted)
	unsigned int byteArrayLen = 0;
}; //class BinFile


#endif
