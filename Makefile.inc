ARDUINO_BOARD = mega2560

all:
	scons -f ../SConstruct ARDUINO_BOARD=$(ARDUINO_BOARD) \
		EXTRA_LIB=../lib/ -Q

clean:
	scons -f ../SConstruct ARDUINO_BOARD=$(ARDUINO_BOARD) \
		EXTRA_LIB=../lib/ -Q -c
