# Vlix Makefile — C11 + ncursesw
CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra
CFLAGS  += -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700

# ncurses detection via pkg-config, fallback to -lncursesw
NC_CF := $(shell pkg-config --cflags ncursesw 2>/dev/null)
NC_LB := $(shell pkg-config --libs   ncursesw 2>/dev/null)
ifeq ($(NC_LB),)
  NC_LB := -lncursesw
endif
CFLAGS  += $(NC_CF)
LDFLAGS += $(NC_LB)

PREFIX  ?= /usr/local
SRCDIR   = src
SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(SOURCES:.c=.o)
TARGET   = vlicx

.PHONY: all clean install uninstall static

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/vlicx.h
	$(CC) $(CFLAGS) -c -o $@ $<

static:
	$(MAKE) clean
	$(MAKE) LDFLAGS="-static -lncursesw"

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -m 755 bin/vlicx-fo $(DESTDIR)$(PREFIX)/bin/vlicx-fo
	install -m 755 bin/vlicx-fi $(DESTDIR)$(PREFIX)/bin/vlicx-fi
	install -m 755 bin/vlicx-upd $(DESTDIR)$(PREFIX)/bin/vlicx-upd
	if [ -d /etc/profile.d ]; then install -m 755 bin/vlicx-login-check /etc/profile.d/vlicx-login-check.sh; fi

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -f $(DESTDIR)$(PREFIX)/bin/vlicx-fo
	rm -f $(DESTDIR)$(PREFIX)/bin/vlicx-fi
	rm -f $(DESTDIR)$(PREFIX)/bin/vlicx-upd
	rm -f /etc/profile.d/vlicx-login-check.sh

clean:
	rm -f $(SRCDIR)/*.o $(TARGET)
