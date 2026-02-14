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
#include <string>
#include <cstring>

#include "filemanager.hpp"

//Constructor: Input Filename. Opens the file ready for read or write
BinFile::BinFile(const char *inptFN, const char mode) {
	//Create a char array at filename the size of the input string.
	this->filename = new char[ strlen(inptFN) + 1 ];
	//Copy the input string to the new char array at filename
	strcpy(this->filename, inptFN);
	
	if(mode == 'r') {
		readMode = true;
		file.open(filename, std::ios::in | std::ios::binary);
	} else if(mode == 'w') {
		readMode = false;
		file.open(filename, std::ios::out | std::ios::trunc | std::ios::binary);
	} else {
	//If the mode is unrecognised, error
		std::cerr << "Error: BinFile mode is not recognised\n";
		exit(EXIT_FAILURE);
	}
	
	//Make sure file is open and exists
	if(file.is_open() == 0) {
		std::cerr << "Error: Cannot open or create file: " << filename << "\n";
		exit(EXIT_FAILURE);
	}
	
	//Create the RAM Byte array, of size defined in header
	byteArrayPtr = new char[MAX_RAM_BYTES];
}

BinFile::~BinFile() {
	if (!readMode && byteArrayPos != 0)
		flushArrayToFile();
	file.close();
	delete[] filename;
	delete[] byteArrayPtr;
}

std::string BinFile::getFilename() {
	return (std::string)this->filename;
}

void BinFile::pushByteToArray(const char byte) {
	//Check if the current byte pos equals the MAX_RAM_BYTES
	if(byteArrayPos == MAX_RAM_BYTES) {
		//Flush the array to the file
		flushArrayToFile();
	}
	
	//Set the byte in the array at current pos to the passed byte value
	byteArrayPtr[byteArrayPos] = byte;
	
	//Incriment the byte pos
	++byteArrayPos;
}

int BinFile::flushArrayToFile() {
	if (byteArrayPos == 0) return 0;
	file.write(byteArrayPtr, byteArrayPos);
	byteArrayPos = 0;
	return 0;
}

bool BinFile::pullByteFromFile(char &byte) {
	if (!readMode) return false;
	if (byteArrayPos >= byteArrayLen) {
		file.read(byteArrayPtr, MAX_RAM_BYTES);
		byteArrayLen = static_cast<unsigned int>(file.gcount());
		byteArrayPos = 0;
		if (byteArrayLen == 0) return false;
	}
	byte = byteArrayPtr[byteArrayPos++];
	return true;
}
