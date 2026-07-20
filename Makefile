TARGET = hex2c
OBJECTS = hex2c.o ihx.o stdz.o

CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Werror -Wpedantic
LDFLAGS += -s
MAKEFLAGS += -r

$(TARGET) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
clean :
	-rm -f $(TARGET) $(OBJECTS)
.PHONY : clean

# !!gcc -MM *.c
hex2c.o: hex2c.c stdz.h getopt.h ihx.h
ihx.o: ihx.c ihx.h stdz.h getopt.h
stdz.o: stdz.c stdz.h getopt.h getopt.c
