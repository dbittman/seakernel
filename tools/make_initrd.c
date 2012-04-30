/* From jamesM's tutorials */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
struct initrd_header
{
	unsigned char magic;
	char name[64];
	unsigned int offset;
	unsigned int length;
};

int main(char argc, char **argv)
{
	
	int nheaders = (argc-1)/2;
	struct initrd_header headers[64];
	unsigned int off = sizeof(struct initrd_header) * 64 + sizeof(int);

	int i;
	for(i = 0; i < nheaders; i++)
	{
		strcpy(headers[i].name, argv[i*2+2]);
		headers[i].offset = off;
		FILE *stream = fopen(argv[i*2+1], "r");
		if(stream == 0)
		{
			printf("Error: file not found: %s\n", argv[i*2+1]);
			return 1;
		}
		fseek(stream, 0, SEEK_END);
		headers[i].length = ftell(stream);
		off += headers[i].length;
		fclose(stream);
		headers[i].magic = 0xBF;
	}
	
	FILE *wstream = fopen("./initrd.img", "w");
	unsigned char *data = (unsigned char *)malloc(off);
	unsigned char mag[4] = "IRD1";
	fwrite(mag, sizeof(unsigned char), 4, wstream);
	fwrite(&nheaders, sizeof(int), 1, wstream);
	fwrite(headers, sizeof(struct initrd_header), 64, wstream);
	
	for(i = 0; i < nheaders; i++)
	{
		FILE *stream = fopen(argv[i*2+1], "r");
		unsigned char *buf = (unsigned char *)malloc(headers[i].length);
		fread(buf, 1, headers[i].length, stream);
		fwrite(buf, 1, headers[i].length, wstream);
		fclose(stream);
		free(buf);
	}
	
	fclose(wstream);
	free(data);

	return 0;	
}
