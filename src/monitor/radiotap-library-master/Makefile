CROSS	= arm-linux-

CC	= @echo " GCC	$@"; $(CROSS)gcc
CPP	= @echo " G++	$@"; $(CROSS)g++
LD	= @echo " LD	$@"; $(CROSS)ld
AR	= @echo " AR	$@"; $(CROSS)ar
RM	= @echo " RM	$@"; rm -f



LIBDIR = ../../../lib




CFLAGS 	+= -DLINUX
CFLAGS	+= -D_REENTRANT
CFLAGS	+= -fomit-frame-pointer
CFLAGS	+= -Wstrict-prototypes
CFLAGS	+= -I$(ROOTDIR)/include
CFLAGS	+= -Wall -O2 -Os -fno-strict-aliasing
CFLAGS	+= -Wall -g
LDFLAGS	+= -lm -ldl -lpthread
LDFLAGS	+= -Wl -L$(ROOTDIR)/lib
LDFLAGS += -Wl,-rpath,./:lib:/usr/lib
AFLAGS	+= -rv

LIB_TARGET=  $(LIBDIR)/libradiotap.a
LIB_OBJS = ./parse.o ./radiotap.o

$(LIB_TARGET): $(LIB_OBJS)
	$(AR) $(AFLAGS) $@ $^
	
.c.o:
	$(CC)  $(CFLAGS) -c -o $@ $<

.cpp.o:
	$(CPP) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(LIB_TARGET) $(LIB_OBJS) $(EXEC_OBJS);
	find ./ -type f \( -name '*.elf*' -o -name '*.gdb' \) | xargs rm -f

