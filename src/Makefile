CROSS = i586-pc-msdosdjgpp
CROSS_PREFIX = i586-pc-msdosdjgpp-
CC = $(CROSS_PREFIX)gcc
AR = $(CROSS_PREFIX)ar
LD = $(CROSS_PREFIX)ld
STRIP = $(CROSS_PREFIX)strip
RANLIB = $(CROSS_PREFIX)ranlib

PATH_LUA = lua
PATH_SPNG = spng
PATH_MINIZ = miniz
PATH_LINENOISE = linenois
LIB_LUA = $(PATH_LUA)/liblua.a
LIB_SPNG = $(PATH_SPNG)/libspng.a
LIB_MINIZ = $(PATH_MINIZ)/libminiz.a
LIB_LINENOISE = $(PATH_LINENOISE)/libln.a

CFLAGS = -O2 -g
LDFLAGS = -lm -L./$(PATH_LUA) -llua -L./$(PATH_SPNG) -lspng -L./$(PATH_MINIZ) -lminiz -L./$(PATH_LINENOISE) -lln

OBJECTS = dos.o main.o image.o script.o log.o font.o text.o event.o repl.o stb/stb.o

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

$(PATH_LINENOISE)/%.o : $(PATH_LINENOISE)/%.c
	$(CC) $(CFLAGS) -c $? -o $@

linenoise: $(LIB_LINENOISE)
$(LIB_LINENOISE): $(PATH_LINENOISE)/linenois.o
	$(AR) rc $@ $?
	$(RANLIB) $@

stb: stb/stb.o
stb/stb.o: stb/stb.c
	$(CC) $(CFLAGS) -c stb/stb.c -o stb/stb.o  

%.o : %.c
	$(CC) $(CFLAGS) -g -c $? -o $@

perentie.exe : $(OBJECTS) $(LIB_LUA) $(LIB_SPNG) $(LIB_MINIZ) $(LIB_LINENOISE)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o perentie.exe

distclean: clean-lua clean-stb clean-miniz clean-spng clean-linenoise clean

clean-lua:
	$(MAKE) -C $(PATH_LUA) -f makefile clean

clean-stb:
	rm stb/*.o

clean-miniz:
	rm miniz/*.o miniz/*.a

clean-spng:
	rm spng/*.o spng/*.a

clean-linenoise:
	rm linenois/*.o linenois/*.a

clean:
	rm *.o
	rm perentie.exe


