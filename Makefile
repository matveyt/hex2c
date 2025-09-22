CFLAGS := -O2 -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s

hex2c : hex2c.o ihx.o stdz.o
hex2c.o : hex2c.c ihx.h stdz.h
ihx.o : ihx.c ihx.h stdz.h
stdz.o : stdz.c stdz.h
clean : ;-rm -f hex2c *.o
.PHONY : clean
