/*
The MIT License (MIT)

Copyright (c) 2015 Aurelien Guy-Duche

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


History:
* 2015 creation

*/
#include <string.h>
#include "utils.h"
#include "blastSam.h"

/************************************************************************************/
/*	Print SAM header																*/
/************************************************************************************/
// Print the reference sequence dictionary (SQ lines)
static void sq_line(AppParamPtr app, char* filename)
{
	FILE* reader;
	char* str = NULL;
	int c, countSpace = 0;
	size_t lenStr = 0;

	reader = safeFOpen(filename, "r");						// Open the reference dictionary file (.dict)

	do
	{
		if (countSpace > 2 || !countSpace)					// Skip the first line of the file and the end of the other lines
			do c = fgetc(reader); while (c != '\n' && c != EOF);

		lenStr = 0;
		countSpace = 0;
		str = NULL;
		c = fgetc(reader);
		lenStr++;

		while (c != '\n' && c != EOF && countSpace <= 2)	// Keep only the reference name and its length
		{
			if (c == '\t') countSpace++;
			str = (char*) safeRealloc(str, lenStr+1);
			str[lenStr-1] = (char) c;
			c = fgetc(reader);
			lenStr++;
		}

		fwrite(str, sizeof(char), lenStr-1, app->out);		// Print the SQ line in the SAM file
		free(str);
		if (c != EOF) fputc('\n', app->out);

	} while (c != EOF);										// If there are more than one reference

	fclose(reader);
}

// Print the read group line
static int rg_line(AppParamPtr app, char* readGroup)
{	
	if (strstr(readGroup, "@RG") != readGroup) return 1;	// Verify that the read group begins with @RG
	if (strstr(readGroup, "\tID:") == NULL) return 1;		// Verify that the read group has an ID
	fprintf(app->out, "%s\n", readGroup);					// Print the read group in the SAM header
	return 0;
}

// Print SAM header section
int samHead(AppParamPtr app)
{
	sq_line(app, app->db);									// Print the SQ Line
	if (app->readGroup != NULL)								// If the read group is available and is correctly formatted, prints it
		if (rg_line(app,app->readGroup) == 1) return 1;		// If the read group is incorrectly formatted, returns an error
	fprintf(app->out, "%s\n", app->pg_line);				// Print the PG line
	return 0;
}


/************************************************************************************/
/*	Build the CIGAR string corresponding to the read alignment						*/
/************************************************************************************/
// Put the count and the symbol in the CIGAR structure
#define CSTRMACRO(TAG, FUNCT) { \
		COUNTCHARMACRO(FUNCT); \
		cigar->elements[cigar->size-1]->count = count; \
		cigar->elements[cigar->size-1]->symbol = TAG;}

// Count the number of alignment event
#define COUNTCHARMACRO(FUNCT) { \
		count = 1; \
		pos++; \
		while (pos < (hsp->hsp_align_len) && FUNCT) \
			count++, pos++; \
		pos--;}

