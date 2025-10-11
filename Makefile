PROG = hex2c
OBJS = hex2c.o ihx.o stdz.o

$(PROG) : $(OBJS)
hex2c.o : ihx.h stdz.h getopt.h
ihx.o : ihx.h stdz.h
stdz.o : stdz.h getopt.h getopt.c

CFLAGS += -O2 -std=c99
CFLAGS += -Wall -Wextra -Wpedantic -Werror
LDFLAGS += -s
MAKEFLAGS += -r

$(PROG) :
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
%.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
clean :
	-rm -f $(PROG) *.o
.PHONY : clean
