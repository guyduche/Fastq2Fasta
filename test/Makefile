.PHONY: clean
.SHELL = /bin/bash
BINDIR = ../bin
SRCDIR = ../src
FASTQ = test_1.fastq.gz test_2.fastq.gz

test.fasta: ${BINDIR}/fastq2fasta ${FASTQ}
	$^ > $@

${BINDIR}/fastq2fasta:
	(cd ${SRCDIR}; ${MAKE})

clean:
	rm -f *.fasta
