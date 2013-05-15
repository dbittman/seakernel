/* From jamesM's tutorials */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
struct initrd_header
{
	unsigned char magic;
	char name[256];
	unsigned int offset;
	unsigned int length;
};

int main(int argc, char **argv)
{
	int nheaders = (argc-1)/2;
	struct initrd_header headers[nheaders];
	unsigned int off = sizeof(struct initrd_header) * nheaders + sizeof(int);
	printf("num headers: %d\n", nheaders);
	int i;
	for(i = 0; i < nheaders; i++)
	{
		char print[256];
		sprintf(print, "processing header %d/%d: %s", i, nheaders, basename(argv[i*2+2]));
		printf("%s", print);
		fflush(stdout);
		strcpy(headers[i].name, argv[i*2+2]);
		headers[i].offset = off;
		FILE *stream = fopen(argv[i*2+1], "r");
		if(stream == 0)
		{
			fprintf(stderr, "Error: file not found: %s\n", argv[i*2+1]);
			return 1;
		}
		fseek(stream, 0, SEEK_END);
		headers[i].length = ftell(stream);
		off += headers[i].length;
		fclose(stream);
		headers[i].magic = 0xBF;
		printf("\r");
		int j;
		for(j=0;j<strlen(print);j++) printf(" ");
		printf("\r");
	}
	printf("processing headers... done\n");
	FILE *wstream = fopen("./initrd.img", "w");
	unsigned char *data = (unsigned char *)malloc(off);
	unsigned char mag[4] = "IRD2";
	fwrite(mag, sizeof(unsigned char), 4, wstream);
	fwrite(&nheaders, sizeof(int), 1, wstream);
	fwrite(headers, sizeof(struct initrd_header), nheaders, wstream);
	
	for(i = 0; i < nheaders; i++)
	{
		char print[256];
		sprintf(print, "writing data %d/%d: %s", i, nheaders, basename(argv[i*2+2]));
		printf("%s", print);
		fflush(stdout);
		FILE *stream = fopen(argv[i*2+1], "r");
		unsigned char *buf = (unsigned char *)malloc(headers[i].length);
		fread(buf, 1, headers[i].length, stream);
		fwrite(buf, 1, headers[i].length, wstream);
		fclose(stream);
		free(buf);
		
		printf("\r");
		int j;
		for(j=0;j<strlen(print);j++) printf(" ");
		printf("\r");
	}
	printf("initrd generated. flushing stream...\n");
	fflush(wstream);
	fclose(wstream);
	free(data);
	printf("generated initrd.img\n");
	return 0;	
}
