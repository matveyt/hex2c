CFLAGS := -O2 -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS := -s

.PHONY : clean

hex2c : ihex.o stdz.o
hex2c.o : ihex.h stdz.h
ihex.o : ihex.h stdz.h
stdz.o : stdz.h
clean : ;-rm -f hex2c *.o
