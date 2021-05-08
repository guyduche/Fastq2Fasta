.PHONY: all clean
CC = gcc
CFLAGS = -c -O3 -Wall
LDFLAGS = -lz
BINDIR = ../bin
EXEC = fastq2fasta
OBJS = $(addsuffix .o, ${EXEC} shortRead utils)

all: ${BINDIR} ${BINDIR}/${EXEC}

${BINDIR}:
	mkdir ../bin

%.o: %.c
	$(CC) ${CFLAGS} -o $@ $<

${BINDIR}/${EXEC}: ${OBJS}
	$(CC) -o $@ $^ ${LDFLAGS}

clean:
	rm -f ${BINDIR}/${EXEC} *.o

shortRead.o: shortRead.h utils.h
utils.o: utils.h
fastq2fasta.o: shortRead.h utils.h