static CigarPtr cigarStrBuilding(SamOutputPtr samOut)
{
	int pos = 0, count = 0;
	size_t queryLength = samOut->query->read_len;
	HspPtr hsp = samOut->hsp;
	CigarPtr cigar = (CigarPtr) safeCalloc(1, sizeof(Cigar));

	if (hsp->hsp_query_from > 1)				// 5' Soft clipping
	{
		cigar->size++;
		cigar->elements = (CigarElementPtr*) safeCalloc(cigar->size, sizeof(CigarElementPtr));	// Create an array of elements for the CIGAR string
		cigar->elements[0] = (CigarElementPtr) safeCalloc(1, sizeof(CigarElement));				// Create the first element of the array
		cigar->elements[0]->count = hsp->hsp_query_from - 1;									// Count = first alignment position in the query sequence - 1
		cigar->elements[0]->symbol = 'S';
	}

	for (pos = 0; pos < (hsp->hsp_align_len); pos++)
	{
		cigar->size++;
		cigar->elements = (CigarElementPtr*) safeRealloc(cigar->elements, cigar->size * sizeof(CigarElementPtr));	// Create or append the array of elements of the CIGAR string
		cigar->elements[cigar->size-1] = (CigarElementPtr) safeCalloc(1, sizeof(CigarElement));						// Create a new element

		if (hsp->hsp_hseq[pos] == '-')
			CSTRMACRO('I', (hsp->hsp_hseq[pos] == '-'))								// Count the number of insertions

		else if (hsp->hsp_qseq[pos] == '-')
			CSTRMACRO('D', (hsp->hsp_qseq[pos] == '-'))								// Count the number of deletions

		else if (hsp->hsp_hseq[pos] == hsp->hsp_qseq[pos])
			CSTRMACRO('=', (hsp->hsp_hseq[pos] == hsp->hsp_qseq[pos]))				// Count the number of matches

		else
			CSTRMACRO('X', (hsp->hsp_hseq[pos] != '-' && hsp->hsp_qseq[pos] != '-' && hsp->hsp_hseq[pos] != hsp->hsp_qseq[pos]))	// Count the number of mismatches

		if (cigar->elements[cigar->size-1]->count >= 100 && cigar->elements[cigar->size-1]->symbol == 'D')
			cigar->elements[cigar->size-1]->symbol = 'N';							// If there is more than a hundred deletion at a time, it is considered a skipped region

		if (cigar->elements[cigar->size-1]->symbol != '=')
			cigar->nbDiff += cigar->elements[cigar->size-1]->count;					// Count the number of I, D/N and X for the NM tag
	}

	if ((queryLength - hsp->hsp_query_to) > 0)	// 3' Soft clipping
	{
		cigar->size++;
		cigar->elements = (CigarElementPtr*) safeRealloc(cigar->elements, cigar->size * sizeof(CigarElementPtr));
		cigar->elements[cigar->size-1] = (CigarElementPtr) safeCalloc(1, sizeof(CigarElement));
		cigar->elements[cigar->size-1]->count = queryLength - hsp->hsp_query_to;	// Count = length of the read - position of the last aligned base on the query sequence 
		cigar->elements[cigar->size-1]->symbol = 'S';
	}

	return cigar;
}


/************************************************************************************/
/*	Get the reverse complement of a sequence										*/
/************************************************************************************/ 
static char* revStr(char* oldStr)
{
	size_t i = 0;
	int l = strlen(oldStr);
	char* newStr = safeCalloc(l+1, sizeof(char));

	for (--l; l >= 0; l--, i++)						// Read oldStr in reverse and newStr straight
	{
		switch (oldStr[l])							// For each char in oldStr, put the complement in the newStr
		{
			case 'A': case 'a' : newStr[i] = 'T'; break;
			case 'T': case 't' : newStr[i] = 'A'; break;
			case 'C': case 'c' : newStr[i] = 'G'; break;
			case 'G': case 'g' : newStr[i] = 'C'; break;
			default: newStr[i] = oldStr[l]; break;	// If not in {ATCG}, keep the same char
		}
	}
	return newStr;
}


/************************************************************************************/
/*	Extract the beginning position in the reference name, if there is one								*/
/************************************************************************************/
// Search for the first colon in the reference name and extract the number directly after
static int firstPosRef(const char* rname)
{
	char* colon = strchr(rname, ':');
	if (colon == NULL) 
		return 0;
	return strtol(colon + 1, NULL, 10);
}

