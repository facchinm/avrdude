#------------------------------------------------------------------------
#
# $Id$
#


TARGET      = avrdude

PREFIX      ?= /usr/local
BINDIR       = ${PREFIX}/bin
MANDIR       = ${PREFIX}/man/man1
MANUAL       = avrdude.1
DOCDIR       = ${PREFIX}/share/doc/avrdude
CONFIGDIR    = ${PREFIX}/etc

DIRS         = ${BINDIR} ${MANDIR} ${DOCDIR} ${CONFIGDIR}

INSTALL      = /usr/bin/install -c -o root -g wheel

CFLAGS       += -Wall -DCONFIG_DIR=\"${CONFIGDIR}\" ${YYDEF}
#CFLAGS       = -g -Wall -DCONFIG_DIR=\"${CONFIGDIR}\" ${YYDEF}
LDFLAGS      =  
YFLAGS       = -t -d -v

INSTALL_PROGRAM = ${INSTALL} -m 555 -s
INSTALL_DATA    = ${INSTALL} -m 444
INSTALL_MANUAL  = ${INSTALL_DATA}


LIBS       = -lreadline

YYDEF  = -DYYSTYPE="struct token_t *"

OBJS = config_gram.o avr.o config.o fileio.o lexer.o lists.o main.o pgm.o \
	ppi.o stk500.o term.o

all : ${TARGET}


${TARGET} : ${OBJS}
	${CC} ${LDFLAGS} -o ${TARGET} ${OBJS} ${LIBS}

clean :
	rm -f *.o lexer.c ${TARGET} *~ *.core y.tab.c y.tab.h y.output

install : dirs                             \
	  ${BINDIR}/${TARGET}              \
	  ${MANDIR}/${MANUAL}              \
	  ${DOCDIR}/avrdude.pdf            \
	  ${CONFIGDIR}/avrdude.conf.sample \
	  ${CONFIGDIR}/avrdude.conf

config_gram.o : avr.h config.h lists.h pindefs.h

avr.o: avr.c avr.h avrpart.h lists.h pgm.h pindefs.h config.h ppi.h

config.o: config.c avr.h avrpart.h lists.h pgm.h pindefs.h config.h

fileio.o: fileio.c avr.h avrpart.h lists.h pgm.h pindefs.h fileio.h

lists.o: lists.c lists.h

main.o: main.c avr.h avrpart.h lists.h pgm.h pindefs.h config.h fileio.h \
	ppi.h term.h

pgm.o: pgm.c pgm.h avrpart.h lists.h pindefs.h

ppi.o: ppi.c avr.h avrpart.h lists.h pgm.h pindefs.h ppi.h config.h

stk500.o: stk500.c avr.h avrpart.h lists.h pgm.h pindefs.h stk500_private.h

term.o: term.c avr.h avrpart.h lists.h pgm.h pindefs.h config.h ppi.h


dirs :
	@for dir in ${DIRS}; do \
	  if [ ! -d $$dir ]; then \
	    echo "creating directory $$dir"; \
	    mkdir -p $$dir; \
	  fi \
	done

${BINDIR}/${TARGET} : ${TARGET}
	${INSTALL_PROGRAM} ${TARGET} $@

${MANDIR}/${MANUAL} : ${MANUAL}
	${INSTALL_MANUAL} ${MANUAL} $@

${DOCDIR}/avrdude.pdf : avrdude.pdf
	${INSTALL_DATA} avrdude.pdf $@

${CONFIGDIR}/avrdude.conf.sample : avrdude.conf.sample
	${INSTALL_DATA} avrdude.conf.sample $@

${CONFIGDIR}/avrdude.conf : avrdude.conf.sample
	@if [ -f ${CONFIGDIR}/avrdude.conf ]; then                       \
	  export TS=`date '+%Y%m%d%H%M%S'`;                              \
	  echo "NOTE: backing up ${CONFIGDIR}/avrdude.conf to ${CONFIGDIR}/avrdude.conf.$${TS}"; \
	  cp -p ${CONFIGDIR}/avrdude.conf ${CONFIGDIR}/avrdude.conf.$${TS}; \
	fi
	${INSTALL_DATA} avrdude.conf.sample $@

