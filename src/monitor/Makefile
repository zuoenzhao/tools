CROSS	= arm-linux-

CC	= @echo " GCC	$@"; $(CROSS)gcc
CPP	= @echo " G++	$@"; $(CROSS)g++
LD	= @echo " LD	$@"; $(CROSS)ld
AR	= @echo " AR	$@"; $(CROSS)ar
RM	= @echo " RM	$@"; rm -f


CFLAGS	+= -Wall -g
CFLAGS	+= -fomit-frame-pointer
CFLAGS	+= -Wstrict-prototypes

TOPDIR := $(shell pwd)
export TOPDIR


CFLAGS += -I$(TOPDIR)/include
LDFLAGS += ../../lib/libradiotap.a


AFLAGS	+= -rv



TARGET	= ./$(Board)
OBJS	= ./$(Board).o  

all: $(TARGET)
	@echo ""
	@echo "make success ----------------------------------"
	@echo ""
	cp $(TARGET) /home/share/zhaozuoen/ -rf


$(TARGET): $(OBJS)
	@echo "LDFLAGS:$(LDFLAGS)" 
	$(CC) -o $@ $^ -pthread $(LDFLAGS) 

.c.o:
	$(CC)  $(CFLAGS) -c -o  $@ $< 

.cpp.o:
	$(CPP) $(CFLAGS) -c -o $@ $<
clean:
	rm -f *.o;


