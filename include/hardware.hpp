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

#include "filemanager.hpp"

#ifndef HARDWARE_H
#define HARDWARE_H

/*** Common limits ************************************************************/
namespace Limits {
	const unsigned long MAX_BYTES = 268435456u;  // 256 MiB
	const int MAX_KHZ = 1000;
	const unsigned int S25_PAGE_SIZE = 256;
}

/*** Default SPI pinout (matches README) ***************************************/
namespace Pinout {
	const int SPI_SCLK = 2;
	const int SPI_MISO = 3;
	const int SPI_MOSI = 4;
	const int SPI_HOLD = 17;
	const int SPI_CS   = 27;
	const int SPI_WP   = 22;
}

/*** Protocol command bytes (25-series SPI) ************************************/
namespace Cmd {
	namespace S25 {
		const unsigned char READ = 0x03;
		const unsigned char WRITE_ENABLE = 0x06;
		const unsigned char PAGE_PROGRAM = 0x02;
		const unsigned char SECTOR_ERASE_4K = 0x20;
		const unsigned char BLOCK_ERASE_32K = 0x52;
		const unsigned char BLOCK_ERASE_64K = 0xD8;
		const unsigned char CHIP_ERASE = 0xC7;
		const unsigned char READ_JEDEC_ID = 0x9F;
		const unsigned char READ_STATUS = 0x05;
	}
}

/*** JEDEC ID (manufacturer, memory type, capacity) ****************************/
struct ChipId {
	unsigned char manufacturer;
	unsigned char memoryType;
	unsigned char capacity;
};

//List of supported interfaces, selected via cli.
enum class IFACE { 
	SPI, DSPI, QSPI, I2C
};

//List of supported protocols, e.g. 24 Series (I2C), 25 Series (SPI), etc
enum class PROT {
	S24, S25
};


/*** Device Specific Struct ***************************************************/
//Each device has a struct with data about itself, eg the size (bytes),
//Interface, Protocol, Speed, offset
struct Device {
	IFACE interface;
	PROT protocol;
	int KHz;
	unsigned long bytes;
	unsigned long offset;
	ChipId jedecId;       // Filled by initRead / readId when available
	bool jedecValid;      // True if jedecId has been read
	Device() : interface(IFACE::SPI), protocol(PROT::S25), KHz(100), bytes(0),
	           offset(0), jedecValid(false) {}
}; //struct Device

/*** Base interface for flash hardware (for expansion) *************************/
class FlashInterface {
public:
	virtual ~FlashInterface() = default;
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual char readByte() = 0;
	virtual void writeByte(char byte) = 0;
	virtual bool readId(ChipId &id) = 0;
};

/*** Hardware I2C Interface ***************************************************/
class hwI2C : public FlashInterface {
public:
	bool readId(ChipId &id) override { (void)id; return false; }
	void start() override {}
	void stop() override {}
	char readByte() override { return 0; }
	void writeByte(char) override {}
}; //class hwI2C

/*** Hardware SPI Interface ***************************************************/
class hwSPI : public FlashInterface {
	public:
	//Constructor. Pass the pin numbers to the onject class
	hwSPI(int SCLK, int MOSI, int MISO, int CS, int WP);
	
	//Initialise the interface to basic non-selected idle state
	void init();
	
	//Set the internal delay times for key aspects of the interface
	void setTiming(unsigned int KHz);
	
	//Write Protect: enable=true drives WP high (protected), false = not protected
	void setWriteProtect(bool enable);
	
	//Transmit a byte using the SPI interface
	void tx_byte(const char byte);
	//Receive a byte using the SPI interface
	char rx_byte(void);
	
	// FlashInterface: start/stop SPI; readByte/rx_byte and writeByte/tx_byte
	void start() override;
	void stop() override;
	char readByte() override;
	void writeByte(char byte) override;
	bool readId(ChipId &id) override;
	bool readJedecId(ChipId &id);
	
	private:
	//hardware pins (Clock, M-Out, M-In, Chip Select, Write Protect)
	int io_SCLK, io_MOSI, io_MISO, io_CS, io_WP;
	
	//Key timing delay values. Default 0, full speed
	unsigned int wait_clk = 0, wait_byte = 0, wait_bit = 0;


}; //class hwSPI

/*** Hardware Dual SPI Interface (stub; same commands, dual data lines) *******/
class hwDSPI : public FlashInterface {
public:
	void start() override {}
	void stop() override {}
	char readByte() override { return 0; }
	void writeByte(char) override {}
	bool readId(ChipId &id) override { (void)id; return false; }
};

/*** Hardware Quad SPI Interface (stub; same commands, quad data lines) ******/
class hwQSPI : public FlashInterface {
public:
	void start() override {}
	void stop() override {}
	char readByte() override { return 0; }
	void writeByte(char) override {}
	bool readId(ChipId &id) override { (void)id; return false; }
};

/*** Splasher hardware namespace **********************************************/
namespace splasher {

// Init before read: GPIO/interface ready, optionally read JEDEC into dev.jedecId
void initRead(Device &dev, FlashInterface &hw);
// Init before write: e.g. disable write protect on SPI
void initWrite(Device &dev, FlashInterface &hw);

void dumpFlashToFile(Device &dev, BinFile &file);
bool readJedecId(Device &dev);

// Write file content to flash (SPI 25-series). Call initWrite first; optionally erase first.
void writeFileToFlash(Device &dev, BinFile &file);
// Erase: full chip or from offset for byteCount bytes (sector-aligned).
void eraseFlash(Device &dev, unsigned long byteCount = 0);

}; //namespace splasher




#endif
