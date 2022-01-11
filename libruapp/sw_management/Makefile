CC=gcc
CFLAGS+=-O -I../..
ZLIB=-L../.. -lz 

UNZ_OBJS = parse_xml_for_software_download.o unzip.o ioapi.o md5.o  md5_one.o md5_dgst.o main.o
ZIP_OBJS = app.o zip.o   ioapi.o

.c.o:
	$(CC) -c $(CFLAGS) $*.c

miniunz:  $(UNZ_OBJS)
	$(CC) $(CFLAGS) -o $@ $(UNZ_OBJS) $(ZLIB) $(LDFLAGS)

clean:
	rm parse_xml_for_software_download.o unzip.o ioapi.o md5.o  md5_one.o md5_dgst.o main.o
