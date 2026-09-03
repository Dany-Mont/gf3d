##############################################################################
#
# The Linux-GCC Makefile
#
##############################################################################


#
# Compiler stuff -- adjust to your system.
#

# Linux
PROJECT = gf3d
CC      = clang
#CC      = clang

LIB_PATH = ../libs
LIB_LIST = ../gfc/libs/libgfc.a ../gfc/simple_json/libs/libsj.a ../gfc/simple_logger/libs/libsl.a
VULKAN_SDK = $(HOME)/VulkanSDK/1.4.357.1
VULKAN_CFLAGS = -I$(VULKAN_SDK)/macOS/include
VULKAN_LDFLAGS = -L$(VULKAN_SDK)/macOS/lib -Wl,-rpath,$(VULKAN_SDK)/macOS/lib -lvulkan
OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))

INC_PATHS = ../include ../gfc/include ../gfc/simple_logger/include ../gfc/simple_json/include
INC_PARAMS =$(foreach d, $(INC_PATHS), -I$d)

SDL_CFLAGS = `sdl2-config --cflags` $(INC_PARAMS) $(VULKAN_CFLAGS)
SDL_LDFLAGS = `sdl2-config --libs` -L/opt/homebrew/lib -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lpng -ljpeg -lz $(VULKAN_LDFLAGS) -lm
LFLAGS = -g  -o ../$(PROJECT) 
CFLAGS = -g -fPIC -Wall -pedantic -std=gnu99 -fgnu89-inline -Wno-unknown-pragmas -Wno-variadic-macros -Wformat-truncation=0 -Wno-strict-prototypes
# -ffast-math for relase version

DOXYGEN = doxygen

#
# Targets
#

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) $(LDFLAGS) $(LIB_LIST) $(SDL_LDFLAGS) 

docs:
	$(DOXYGEN) doxygen.cfg

sources:
	echo (patsubst %.c,%.o,$(wildcard *.c)) > makefile.sources


makefile.dep: depend

depend:
	@touch makefile.dep
	@-rm makefile.dep
	@echo Creating dependencies.
	@for i in *.c; do $(CC) -MM $(SDL_CFLAGS) $$i; done > makefile.dep
	@echo Done.

clean:
	rm *.o

count:
	wc -l *.c $(foreach d, $(INC_PATHS), $d/*.h) makefile

#
# Dependencies.
#

include makefile.dep

#
# The default rule.
#

.c.o:
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $<
