CROSS = i586-pc-msdosdjgpp
CROSS_PREFIX = i586-pc-msdosdjgpp-
CC = $(CROSS_PREFIX)gcc
AR = $(CROSS_PREFIX)ar
LD = $(CROSS_PREFIX)ld
STRIP = $(CROSS_PREFIX)strip
RANLIB = $(CROSS_PREFIX)ranlib

PATH_LUA = lua
LIB_LUA = $(PATH_LUA)/liblua.a

CFLAGS = -O2
LDFLAGS = -lm -L./$(PATH_LUA) -llua

OBJECTS = dos.o main.o stb/stb.o

all: perentie.exe

lua: $(LIB_LUA)
$(LIB_LUA):
	$(MAKE) -C $(PATH_LUA) -f makefile liblua.a CFLAGS=$(CFLAGS) CC=$(CC) AR="$(AR) rc" LD=$(LD) STRIP=$(STRIP) RANLIB=$(RANLIB)

stb: stb/stb.o
stb/stb.o: stb/stb.c
	$(CC) $(CFLAGS) -c stb/stb.c -o stb/stb.o  

%.o : %.c
	$(CC) $(CFLAGS) -c $<

perentie.exe : $(OBJECTS) $(LIB_LUA)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o perentie.exe

distclean: clean-lua clean

clean-lua:
	$(MAKE) -C $(PATH_LUA) -f makefile clean

clean-stb:
	rm stb/*.o

clean:
	rm *.o
	rm perentie.exe


