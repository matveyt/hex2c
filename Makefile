TARGET = hex2c
OBJECTS = hex2c.o stdz.o ihx.o

CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Wpedantic -Werror
LDFLAGS += -s
MAKEFLAGS += -r

$(TARGET) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
clean :
	-rm -f $(TARGET) $(OBJECTS)
.PHONY : clean

hex2c.o : stdz.h getopt.h ihx.h
stdz.o : stdz.h getopt.h getopt.c
ihx.o : stdz.h ihx.h
