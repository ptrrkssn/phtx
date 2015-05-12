# Makefile for phtx - Peter's HTML Table Extractor

CC=gcc
CPPFLAGS=-D USER="\"$${USER:-$$LOGNAME}\"" -D HOST="\"$$HOST\""
CFLAGS=-O -g -Wall
DEST=/usr/local/bin
DIFF=diff

OBJS=phtx.o entities.o version.o

all: phtx

phtx: $(OBJS)
	$(CC) -o phtx $(OBJS)

phtx.o: 	phtx.c entities.h
entities.o: 	entities.c entities.h
version.o:	version.c

version.c:
	basename `pwd` | awk -F- '{print "char version[] = \"" $$2 "\";"}' >version.c

clean:
	-rm -f *.o core phtx *~ \#* t/*.out t/*.log t/*~ t/\#*

distclean: clean
	-rm -f version.c

install: phtx
	cp phtx $(DEST)

test:	phtx
	@for F in "" "-r" "-M2" "-f" "-D," "-E-" "-R" "-s" "-ss" ; do \
	    printf "Test(%s):\t" "$$F" ; \
	    for TH in t/[0-9]*.html; do \
		T="`basename $$TH .html`" ; \
	    	printf " %s" "$$T" ; \
		if (./phtx $$F t/$$T.html >t/$$T$$F.out && $(DIFF) t/$$T$$F.out t/$$T$$F.ok >t/$$T$$F.log 2>/dev/null) then \
	            true ; \
	        else \
	            printf "!"; \
	        fi; \
	    done ; \
	    echo "" ; \
	done

dist:	distclean
	NAME="`basename \`pwd\``" ; cd .. ; tar cpf - $$NAME | gzip >$$NAME.tar.gz