/************************************************************************************/
/*	Print SAM alignment section														*/
/************************************************************************************/
// SAM flags
#define SAM_PAIRED 0x1				// Paired end
#define SAM_PROPER_PAIR 0x2			// Read mapped in a proper pair
#define SAM_UNMAP 0x4				// Read unmapped
#define SAM_MUNMAP 0x8				// Mate unmapped
#define SAM_REVERSE 0x10			// Read mapped on the reverse strand
#define SAM_MREVERSE 0x20			// Mate mapped on the reverse strand
#define SAM_READF 0x40				// Read is first in pair
#define SAM_READL 0x80				// Read is last in pair
#define SAM_SECONDARY 0x100			// Not primary alignment
#define SAM_QCFAIL 0x200			// Failed control quality
#define SAM_DUP 0x400				// Optical or PCR duplicate
#define SAM_SUPPLEMENTARY 0x800		// Supplementary alignment

// Function to filter the results according to option -W
static int allowedToPrint(SamOutputPtr* samOut, int minLen, int countRec, int countHSPsec, int* countUnprint)
{
	if (samOut[0]->hsp != NULL && samOut[0]->hsp->hsp_align_len > minLen)							// First read is mapped and alignment length is greater than the minimum length
	{
		if (samOut[1] != NULL && samOut[1]->hsp != NULL && samOut[1]->hsp->hsp_align_len < minLen)		// Second read is mapped and alignment length is less than the minimum length
		{
			if (*countUnprint != (countRec - countHSPsec))													// If not last first read HSP, the record is not printed
				{(*countUnprint)++; return 0;}	
			else																							// If last first read HSP, print with second read considered unmapped
				samOut[1]->hsp = NULL;
		}
	}
	
	else																							// First read is unmapped or alignment length less than the minimum length
	{
		if (samOut[1] != NULL && samOut[1]->hsp != NULL && samOut[1]->hsp->hsp_align_len > minLen)		// Second read is mapped and alignment length greater than the minimum given length
		{
			if (*countUnprint != (countRec - countHSPsec))													// If not last first read HSP, the record is not printed
				{(*countUnprint)++; return 0;}
			else																							// If last first read HSP or if first read unmapped, print with first read considered unmapped
				samOut[0]->hsp = NULL;
		}
		else																							// Single end or second read unmapped or alignment length less than the minimum length
		{
			if (*countUnprint != countRec -1)																// If not last record, it is not printed
				{(*countUnprint)++; return 0;}
			else																							// If last record, print with both reads considered unmapped
			{
				samOut[0]->hsp = NULL;
				if (samOut[1] != NULL)
					samOut[1]->hsp = NULL;
			}
		}
	}
	return 1;
}

// Print the CIGAR str straight or reverse depending on the flag
static void printCigarStr (AppParamPtr app, CigarElementPtr* cigElements, size_t size, int flag)
{
	int i = 0;
	if (flag & SAM_REVERSE)					// If the sequence is mapped on the reverse strand
		for (i = size - 1; i >= 0; i--)		// Go through the CIGAR elements in reverse order
		{
			fprintf(app->out, "%d%c", cigElements[i]->count, cigElements[i]->symbol);
			free(cigElements[i]);
		}

	else									// If the sequence is mapped on the forward strand
		for (i = 0; i < size; i++)			// Go through the CIGAR element in straight order
		{
			fprintf(app->out, "%d%c", cigElements[i]->count, cigElements[i]->symbol);
			free(cigElements[i]);
		}

	free(cigElements);						// The CIGAR string is freed just after being printed
}

// Structure that contains the infos of one line of SAM alignment section
typedef struct SamLine
{
	char* readName;		// Col 1	Field QNAME
	char* refName;		// Col 3	Field RNAME
	char* rnext;		// Col 7	Field RNEXT
	char* seq;			// Col 10	Field SEQ
	char* qual;			// Col 11	Field QUAL
	int flag;			// Col 2	Field FLAG
	int pos;			// Col 4	Field POS
	int mapq;			// Col 5	Field MAPQ
	int pnext;			// Col 8	Field PNEXT
	int tlen;			// Col 9	Field TLEN
	CigarPtr cigarStr;	// Col 6	Field CIGAR
} SamLine, *SamLinePtr;

