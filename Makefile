target  ?= sf64ex
objects := $(patsubst %.c,%.o,$(wildcard *.c))
CFLAGS=-std=c99

.PHONY: all
all:	$(target)

.PHONY: clean
clean:
	rm -f $(target) $(objects)

$(target): $(objects)
