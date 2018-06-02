//	sf64ex - extract files from star fox 64, lylat wars
// 	jrra 2018

#define _GNU_SOURCE	// for memmem
#define _USE_GNU	// for memmem
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#define die(...) {				\
	fprintf(stderr,"%s: ", argv[0]);	\
	fprintf(stderr, __VA_ARGS__);		\
	fprintf(stderr,"\n");			\
	exit(1);				\
}

struct {
	char *description;
	uint8_t sigdata[20];
	int end;
} const dmadata_sigs[] = {
	{
		.description = "Star Fox 64",	// or Lylat Wars
		.sigdata = 
		{0,0,0,0, 0,0,0,0, 0,0,0x10,0x50, 0,0,0,0, 0,0,0x10,0x50 },
	},
	{
		.description = "Star Fox 64 iQue",
		.sigdata = 
		{0,0,0,0, 0,0,0,0, 0,0,0x10,0x60, 0,0,0,0, 0,0,0x10,0x60 },
	},
	{
		.description = "no dmadata found",
		.end = 1,
	},
};

uint8_t *find_dmadata(uint8_t *rom, size_t size)
{	
	uint8_t *dmadata = NULL;
	int i;
	for (i=0; !dmadata_sigs[i].end; i++)
	{
		dmadata = memmem(rom, size, dmadata_sigs[i].sigdata, 20);
		if (dmadata)
			break;
	}
	
	printf("dmadata type: %s\n", dmadata_sigs[i].description);
	
	return dmadata;
}

uint32_t mio0_get_size(uint8_t *s)
{
	return be32toh( *(uint32_t*)(s+4) );
}

void mio0_decode(uint8_t *s, uint8_t *d)
{
	uint8_t *backreferences	= s + be32toh( *(uint32_t*)(s+8) );
	uint8_t *literals	= s + be32toh( *(uint32_t*)(s+12) );
	uint8_t *end		= d + be32toh( *(uint32_t*)(s+4) );
	s += 16; // thanks hcs
	int	bits_left = 0;
	int32_t bits;
	do {
		if(bits_left == 0) {
			bits = be32toh( *(uint32_t*)s );
			bits_left = 32;
			s += 4;
		}
		if(bits < 0) {
			// literal
			*(d++) = *(literals++);
		} else {
			// backreference
			uint16_t codeword = be16toh( *(uint16_t*)backreferences );
			backreferences += 2;
			unsigned int count = codeword >> 12;
			unsigned int offset = codeword & 0xfff;
			uint8_t *backref = d - offset;
			count += 3;
			do {
				count--;
				*(d++) = *(backref-1);	// yes really
				backref++;
			} while(count != 0);
		}
		bits <<= 1;
		bits_left--;
	} while(d < end);
}

int main(int argc, char *argv[])
{
	if(argc < 2)
		die("need a filename");
	
	char *filename = argv[1];
	struct stat sb;
	if(stat(filename, &sb) == -1)
		die("couldn't stat file '%s'", filename);
	int f = open(filename, O_RDONLY);
	if(!f)
		die("couldn't open file '%s'", filename);
	uint8_t *rom = (uint8_t *)mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE,
				       MAP_PRIVATE, f, 0);

	struct dmadata_elm_s {
		uint32_t vrom_start;
		uint32_t phys_start;
		uint32_t phys_end;
		uint32_t compressed;
	};
	struct dmadata_elm_s *tbl =
		(struct dmadata_elm_s *)find_dmadata(rom, sb.st_size);
	if(!tbl) die("couldn't find dmadata in '%s'", filename);
	int filenum = 0;
	unsigned totalsize = 0;
	do {
		struct dmadata_elm_s *elm = &tbl[filenum];
		elm->vrom_start = be32toh(elm->vrom_start);
		elm->phys_start = be32toh(elm->phys_start);
		elm->phys_end = be32toh(elm->phys_end);
		elm->compressed = be32toh(elm->compressed);
		unsigned size = elm->phys_end-elm->phys_start;
		if(elm->compressed) {
			size = mio0_get_size(rom+(elm->phys_start));
		}
		totalsize += size;
		printf("%5d:\t%8x %8x %8x %8dk %s\n",
			filenum,
			elm->vrom_start,
			elm->phys_start,
			elm->phys_end,
			size/1024,
			elm->compressed?"compressed":""
		);
		char outfilename[20];
		sprintf(outfilename, "f%02d.bin", filenum);
		FILE *f = fopen(outfilename, "w");
		if (!elm->compressed) {
			fwrite(rom+(elm->phys_start), size, 1, f);
		} else {
			uint8_t *buf = malloc(size);
			if (!buf)
				die("oops");
			mio0_decode(rom+(elm->phys_start), buf);
			fwrite(buf, size, 1, f);
			free(buf);
		}
		fclose(f);
	} while(tbl[++filenum].vrom_start != 0);
	printf("total size: %dk\n", totalsize/1024);

	munmap(rom, sb.st_size);
	close(f);
	return 0;
}