// Print one line of the SAM alignment section
static void printSamLine (AppParamPtr app, SamLinePtr samLine)
{
	int i = 0;
	size_t qualLen = 0;
	char* seq = NULL;
	
	fputs(samLine->readName, app->out);																// Print QNAME
	fprintf(app->out, "\t%d\t", samLine->flag);														// Print FLAG
	fputs(samLine->refName, app->out);																// Print RNAME
	fprintf(app->out, "\t%d\t%d\t", samLine->pos, samLine->mapq);									// Print POS and MAPQ
	
	if (samLine->cigarStr != NULL)
	{
		printCigarStr (app, samLine->cigarStr->elements, samLine->cigarStr->size, samLine->flag);	// Print CIGAR
		free(samLine->cigarStr);
	}
	else
		fputc('*', app->out);
		
	fputc('\t', app->out);
	fputs(samLine->rnext, app->out);																// Print RNEXT
	fprintf(app->out, "\t%d\t%d\t", samLine->pnext, samLine->tlen);									// Print PNEXT and TLEN
	
	if (samLine->flag & SAM_REVERSE)																// If mapped on the reverse strand, print the reverse complement SEQ and the reverse of QUAL
	{
		seq = revStr(samLine->seq);
		fputs(seq, app->out);
		fputc('\t', app->out);
		
		qualLen = strlen(samLine->qual);
		for (i = qualLen - 1; i >= 0; i--)
			fputc(samLine->qual[i], app->out);
	}
	else																							// If mapped on the forward strand, print SEQ and QUAL as they came out of the sequencer
	{
		fputs(samLine->seq, app->out);
		fputc('\t', app->out);
		fputs(samLine->qual, app->out);
	}
																									// Metadata
	if (samLine->cigarStr != NULL)
		fprintf(app->out, "\tNM:i:%d", samLine->cigarStr->nbDiff);									// Print NM tag

	if (app->readGroupID != NULL)
		fprintf(app->out, "\tRG:Z:%s", app->readGroupID);											// Print RG tag
				
	fputc('\n', app->out);
}

