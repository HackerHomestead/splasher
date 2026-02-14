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
#include "hardware.hpp"

#include <iostream>
#include <string>
#include <pigpio.h>

/*** Hardware SPI Interface ***************************************************/
hwSPI::hwSPI(int SCLK, int MOSI, int MISO, int CS, int WP) {
	//Set the object pins to the passed pins
	io_SCLK = SCLK;
	io_MOSI = MOSI;
	io_MISO = MISO;
	io_CS = CS;
	io_WP = WP;
	
	//Set the GPIO pinout to idle the interface
	init();
	
}

void hwSPI::init() {
	//Set the output pins
	gpioSetMode(io_SCLK, PI_OUTPUT);
	gpioSetMode(io_MOSI, PI_OUTPUT);
	gpioSetMode(io_CS, PI_OUTPUT);
	gpioSetMode(io_WP, PI_OUTPUT);
	
	//MISO is an input (Master In)
	gpioSetMode(io_MISO, PI_INPUT);
	
	//Set MOSI and SCLK low to idle
	gpioWrite(io_SCLK, 0);
	gpioWrite(io_MOSI, 0);
	//MISO LOW to pulldown
	gpioWrite(io_MISO, 0);
	
	stop(); //Pulls the CS pin high and waits
	
	//WP default: asserted (write protected) until explicitly disabled for writes
	setWriteProtect(true);
}

void hwSPI::setWriteProtect(bool enable) {
	gpioWrite(io_WP, enable ? 1 : 0);
}

void hwSPI::setTiming(unsigned int KHz) {
	// pigpio minimum delay is 1 us; 0 = no delay (max speed). Half-period in us = 500/KHz.
	if (KHz == 0) {
		wait_clk = 0;
		wait_bit = 0;
		wait_byte = 0;
	} else {
		unsigned int halfUs = 500 / KHz;
		if (halfUs < 1) halfUs = 1;
		wait_clk = halfUs;
		wait_bit = halfUs;
		wait_byte = halfUs;
	}
}

void hwSPI::tx_byte(const char byte) {
	//TX Bits, data clocked in on the rising edge of CLK, MSBFirst
	for(signed char bitIndex = 7; bitIndex >= 0; bitIndex--) {
		//Write the current bit (input byte shifted x to the right, AND 0x01)
		gpioWrite(io_MOSI, (byte >> bitIndex) & 0x01);
		//Wait for the bit delay
		if(wait_bit != 0) gpioDelay(wait_bit);
		
		
		gpioWrite(io_SCLK, 1);                    //Set the clock pin HIGH
		if(wait_clk != 0) gpioDelay(wait_clk); //Delay if selected
		gpioWrite(io_SCLK, 0);                    //Set the clock pin LOW
		if(wait_clk != 0) gpioDelay(wait_clk); //Delay if selected
	}

	//Wait for the byte delay if selected
	if(wait_byte != 0) gpioDelay(wait_byte);	
}

char hwSPI::rx_byte(void) {
	char data = 0;
	
	//RX Bits into data, bit present on falling edge, MSBFirst
	for(unsigned char bitIndex = 0; bitIndex < 8; bitIndex++) {
		//shift the data byte 1 position to the left
		data = data << 1;
		
		bool cBit = gpioRead(io_MISO);
		
		//Set the LSB of data to read from gpio
		if(cBit != 0) data = data | 0x01;
		
		//Wait for the bit delay
		if(wait_bit != 0) gpioDelay(wait_bit);
		
		gpioWrite(io_SCLK, 1);                 //Set the clock pin HIGH
		if(wait_clk != 0) gpioDelay(wait_clk); //Delay if selected
		gpioWrite(io_SCLK, 0);                 //Set the clock pin LOW
		if(wait_clk != 0) gpioDelay(wait_clk); //Delay if selected
	}
	
	//Wait for the byte delay if selected
	if(wait_byte != 0) gpioDelay(wait_byte);	
	
	return data;
}

