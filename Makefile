.PHONY: all run clean

CFLAGS = $(shell pkg-config --cflags --libs libavformat)
CFLAGS += $(shell pkg-config --cflags --libs libavcodec)
CFLAGS += $(shell pkg-config --cflags --libs libavutil)
CFLAGS += $(shell pkg-config --cflags --libs libswscale)
SOURCES = tutorial1 tutorial2

all: $(SOURCES)

%: %.c
	gcc -o $@ $^ $(CFLAGS)

run:
	./${cmd} sample.mp4

clean:
	rm -rf $(SOURCES)
