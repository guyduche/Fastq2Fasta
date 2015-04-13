#ifndef CIGAR_H_INCLUDED
#define CIGAR_H_INCLUDED

#include "parseXML.h"

int *cigarStrBuilding(Hsp *hsp, int queryLength, int *sizeCStr);

#endif // CIGAR_H_INCLUDED
