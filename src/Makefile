CROSS = i586-pc-msdosdjgpp
CROSS_PREFIX = i586-pc-msdosdjgpp-
CC = $(CROSS_PREFIX)gcc
AR = $(CROSS_PREFIX)ar
LD = $(CROSS_PREFIX)ld
STRIP = $(CROSS_PREFIX)strip
RANLIB = $(CROSS_PREFIX)ranlib
EXE2COFF = exe2coff
LUAC = luac
LUA = lua
LDOC = ldoc

PATH_LUA = lua
PATH_SPNG = spng
PATH_MINIZ = miniz
LIB_LUA = $(PATH_LUA)/liblua.a
LIB_SPNG = $(PATH_SPNG)/libspng.a
LIB_MINIZ = $(PATH_MINIZ)/libminiz.a

CFLAGS = -O2 -g -DSYSTEM_DOS
LDFLAGS = -lm -L./$(PATH_LUA) -llua -L./$(PATH_SPNG) -lspng -L./$(PATH_MINIZ) -lminiz 

OBJECTS = system.o colour.o dos.o dos_vga.o main.o image.o script.o log.o font.o text.o event.o repl.o musicrad.o stb/stb.o

all: perentie.exe

lua: $(LIB_LUA)
$(LIB_LUA):
	$(MAKE) -C $(PATH_LUA) -f makefile liblua.a CFLAGS="$(CFLAGS)" CC="$(CC)" AR="$(AR) rc" LD="$(LD)" STRIP="$(STRIP)" RANLIB="$(RANLIB)"

$(PATH_MINIZ)/%.o : $(PATH_MINIZ)/%.c
	$(CC) $(CFLAGS) -c $? -o $@

miniz: $(LIB_MINIZ)
$(LIB_MINIZ): $(PATH_MINIZ)/miniz.o $(PATH_MINIZ)/miniz_tdef.o $(PATH_MINIZ)/miniz_tinfl.o $(PATH_MINIZ)/miniz_zip.o
	$(AR) rc $@ $?
	$(RANLIB) $@ 
	
$(PATH_SPNG)/%.o : $(PATH_SPNG)/%.c
	$(CC) $(CFLAGS) -I$(PATH_MINIZ) -DSPNG_STATIC -DSPNG_DISABLE_OPT=1 -DSPNG_USE_MINIZ -c $? -o $@ 

spng: $(LIB_SPNG)
$(LIB_SPNG): $(PATH_SPNG)/spng.o
	$(AR) rc $@ $?
	$(RANLIB) $@ 

stb: stb/stb.o
stb/stb.o: stb/stb.c
	$(CC) $(CFLAGS) -c stb/stb.c -o stb/stb.o  

boot.lo: boot.lua
	$(LUAC) -o boot.lo boot.lua

boot.h: boot.lo
	$(LUA) bin2c.lua boot.lo > boot.h

cbor.lo: cbor.lua
	$(LUAC) -o cbor.lo cbor.lua

cbor.h: cbor.lo
	$(LUA) bin2c.lua cbor.lo > cbor.h

inspect.lo: inspect.lua
	$(LUAC) -o inspect.lo inspect.lua

inspect.h: inspect.lo
	$(LUA) bin2c.lua inspect.lo > inspect.h

script.o: script.c boot.h cbor.h inspect.h
	$(CC) $(CFLAGS) -g -c script.c -o script.o

%.o : %.c
	$(CC) $(CFLAGS) -g -c $? -o $@

pt_base.exe : $(OBJECTS) $(LIB_LUA) $(LIB_SPNG) $(LIB_MINIZ) 
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o pt_base.exe

perentie.exe : pt_base.exe
	$(EXE2COFF) pt_base.exe
	cat CWSDSTUB.EXE pt_base > perentie.exe

distclean: clean-lua clean-stb clean-miniz clean-spng clean

clean-lua:
	$(MAKE) -C $(PATH_LUA) -f makefile clean

clean-stb:
	rm stb/*.o

clean-miniz:
	rm miniz/*.o miniz/*.a

clean-spng:
	rm spng/*.o spng/*.a

clean:
	rm *.o *.lo
	rm pt_base pt_base.exe perentie.exe

doc:
	$(LDOC) -p Perentie -d ../doc boot.lua

