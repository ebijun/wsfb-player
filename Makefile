PROG = wsfb-player
SRCS = wsfb_player.c

PREFIX ?= /usr/pkg
CFLAGS ?= -O2

# ARCH 判定およびコンパイルオプション設定
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),evbarm)
    CFLAGS += -mfpu=neon
endif

INCLUDES = -I$(PREFIX)/include/ffmpeg7 -I$(PREFIX)/include
LDFLAGS_OPT = -L$(PREFIX)/lib/ffmpeg7 -Wl,-R$(PREFIX)/lib/ffmpeg7 \
              -L$(PREFIX)/lib -Wl,-R$(PREFIX)/lib -Wl,-rpath-link,$(PREFIX)/lib

LIBS = -pthread -lavformat -lavcodec -lswscale -lavutil

all: $(PROG)

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(LDFLAGS_OPT) $(SRCS) -o $(PROG) $(LIBS)

clean:
	rm -f $(PROG) *.o

.PHONY: all clean

