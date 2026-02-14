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
* v0.0.1
* 11 Apr 2023
*******************************************************************************/
#include <iostream>

#include <pigpio.h>

#include "CLIah.hpp"
#include "filemanager.hpp"
#include "hardware.hpp"

/*** Pre-defined output messages **********************************************/
namespace message {
const char *copyright = "\nsplasher 2023 ADBeta(c)";

const char *shortHelp = "Usage: splasher [binary file] [options]\n\
use --help for full help information\n";

const char *longHelp =
	"Usage: splasher <file> [options]\n\n"
	"By default splasher dumps (reads) from the flash chip to the given file.\n"
	"Requires -b/--bytes for dump and write. Run with sudo (pigpio).\n\n"
	"Options:\n"
	"  -h, --help       Show this help\n"
	"  -b, --bytes      Bytes to read/write (required for dump/write). Suffixes: K, M (e.g. 16M)\n"
	"  -s, --speed     SPI speed in KHz (1-1000), or \"max\". Also: --speed=500\n"
	"  -o, --offset     Start address in bytes (default 0). Suffixes: K, M\n"
	"  --jedec          Read and print JEDEC ID (manufacturer, type, capacity), then exit\n"
	"  -w, --write      Flash (write) file to device; requires -b; -o = start address\n"
	"  -e, --erase      Erase: full chip, or from -o for -b bytes\n"
	"  -i, --interface  Interface: spi (default), dspi, qspi, i2c\n\n"
	"Examples:\n"
	"  splasher output.bin -b 16M\n"
	"  splasher out.bin -b 16M -s 500 -o 64K\n"
	"  splasher --jedec\n"
	"  splasher firmware.bin -b 256K -w\n"
	"  splasher /dev/null -e\n"
	"  splasher /dev/null -b 64K -o 0 -e\n";


const char *speedNotValid = "Speed (in KHz) input is invalid\n";
const char *speedTooHigh = "Speed (in KHz) is too high, Maximum is 1000KHz\n";
const char *speedDefault = "Speed not specified, using default of 100KHz\n";

const char *bytesNotValid = "Bytes argument input is invalid. valid input e.g. \
-b 100    -b 100K    -b 2M\n";
const char *bytesNotSpecified = "Bytes to read has not been specified\n";
const char *bytesTooLarge = "Bytes is too large, byte limit is 256MiB\n";
const char *offsetNotValid = "Offset argument invalid. e.g. -o 0  -o 64K  -o 1M\n";
} //namespace message

/*** Helper functions *********************************************************/
//converts a string into a KHz value - for user argument handling
//Negative values are coded errors (-1 not valid   -2 too large input)
int convertKHz(std::string speedString) {
	//First check if the input is "max"
	if(speedString.compare("max") == 0) {
		//Unlimit the KHz
		return 0;
	}
	
	//If the string contains non-numeral chars, error -1
	if(speedString.find_first_not_of("0123456789") != std::string::npos) {
		std::cerr << message::speedNotValid;
		return -1;
	}
	
	//Otherwise, convert it to an int
	int speedInt = std::stoi(speedString);
	
	//Detect if the input value is too high, if so return -2
	if(speedInt > 1000) {
		std::cerr << message::speedTooHigh;
		return -2;
	}
	
	//If no errors, return the speed int
	return speedInt;
}

unsigned long convertBytes(std::string byteString) {
	//Keep a multiplier, 1 by default for bytes, changes via 'K' or 'M'
	unsigned int multiplier = 1;
	
	/*** Detect Multiplier Char ***********************************************/
	//Find the first non-numeral character in the string
	size_t notNumeral = byteString.find_first_not_of("0123456789");
	//Get the index of the last char in the string
	size_t lastIndx = byteString.length() - 1;
	
	//If there is a non-numeral char in the string, check it
	if(notNumeral != std::string::npos) {
		//if that non-numeral char is NOT the last char, error
		if(notNumeral != lastIndx) {
			std::cerr << message::bytesNotValid;
			return 0;
		}
		
		//If the last char is either 'K' or 'M' adjust the multiplier
		char lastChar = byteString[lastIndx];
		if(lastChar == 'K') {
			multiplier = 1024; //1KiB
			
		} else if(lastChar == 'M') {
			multiplier = 1048576; //1MiB
		
		} else {
			//if the last char is NOT 'K' or 'M', Error and exit
			std::cerr << message::bytesNotValid;
			return 0;
		}
		
		//Remove the last char from the string, we are done with it
		byteString.pop_back();
	}
	
	/*** Convert and multiply the input number ********************************/
	//convert the passed string into an int and set device bytes
	unsigned long bytes = std::stoi(byteString) * multiplier;
	
	//Make sure the bytes are not too high (Limit to 256MB)
	if(bytes > 268435456) {
		std::cerr << message::bytesTooLarge;
		return 0;
	}
	
	//If everything is good, return the value
	return bytes;

}

/******************************************************************************/