// Print the alignment section
void printSam(IterationSamPtr itSam, AppParamPtr app)
{
	int i = 0, j = 0, k = 0;
	int invk = 0, len0 = 0, len1 = 0, doNotPrint = 0, countUnprint = 0;
	SamOutputPtr samOut[2] = {NULL, NULL};
	SamLinePtr samLine = NULL;

	for (i = 0; i < itSam->countHit; i++)				// Go through all the reference hits
	{
		for (j = 0; j < itSam->samHits[i]->countRec; j++)	// Go through all the records
		{
			for (k = 0; k < 2; k++)								// Print the first in pair and then the second
			{
				invk = (k ? 0 : 1);
				samOut[k] = itSam->samHits[i]->rsSam[j]->samOut[k];
				samOut[invk] = itSam->samHits[i]->rsSam[j]->samOut[invk];
				
				if (samOut[k] == NULL || doNotPrint) continue;			// When on the second in pair part of the record, if single end or record filtered, go to the next record

				if (app->minLen != 0 && !k)								// If option -W is active, filter the records
					if (!allowedToPrint(samOut, app->minLen, itSam->samHits[i]->countRec, itSam->samHits[i]->countHSPsec, &countUnprint))
						{doNotPrint = 1; continue;}
				
				samLine = (SamLinePtr) safeCalloc(1, sizeof(SamLine));	// Create a new line for the alignment section of the SAM file
				
				// Paired end
				if (samOut[invk] != NULL)
				{
					samLine->flag |= SAM_PAIRED | (!k ? SAM_READF : SAM_READL);
					
					// The mate is mapped
					if (samOut[invk]->hsp != NULL)
					{
						samLine->flag |= (samOut[invk]->hsp->hsp_hit_to < samOut[invk]->hsp->hsp_hit_from ? SAM_MREVERSE : 0);				// Mate mapped on the reverse strand ?
						samLine->pnext = (samLine->flag & SAM_MREVERSE ? samOut[invk]->hsp->hsp_hit_to : samOut[invk]->hsp->hsp_hit_from);	// PNEXT is the leftmost position of the mate alignment on the reference
						if (app->posOnChr) samLine->pnext += firstPosRef(samOut[invk]->rname);												// Adjust the position to the first position of the reference (-z)
						
						// The read is mapped
						if (samOut[k]->hsp != NULL)
						{
							samLine->tlen = samOut[invk]->hsp->hsp_hit_from - samOut[k]->hsp->hsp_hit_from;									// TLEN: distance between the first alignment position of both reads
							if (samLine->tlen)
								samLine->tlen += (samLine->tlen > 0 ? 1 : -1);
							
							len0 = abs(samOut[k]->hsp->hsp_hit_to - samOut[k]->hsp->hsp_hit_from) + 1;										// Length of the read alignment on the reference
							len1 = abs(samOut[invk]->hsp->hsp_hit_to - samOut[invk]->hsp->hsp_hit_from) + 1;								// Length of the mate alignment on the reference
							if (abs(samLine->tlen) > 3 * (len0 >= len1 ? len0 : len1) || abs(samLine->tlen) < (len0 >= len1 ? len1 : len0))	// Pair of reads not properly aligned
							{
								if (countUnprint != (itSam->samHits[i]->countRec -1))															// If not the last record, go to the next record
									{doNotPrint = 1; countUnprint++; free(samLine); continue;}
								else																											// If last record, both reads are considered unmapped
								{
									samLine->flag &= ~SAM_MREVERSE;
									samLine->flag |= SAM_MUNMAP;
									samLine->rnext = "*";
									samLine->pnext = 0;
									samOut[k]->hsp = NULL;
									samOut[invk]->hsp = NULL;
								}
							}
							else																											// Pair of reads properly aligned
							{
								samLine->rnext = "=";
								samLine->flag |= SAM_PROPER_PAIR;
							}
						}
						
						// The read is unmapped
						else
							samLine->rnext = shortName(samOut[invk]->rname);
					}
					
					// The mate is unmapped
					else
					{
						samLine->flag |= SAM_MUNMAP;
						samLine->rnext = "*";
						samLine->pnext = 0;
					}
				}
				
				// Single end
				else
				{
					samLine->rnext = "*";
					samLine->pnext = 0;
				}

				// Put the read infos in samLine
				samLine->readName = samOut[k]->query->name;
				samLine->seq = samOut[k]->query->seq;
				samLine->qual = samOut[k]->query->qual;
				
				// The read is mapped
				if (samOut[k]->hsp != NULL)
				{
					if (j - countUnprint != 0)																					// This is not the first record printed for this read
						samLine->flag |= SAM_SECONDARY;
					samLine->flag |= (samOut[k]->hsp->hsp_hit_to < samOut[k]->hsp->hsp_hit_from ? SAM_REVERSE : 0);				// The read is mapped on the reverse strand ?
					samLine->refName = shortName(samOut[k]->rname);																// Put a short version of the reference name in samLine
					samLine->pos = (samLine->flag & SAM_REVERSE ? samOut[k]->hsp->hsp_hit_to : samOut[k]->hsp->hsp_hit_from);	// POS is the leftmost position of the read alignment on the reference
					if (app->posOnChr)
						samLine->pos += firstPosRef(samOut[k]->rname);															// Adjust the position to the first position of the reference (-z)
					samLine->cigarStr = cigarStrBuilding(samOut[k]);															// Build the CIGAR string
					samLine->mapq = 60;																							// MAPQ
				}
				
				// The read is unmapped
				else
				{
					samLine->flag |= SAM_UNMAP;
					samLine->refName = "*";
				}
				
				// Print a new line in SAM alignment section
				printSamLine (app, samLine);
				free(samLine);
			}
		}
		countUnprint = 0;
	}
}
