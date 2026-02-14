# splasher

splasher, Software Pi Flasher, is a Raspberry Pi program with the ability to 
flash, dump, clone and erase SPI/I2C/DSPI/QSPI flash chips, including the 24 and
25 series NAND and NOR Flash Chips without the need for any external devices
such as the CH341A etc. Written in C++ using pigpio, it is easy to use,
reliable and designed around compatibilty with any protocol or interface.

splasher allows interface speed selection down to 1KHz, for chips with slow
interfaces, or damaged chips, up to 1MHz limited interface for older but regular 
chips. It also allows "max" to be passed to the speed flag, in order to delimit
the interface (This varies wildly between 2MHz to 50MHz depending on model. be
aware of this before use)

## How To Use
When running splasher, you will need to use `sudo`, this is normal, and is a
side-effect of using pigpio.

To use, call splasher, pass a file to output to, and how many bytes to read (required)  
e.g `splasher output.bin -b 16M`  

The Default Pinout is as follows:  
![](https://github.com/ADBeta/splasher/blob/main/Pinout.png)

```
SCLK    2
MISO    3
MOSI    4

HOLD    17
CS      27
WP      22
```

It should be identical between Pi1, Pi2, Pi3, Pi4, PiZero etc.
**Raspberry Pi 1 REVISION 1 Boards are not supported with the standard pinout**

For full options and examples, run **`splasher --help`**. Summary of arguments:  
* -b or --bytes		How many bytes (required for dump/write). e.g. 123456, 10K, 16M
* -s or --speed		SPI speed in KHz (1–1000), or `max` for unconstrained
* -o or --offset		Start address in bytes (default 0). Supports K and M suffix
* --jedec		Read and print JEDEC ID (manufacturer, type, capacity) then exit
* -w or --write		Flash (write) file to device; requires -b; use -o for address
* -e or --erase		Erase device: full chip, or from -o for -b bytes
* -i or --interface	Interface: spi (default), dspi, qspi, i2c (dspi/qspi/i2c stubs)

## Notes
(DSPI, QSPI and I2C are stubbed; only SPI/25-series is fully implemented.)

----
## Dependencies
This program requires that you have the pigpio library installed.  
A tutorial of how to do this is [here](https://abyz.me.uk/rpi/pigpio/download.html)

## Compilation
After installing pigpio, the compilation instructions are as follows:  
```
git clone https://github.com/ADBeta/splasher.git
unzip ./splasher-master.zip
cd ./splasher-master
make
sudo make install
```

## Usage
Run with `sudo` (required by pigpio). Examples:

```bash
# Dump 16 MiB from flash to file (default offset 0, 100 KHz)
sudo splasher output.bin -b 16M

# Dump at 500 KHz, starting at offset 64 KiB
sudo splasher out.bin -b 16M -s 500 -o 64K

# Read JEDEC ID only
sudo splasher --jedec

# Flash (write) a file to device at offset 0
sudo splasher firmware.bin -b 256K -w

# Erase full chip
sudo splasher /dev/null -e

# Erase 64 KiB starting at offset 0
sudo splasher /dev/null -b 64K -o 0 -e
```

----
## Licence
This software is under the GPL 2.0 Licence, please see LICENCE for information  
<b>(c) 2023 ADBeta </b>
