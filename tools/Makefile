
INSTALLDIR = ./_release
FFMPEGDIR = /home/tvie/work/ffmpegtools/_release_ffmpeg_2_3_3

CPPFLAGS = -I. -I$(FFMPEGDIR)/include 
CFLAGS += -Wall -std=c99 -g
LDFLAGS += -lpthread -ldl -lrt


InfoSource := info.c 
InfoOBJECTS := $(patsubst %.c, %.o, $(InfoSource))
PicSource := getpic.c
PicOBJECTS := $(patsubst %.c, %.o, $(PicSource))


BUILD_ARCH = $(shell uname -m)

.PHONY: all

all: info getpic
	mkdir -p $(INSTALLDIR)/bin
	install -m 755 info $(INSTALLDIR)/bin/info


info: $(InfoOBJECTS) $(FFMPEGDIR)/lib/libavformat.a $(FFMPEGDIR)/lib/libavcodec.a \
$(FFMPEGDIR)/lib/libavutil.a $(FFMPEGDIR)/lib/libswscale.a $(FFMPEGDIR)/lib/libswresample.a
	$(CC) $^ -o $@  -lm -lz -lbz2  $(CPPFLAGS) $(LDFLAGS) $(LINKFLAGS)

getpic: $(PicOBJECTS) $(FFMPEGDIR)/lib/libavformat.a $(FFMPEGDIR)/lib/libavcodec.a \
$(FFMPEGDIR)/lib/libavutil.a $(FFMPEGDIR)/lib/libswscale.a $(FFMPEGDIR)/lib/libswresample.a
	$(CC) $^ -o $@  -lm -lz -lbz2  $(CPPFLAGS) $(LDFLAGS) $(LINKFLAGS)



clean:
	rm -rf *.o info
