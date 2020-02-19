.PHONY: all clean release

SRCS = spc2wav.c
OBJS = $(SRCS:.c=.o)

EXE=

CFLAGS = -Wall -Wextra -fPIC -O2

all: spc2wav$(EXE)

spc2wav$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lspc -lid666

%.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $<

clean:
	rm -f spc2wav spc2wav.exe spc2wav.o

release:
	docker build -t snes_spc .
	mkdir -p output
	docker run --rm -ti -v $(shell pwd)/output:/output snes_spc
