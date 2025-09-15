CFLAGS := -O2 -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s

.PHONY : clean

hex2c : ihx.o stdz.o
hex2c.o : ihx.h stdz.h
ihx.o : ihx.h stdz.h
stdz.o : stdz.h
clean : ;-rm -f hex2c *.o
