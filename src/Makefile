STDLIBHOME = ../libs/circle-stdlib
CIRCLEHOME = ../libs/circle-stdlib/libs/circle
NEWLIBDIR  = ../libs/circle-stdlib/install/arm-none-circle

# Toolchain definitions (ensures cross-compiler is used for C++)
PREFIX ?= arm-none-eabi-
CC     := $(PREFIX)gcc
CXX    := $(PREFIX)g++
LD     := $(PREFIX)g++
AR     := $(PREFIX)ar

# Source directories
Q3_SRC = ../libs/q3lite/code

# Collect sub-library sources
Q3LITE_RENDERER_SRCS = $(filter-out $(Q3_SRC)/renderergles1/tr_subs.c,$(wildcard $(Q3_SRC)/renderercommon/*.c) $(wildcard $(Q3_SRC)/renderergles1/*.c))
Q3LITE_JPEG_SRCS     = $(wildcard $(Q3_SRC)/jpeg-8c/*.c)
Q3LITE_ZLIB_SRCS     = $(wildcard $(Q3_SRC)/zlib/*.c)
Q3LITE_BOTLIB_SRCS   = $(wildcard $(Q3_SRC)/botlib/*.c)

# System and Platform port files
Q3LITE_CIRCLE_SRCS   = $(Q3_SRC)/circle/circle_glimp.c \
                       $(Q3_SRC)/circle/circle_input.c \
                       $(Q3_SRC)/circle/circle_net_ip.c \
                       $(Q3_SRC)/circle/vm_arm7l_circle.c \
                       $(Q3_SRC)/circle/circle_gamma.c 
                       #$(Q3_SRC)/circle/vm_arm7l_circle.c

Q3LITE_SYS_CPPS      = $(Q3_SRC)/circle/sys_circle.cpp \
                       $(Q3_SRC)/circle/con_circle.cpp \
                       $(Q3_SRC)/circle/circle_snd.cpp \
                       $(Q3_SRC)/circle/sys_main_circle.cpp 

# Re-enabled sv_bot.c
Q3LITE_SERVER_SRCS   = $(filter-out $(Q3_SRC)/server/sv_rankings.c, $(wildcard $(Q3_SRC)/server/*.c))

# Core engine sources
Q3LITE_CORE_SRCS     = $(Q3_SRC)/qcommon/q_shared.c \
                       $(Q3_SRC)/qcommon/common.c \
                       $(Q3_SRC)/qcommon/q_math.c \
                       $(Q3_SRC)/qcommon/puff.c \
                       $(Q3_SRC)/qcommon/huffman.c \
                       $(Q3_SRC)/qcommon/huffman_static.c \
                       $(Q3_SRC)/qcommon/md4.c \
                       $(Q3_SRC)/qcommon/cvar.c \
                       $(Q3_SRC)/qcommon/cmd.c \
                       $(Q3_SRC)/qcommon/files.c \
                       $(Q3_SRC)/qcommon/md5.c \
                       $(Q3_SRC)/qcommon/msg.c \
                       $(Q3_SRC)/qcommon/unzip.c \
                       $(Q3_SRC)/qcommon/ioapi.c \
                       $(Q3_SRC)/qcommon/cm_load.c \
                       $(Q3_SRC)/qcommon/cm_patch.c \
                       $(Q3_SRC)/qcommon/cm_polylib.c \
                       $(Q3_SRC)/qcommon/cm_test.c \
                       $(Q3_SRC)/qcommon/cm_trace.c \
                       $(Q3_SRC)/qcommon/vm.c \
                       $(Q3_SRC)/qcommon/vm_interpreted.c \
                       $(Q3_SRC)/qcommon/net_chan.c \
                       $(Q3LITE_SERVER_SRCS) \
                       $(Q3_SRC)/client/cl_avi.c \
                       $(Q3_SRC)/client/cl_cgame.c \
                       $(Q3_SRC)/client/cl_cin.c \
                       $(Q3_SRC)/client/cl_console.c \
                       $(Q3_SRC)/client/cl_input.c \
                       $(Q3_SRC)/client/cl_keys.c \
                       $(Q3_SRC)/client/cl_main.c \
                       $(Q3_SRC)/client/cl_net_chan.c \
                       $(Q3_SRC)/client/cl_parse.c \
                       $(Q3_SRC)/client/cl_scrn.c \
                       $(Q3_SRC)/client/cl_ui.c \
                       $(Q3_SRC)/client/con_log.c \
                       $(Q3_SRC)/client/snd_adpcm.c \
                       $(Q3_SRC)/client/snd_codec_wav.c \
                       $(Q3_SRC)/client/snd_codec.c \
                       $(Q3_SRC)/client/snd_dma.c \
                       $(Q3_SRC)/client/snd_main.c \
                       $(Q3_SRC)/client/snd_mem.c \
                       $(Q3_SRC)/client/snd_mix.c \
                       $(Q3_SRC)/client/snd_wavelet.c 

# Combine all source lists
ALL_C_SRCS   = stubs.c $(Q3LITE_CORE_SRCS) $(Q3LITE_CIRCLE_SRCS) \
               $(Q3LITE_RENDERER_SRCS) $(Q3LITE_JPEG_SRCS) $(Q3LITE_ZLIB_SRCS) \
               $(Q3LITE_BOTLIB_SRCS)
ALL_CPP_SRCS = main.cpp kernel.cpp $(Q3LITE_SYS_CPPS)

MAIN_OBJS    = main.o kernel.o 
OTHER_CPPS   = $(notdir $(Q3LITE_SYS_CPPS:.cpp=.o))
OTHER_CS     = $(notdir $(ALL_C_SRCS:.c=.o))
OBJS         = $(MAIN_OBJS) $(OTHER_CPPS) $(OTHER_CS)

# Enable Standard Library integration level for Circle-Stdlib
STDLIB_SUPPORT = 4

include $(STDLIBHOME)/Config.mk
include $(CIRCLEHOME)/Rules.mk

# -DCIRCLE_VM_JIT_EXPERIMENTAL \

# Include Paths & CFLAGS (Added -DBOTLIB)
CFLAGS += -I. \
	  -I"$(NEWLIBDIR)/include" \
          -I../libs/circle-stdlib/include \
          -I$(CIRCLEHOME)/addon \
          -D__CIRCLE__ -D__RASPBERRYPI__ -DHAVE_BOOLEAN -DNO_NET \
          -DOS_STRING=\"circle\" -DARCH_STRING=\"arm\" \
          -DID_INLINE=inline -DPATH_SEP=47 -DPATH_SEP_CHAR=47 -D__Q3_PATH_SEP__=47 \
          -DDLL_EXT=\".so\" -DQ3_LITTLE_ENDIAN -DUSE_GLES -DQGL_DIRECT \
          -mno-unaligned-access -DBOTLIB \
          -D_SYS__PTHREADTYPES_H_ \
	  -DCIRCLE_VM_JIT_EXPERIMENTAL \
          -I$(Q3_SRC)/qcommon \
          -I$(Q3_SRC)/client \
          -I$(Q3_SRC)/server \
          -I$(Q3_SRC)/sys \
          -I$(Q3_SRC)/circle \
          -I$(Q3_SRC)/botlib \
          -I$(Q3_SRC) \
          -I$(Q3_SRC)/zlib \
          -I$(Q3_SRC)/jpeg-8c \
          -I"$(CIRCLEHOME)/addon/vc4/interface/khronos/include"

# Inherit CFLAGS for C++ compiler
CXXFLAGS += $(CFLAGS) -std=gnu++14

# Static hardware/system libraries link group
LIBS := --start-group \
       "$(NEWLIBDIR)/lib/libcirclenewlib.a" \
       "$(NEWLIBDIR)/lib/libc.a" \
       "$(NEWLIBDIR)/lib/libm.a" \
       $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
       $(CIRCLEHOME)/lib/sound/libsound.a \
       $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/addon/vc4/interface/bcm_host/libbcm_host.a \
       $(CIRCLEHOME)/addon/vc4/interface/khronos/libkhrn_client.a \
       $(CIRCLEHOME)/addon/vc4/interface/vmcs_host/libvmcs_host.a \
       $(CIRCLEHOME)/addon/vc4/interface/vcos/libvcos.a \
       $(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
       $(CIRCLEHOME)/addon/vc4/sound/libvchiqsound.a \
       $(CIRCLEHOME)/addon/linux/liblinuxemu.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/libcircle.a \
       --end-group

# Automated VPATH patterns to find sources across subdirectories
vpath %.c $(Q3_SRC)/qcommon
vpath %.c $(Q3_SRC)/client
vpath %.c $(Q3_SRC)/server
vpath %.c $(Q3_SRC)/circle
vpath %.c $(Q3_SRC)/sys
vpath %.c $(Q3_SRC)/botlib
vpath %.c $(Q3_SRC)/renderercommon
vpath %.c $(Q3_SRC)/renderergles1
vpath %.c $(Q3_SRC)/jpeg-8c
vpath %.c $(Q3_SRC)/zlib

vpath %.cpp $(Q3_SRC)/circle
vpath %.cpp $(Q3_SRC)/sys
vpath %.cpp .

# Clean generic C rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Generic compilation rule for C++ files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