void hwSPI::start() {
	gpioWrite(io_CS, 0);
	if(wait_byte != 0) gpioDelay(wait_byte);
}

void hwSPI::stop() {
	gpioWrite(io_CS, 1);
	if(wait_byte != 0) gpioDelay(wait_byte);
}

char hwSPI::readByte() { return rx_byte(); }
void hwSPI::writeByte(char byte) { tx_byte(byte); }

bool hwSPI::readId(ChipId &id) { return readJedecId(id); }

bool hwSPI::readJedecId(ChipId &id) {
	start();
	tx_byte(static_cast<char>(Cmd::S25::READ_JEDEC_ID));
	id.manufacturer = static_cast<unsigned char>(rx_byte());
	id.memoryType   = static_cast<unsigned char>(rx_byte());
	id.capacity     = static_cast<unsigned char>(rx_byte());
	stop();
	return true;
}

/*** Splasher specific functions **********************************************/
namespace splasher {

void initRead(Device &dev, FlashInterface &hw) {
	hwSPI *spi = dynamic_cast<hwSPI*>(&hw);
	if (spi) {
		spi->setTiming(dev.KHz == 0 ? 0 : static_cast<unsigned int>(dev.KHz));
		dev.jedecValid = hw.readId(dev.jedecId);
	}
}

void initWrite(Device &dev, FlashInterface &hw) {
	(void)dev;
	hwSPI *spi = dynamic_cast<hwSPI*>(&hw);
	if (spi)
		spi->setWriteProtect(false);
}

bool readJedecId(Device &dev) {
	if (dev.interface != IFACE::SPI) return false;
	hwSPI dut(Pinout::SPI_SCLK, Pinout::SPI_MOSI, Pinout::SPI_MISO,
	          Pinout::SPI_CS, Pinout::SPI_WP);
	dut.setTiming(dev.KHz == 0 ? 0 : static_cast<unsigned int>(dev.KHz));
	dev.jedecValid = dut.readJedecId(dev.jedecId);
	return dev.jedecValid;
}

void dumpFlashToFile(Device &dev, BinFile &file) {
	if (dev.interface != IFACE::SPI || dev.protocol != PROT::S25) {
		std::cerr << "Dump only supported for SPI/25-series. DSPI, QSPI, I2C not yet implemented." << std::endl;
		return;
	}
	
	std::cout << "\nReading " << dev.bytes << " bytes from offset " << dev.offset
	          << ", at " << (dev.KHz ? std::to_string(dev.KHz) : "max")
	          << " KHz to " << file.getFilename() << "\n\n" << std::flush;
	
	hwSPI dut(Pinout::SPI_SCLK, Pinout::SPI_MOSI, Pinout::SPI_MISO,
	          Pinout::SPI_CS, Pinout::SPI_WP);
	dut.setTiming(dev.KHz == 0 ? 0 : static_cast<unsigned int>(dev.KHz));
	
	initRead(dev, dut);
	
	dut.start();
	dut.tx_byte(Cmd::S25::READ);
	dut.tx_byte((dev.offset >> 16) & 0xFF);
	dut.tx_byte((dev.offset >> 8) & 0xFF);
	dut.tx_byte(dev.offset & 0xFF);
	
	unsigned long KiBDone = 0;
	unsigned long maxByte = dev.bytes + 1;
	for(unsigned long cByte = 1; cByte < maxByte; cByte++) {
		file.pushByteToArray(dut.readByte());
		if(cByte % 1024 == 0) {
			++KiBDone;
			std::cout << "\rDumped " << KiBDone << "KiB" << std::flush;
		}
	}
	
	std::cout << "\n\nFinished dumping to " << file.getFilename() << std::endl;
	dut.stop();
}

static void s25_waitBusy(hwSPI &dut) {
	while (true) {
		dut.start();
		dut.tx_byte(Cmd::S25::READ_STATUS);
		unsigned char st = static_cast<unsigned char>(dut.rx_byte());
		dut.stop();
		if ((st & 1) == 0) break;  // WIP bit clear
	}
}

void writeFileToFlash(Device &dev, BinFile &file) {
	if (dev.interface != IFACE::SPI || dev.protocol != PROT::S25) {
		std::cerr << "Write only supported for SPI/25-series. DSPI, QSPI, I2C not yet implemented." << std::endl;
		return;
	}
	if (!file.isReadMode()) {
		std::cerr << "Write requires a file opened for reading." << std::endl;
		return;
	}
	std::cout << "\nWriting " << dev.bytes << " bytes from " << file.getFilename()
	          << " to flash at offset " << dev.offset << "\n\n" << std::flush;
	hwSPI dut(Pinout::SPI_SCLK, Pinout::SPI_MOSI, Pinout::SPI_MISO,
	          Pinout::SPI_CS, Pinout::SPI_WP);
	dut.setTiming(dev.KHz == 0 ? 0 : static_cast<unsigned int>(dev.KHz));
	initWrite(dev, dut);
	unsigned long addr = dev.offset;
	unsigned long remaining = dev.bytes;
	unsigned long KiBDone = 0;
	while (remaining > 0) {
		unsigned int chunk = static_cast<unsigned int>(remaining > Limits::S25_PAGE_SIZE ? Limits::S25_PAGE_SIZE : remaining);
		dut.start();
		dut.tx_byte(Cmd::S25::WRITE_ENABLE);
		dut.stop();
		dut.start();
		dut.tx_byte(Cmd::S25::PAGE_PROGRAM);
		dut.tx_byte((addr >> 16) & 0xFF);
		dut.tx_byte((addr >> 8) & 0xFF);
		dut.tx_byte(addr & 0xFF);
		for (unsigned int i = 0; i < chunk; i++) {
			char b;
			if (!file.pullByteFromFile(b)) break;
			dut.tx_byte(b);
		}
		dut.stop();
		s25_waitBusy(dut);
		addr += chunk;
		remaining -= chunk;
		if ((dev.bytes - remaining) / 1024 > KiBDone) {
			KiBDone = (dev.bytes - remaining) / 1024;
			std::cout << "\rWritten " << KiBDone << " KiB" << std::flush;
		}
	}
	std::cout << "\n\nFinished writing to flash." << std::endl;
}

void eraseFlash(Device &dev, unsigned long byteCount) {
	if (dev.interface != IFACE::SPI || dev.protocol != PROT::S25) {
		std::cerr << "Erase only supported for SPI/25-series. DSPI, QSPI, I2C not yet implemented." << std::endl;
		return;
	}
	hwSPI dut(Pinout::SPI_SCLK, Pinout::SPI_MOSI, Pinout::SPI_MISO,
	          Pinout::SPI_CS, Pinout::SPI_WP);
	dut.setTiming(dev.KHz == 0 ? 0 : static_cast<unsigned int>(dev.KHz));
	initWrite(dev, dut);
	dut.start();
	dut.tx_byte(Cmd::S25::WRITE_ENABLE);
	dut.stop();
	dut.start();
	if (byteCount == 0) {
		dut.tx_byte(Cmd::S25::CHIP_ERASE);
		std::cout << "Chip erase started (full device)." << std::endl;
	} else {
		// Sector erase 4KB at a time
		unsigned long addr = dev.offset;
		unsigned long end = dev.offset + byteCount;
		while (addr < end) {
			dut.start();
			dut.tx_byte(Cmd::S25::WRITE_ENABLE);
			dut.stop();
			dut.start();
			dut.tx_byte(Cmd::S25::SECTOR_ERASE_4K);
			dut.tx_byte((addr >> 16) & 0xFF);
			dut.tx_byte((addr >> 8) & 0xFF);
			dut.tx_byte(addr & 0xFF);
			dut.stop();
			s25_waitBusy(dut);
			addr += 4096;
		}
		std::cout << "Erased " << byteCount << " bytes from offset " << dev.offset << std::endl;
	}
	dut.stop();
}

}; //namespace splasher
