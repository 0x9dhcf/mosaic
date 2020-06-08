.POSIX:

TARGET	= mosaic
MAJOR 	= 5
MINOR 	= 0
PATCH 	= 0
VERSION = ${MAJOR}.${MINOR}.${PATCH}

PREFIX = /usr/local

PKG_CONFIG = pkg-config

DEPS = xcb\
       xcb-util\
       xcb-keysyms\
       xcb-xkb\
       xkbcommon\
       xkbcommon-x11\
       xcb-randr\
       xcb-icccm\
       xcb-ewmh\

SRC = bar.c\
      client.c\
      events.c\
      hints.c\
      mosaic.c\
      monitor.c\
      settings.c\
      x11.c

INC = `$(PKG_CONFIG) --cflags $(DEPS)`
LIB = `$(PKG_CONFIG) --libs $(DEPS)` 
OBJ = ${SRC:.c=.o}

CPPFLAGS 	= -DVERSION=\"${VERSION}\"
CFLAGS 		= -Wall -Wextra $(INC)
LDFLAGS 	= $(LIB)

.ifdef NDEBUG
CFLAGS 	+= -O2 -DNDEBUG
.else
CFLAGS 	+= -g -O0
.endif

all: options ${TARGET}

options:
	@echo build options: 
	@echo "CPPLAGS  = ${CPPFLAGS}"
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"

.c.o:
	${CC} -c ${CPPFLAGS} ${CFLAGS} $<

${TARGET}: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ${TARGET} ${OBJ}

dist: clean
	mkdir -p ${TARGET}-${VERSION}
	cp -R Makefile LICENCE README ${TARGET}.1 
	tar -zcf ${TARGET}-${VERSION}.tar.gz ${TARGET}-${VERSION}
	rm -rf ${TARGET}-${VERSION}

install: all
	mkdir -p $(PREFIX)/bin
	cp -f ${TARGET} $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/${TARGET}

.PHONY: all options clean dist install uninstall