/*** Main *********************************************************************/
int main(int argc, char *argv[]){
	/*** Generic pigpio stuff *************************************************/
	if(gpioInitialise() < 0) {
		std::cerr << "Error: Failed to initialise the GPIO" << std::endl;
		exit(EXIT_FAILURE);
	}

	/*** Define CLIah Arguments ***********************************************/
	//CLIah::Config::verbose = true; //Set verbosity when match is found
	CLIah::Config::stringsEnabled = true; //Set arbitrary strings allowed
	
	//Request help message
	CLIah::addNewArg(
		"Help",                 //Reference
		"--help",               //Primary match string
		CLIah::ArgType::flag,   //Argument type
		"-h"                    //Alias match string
	);

	//Speed (in KHz) of the device
	CLIah::addNewArg(
		"Speed",
		"--speed",
		CLIah::ArgType::subcommand,
		"-s"
	);

	//How many bytes to read from device
	CLIah::addNewArg(
		"Bytes",
		"--bytes",
		CLIah::ArgType::subcommand,
		"-b"
	);

	//Start address offset (bytes)
	CLIah::addNewArg(
		"Offset",
		"--offset",
		CLIah::ArgType::subcommand,
		"-o"
	);

	CLIah::addNewArg("Jedec", "--jedec", CLIah::ArgType::flag);
	CLIah::addNewArg("Write", "--write", CLIah::ArgType::flag, "-w");
	CLIah::addNewArg("Erase", "--erase", CLIah::ArgType::flag, "-e");
	CLIah::addNewArg("Interface", "--interface", CLIah::ArgType::subcommand, "-i");

	/*** User Argument handling ***************************************************/
	//Get CLIah to scan the CLI Args
	CLIah::analyseArgs(argc, argv);
	
	if( argc == 1 ) {
		std::cout << message::shortHelp << std::endl;
		gpioTerminate();
		exit(EXIT_FAILURE);
	}
	
	if( CLIah::isDetected("Help") ) {
		std::cout << message::longHelp << message::copyright << std::endl;
		gpioTerminate();
		exit(EXIT_SUCCESS);
	}
	
	/*** JEDEC-only: read and print ID then exit ******************************/
	if( CLIah::isDetected("Jedec") ) {
		Device dev;
		dev.interface = IFACE::SPI;
		dev.protocol = PROT::S25;
		dev.KHz = CLIah::isDetected("Speed") ? convertKHz(CLIah::getSubstring("Speed")) : 100;
		if (dev.KHz < 0) { gpioTerminate(); exit(EXIT_FAILURE); }
		if (splasher::readJedecId(dev)) {
			std::cout << "JEDEC ID: " << std::hex
			          << "0x" << (int)dev.jedecId.manufacturer << " "
			          << "0x" << (int)dev.jedecId.memoryType << " "
			          << "0x" << (int)dev.jedecId.capacity << std::dec << std::endl;
			gpioTerminate();
			exit(EXIT_SUCCESS);
		} else {
			std::cerr << "Failed to read JEDEC ID" << std::endl;
			gpioTerminate();
			exit(EXIT_FAILURE);
		}
	}
	
	/*** Filename handling ****************************************************/
	if( CLIah::stringVector.size() == 0 ) {
		std::cerr << "Error: No filename provided" << std::endl;
		gpioTerminate();
		exit(EXIT_FAILURE);
	}
	const char *filename = CLIah::stringVector.at(0).string.c_str();

	Device priDev;
	priDev.offset = 0;
	if (CLIah::isDetected("Interface")) {
		std::string iface = CLIah::getSubstring("Interface");
		if (iface == "spi")  { priDev.interface = IFACE::SPI;  priDev.protocol = PROT::S25; }
		else if (iface == "dspi") { priDev.interface = IFACE::DSPI; priDev.protocol = PROT::S25; }
		else if (iface == "qspi") { priDev.interface = IFACE::QSPI; priDev.protocol = PROT::S25; }
		else if (iface == "i2c")  { priDev.interface = IFACE::I2C;  priDev.protocol = PROT::S24; }
		else {
			std::cerr << "Unknown interface: " << iface << " (use spi, dspi, qspi, i2c)" << std::endl;
			gpioTerminate();
			exit(EXIT_FAILURE);
		}
	} else {
		priDev.interface = IFACE::SPI;
		priDev.protocol = PROT::S25;
	}
	
	if( CLIah::isDetected("Speed") ) {
		int KHzVal = convertKHz( CLIah::getSubstring("Speed") );
		if(KHzVal < 0) { gpioTerminate(); exit(EXIT_FAILURE); }
		priDev.KHz = KHzVal;
	} else {
		priDev.KHz = 100;
	}
	
	bool needBytes = !CLIah::isDetected("Erase") || CLIah::isDetected("Write");
	if( CLIah::isDetected("Bytes") ) {
		unsigned long byteVal = convertBytes( CLIah::getSubstring("Bytes") );
		if(byteVal == 0) { gpioTerminate(); exit(EXIT_FAILURE); }
		priDev.bytes = byteVal;
	} else if (needBytes) {
		std::cerr << message::bytesNotSpecified;
		gpioTerminate();
		exit(EXIT_FAILURE);
	}
	
	if( CLIah::isDetected("Offset") ) {
		unsigned long offsetVal = convertBytes( CLIah::getSubstring("Offset") );
		if(offsetVal == 0) {
			std::cerr << message::offsetNotValid;
			gpioTerminate();
			exit(EXIT_FAILURE);
		}
		priDev.offset = offsetVal;
	}
	
	if (CLIah::isDetected("Erase")) {
		unsigned long eraseCount = CLIah::isDetected("Bytes") ? priDev.bytes : 0;
		splasher::eraseFlash(priDev, eraseCount);
		gpioTerminate();
		return 0;
	}
	
	if (CLIah::isDetected("Write")) {
		BinFile binFile(filename, 'r');
		splasher::writeFileToFlash(priDev, binFile);
		gpioTerminate();
		return 0;
	}
	
	BinFile binFile(filename, 'w');
	splasher::dumpFlashToFile(priDev, binFile);

	gpioTerminate();
	return 0;
} 
