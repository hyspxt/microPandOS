# Cross toolchain variables
# If these are not in your path, you can make them absolute.
XT_PRG_PREFIX = mipsel-linux-gnu-
CC = $(XT_PRG_PREFIX)gcc
LD = $(XT_PRG_PREFIX)ld

# uMPS3-related paths

# Simplistic search for the umps3 installation prefix.
# If you have umps3 installed on some weird location, set UMPS3_DIR_PREFIX by hand.
ifneq ($(wildcard /usr/bin/umps3),)
	UMPS3_DIR_PREFIX = /usr
else
	UMPS3_DIR_PREFIX = /usr/local
endif

UMPS3_DATA_DIR = $(UMPS3_DIR_PREFIX)/share/umps3
UMPS3_INCLUDE_DIR = $(UMPS3_DIR_PREFIX)/include/umps3

# Compiler options
CFLAGS_LANG = -ffreestanding # -ansi
CFLAGS_MIPS = -mips1 -mabi=32 -mno-gpopt -G 0 -mno-abicalls -fno-pic -mfp32
CFLAGS = $(CFLAGS_LANG) $(CFLAGS_MIPS) -I$(UMPS3_INCLUDE_DIR) -Wall -O0

# Linker options
LDFLAGS = -G 0 -nostdlib -T $(UMPS3_DATA_DIR)/umpscore.ldscript

# Add the location of crt*.S to the search path
VPATH = $(UMPS3_DATA_DIR)
BINDIR  := ./bin
SRCDIR1 := ./phase1
SRCDIR2 := ./phase2
VMDIR  := ./vm
FILES := $(filter-out p1test, $(basename $(notdir $(wildcard $(SRCDIR1)/*.c))) $(basename $(notdir $(wildcard $(SRCDIR2)/*.c)))) libumps crtso
OBJECTS = $(addprefix $(BINDIR)/, $(addsuffix .o,$(FILES)))
HEADERS = $(wildcard ./h/*.h) $(wildcard ./h/umps/*.h)

$(shell mkdir -p $(BINDIR))
$(shell mkdir -p $(VMDIR))

.PHONY : all clean

all : $(BINDIR)/kernel.core.umps

$(BINDIR)/kernel.core.umps : $(BINDIR)/kernel
	umps3-elf2umps -k $<

$(BINDIR)/kernel : $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

clean :
	-rm -f *.o ./phase1/*.o kernel kernel.*.umps ./phase2/*.o
	-rm -rf ./bin
	-rm -rf ./vm

# Pattern rule for assembly modules
$(BINDIR)/%.o: $(SRCDIR1)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
$(BINDIR)/%.o: $(SRCDIR2)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
$(BINDIR)/%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@
