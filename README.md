Arduino DALI library for Cosino MEGA 2560
=========================================

Multi bus DALI master based on a [Cosino Mega 2560](http://www.cosino.io/product/cosino-mega-2560-developer-kit) board.

This device con control 2 DALI busses.

<img src="https://github.com/cosino/dali2560/blob/master/cosino_dali_logo.png" width="100">

Prerequisites
-------------

To compile the code you need some packages on your [Cosino Mega 2560](http://www.cosino.io/product/cosino-mega-2560-developer-kit) board
and you can use the following command line to install them:

    # aptitude install arduino scons make git-core

Then you need the file [`mega2560.sh`](https://github.com/cosino/cosino.github.io/blob/master/data/extensions/mega_2560/mega2560.sh) to upload your HEX file into the Mega 2560 CPU.
It can be downloaded from the URL https://github.com/cosino/cosino.github.io/blob/master/data/extensions/mega_2560/mega2560.sh.

Doing a simple program
----------------------

An example program can be found in [Dali_sketch/Dali_sketch.pde](Dali_sketch/Dali_sketch.pde). The code is reported below:

    #include <Dali.h>
    
    /*
     * Objects
     */
     
    Dali dali0, dali1;
    
    /*
     * Setup
     */
     
    void setup()
    {
    	delay(500);
    	Serial.begin(115200);
    	
    	delay(500);
    	Serial.println("DALI testing program - ver. 1.00");
    	Serial.println("Cosino Mega 2560 extension");
    	Serial.println("Luca Zulberti <luca.zulberti@cosino.io>");
    	
    	delay(500);
    	dali0.begin(18, 10);	/* Overwrite OC1A and OC1B */
    	dali1.begin(A14, A15);	/* Overwrite ADCch 14 and 15 */
    }
    
    /*
     * Loop
     */
    
    void loop()
    {
    	serialDali();
    }


As a normal Arduino projects, you can compile and load the sketch into the Mega 2560 CPU with few commands.

From the Cosino Mega 2560 console enter into the main folder and then use the commands below:

    # cd Dali_sketch
    # make
    scons -f ../SConstruct ARDUINO_BOARD=mega2560 \
                EXTRA_LIB=../lib/ -Q
    maximum size for hex file: 258048 bytes
    scons: building associated VariantDir targets: build
    [ ... ]
    avr-size --target=ihex Dali_sketch.hex
    text    data     bss     dec     hex filename
       0   13028       0   13028    32e4 Dali_sketch.hex

Then use the [`mega2560.sh`](https://github.com/cosino/cosino.github.io/blob/master/data/extensions/mega_2560/mega2560.sh) to load the HEX file as follow (I supposed you installed the program under the `/opt` directory):

    # /opt/mega2560.sh prog Dali_sketch.hex
    avrdude: AVR device initialized and ready to accept instructions

    Reading | ################################################## | 100% 0.01s

    avrdude: Device signature = 0x1e9801
    avrdude: NOTE: FLASH memory has been specified, an erase cycle will be performed
             To disable this feature, specify the -D option.
    avrdude: erasing chip
    avrdude: reading input file "Dali_sketch.hex"
    avrdude: input file Dali_sketch.hex auto detected as Intel Hex
    avrdude: writing flash (13028 bytes):

    Writing | ################################################## | 100% 1.73s

    avrdude: 13028 bytes of flash written

    avrdude: safemode: Fuses OK

    avrdude done.  Thank you.

Test the simple program
------------------------

To test the program you can use the screen program (use the usual `aptitude` command to install it).

   # screen /dev/ttyS3 115200

In the screen type:

   d1000*(CTRL+j)*

This command will turn ON the light at the address 0x00 on bus 0 (see the documentation file [protocol.txt](protocol.txt)).

To turn OFF the device, type:

   d0000*(CTRL+j)*


Documentation files
-------------------

* [dali.txt](dali.txt): hardware and dali commands specs.

* [protocol.txt](protocol.txt): the serial protocol to use the library.
