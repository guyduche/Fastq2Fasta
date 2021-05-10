FROM ubuntu:20.04

LABEL author="Aurelien Guy-Duche"

RUN apt update && apt install -y make gcc zlib1g-dev

ADD src /fastq2fasta

RUN cd fastq2fasta && make

WORKDIR /data

ENTRYPOINT ["fastq2fasta"]

CMD ["--help"]
