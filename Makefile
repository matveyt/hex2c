CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Wpedantic -Werror
LDFLAGS += -s

hex2c : hex2c.o ihx.o stdz.o ya_getopt.o
hex2c.o : hex2c.c ihx.h stdz.h ya_getopt.h
ihx.o : ihx.c ihx.h stdz.h
stdz.o : stdz.c stdz.h
ya_getopt.o : ya_getopt.c ya_getopt.h
clean : ;-rm -f hex2c *.o
.PHONY : clean
