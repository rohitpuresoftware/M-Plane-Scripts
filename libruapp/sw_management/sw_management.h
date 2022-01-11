/*
#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
        #ifndef __USE_FILE_OFFSET64
                #define __USE_FILE_OFFSET64
        #endif
        #ifndef __USE_LARGEFILE64
                #define __USE_LARGEFILE64
        #endif
        #ifndef _LARGEFILE64_SOURCE
                #define _LARGEFILE64_SOURCE
        #endif
        #ifndef _FILE_OFFSET_BIT
                #define _FILE_OFFSET_BIT 64
        #endif
#endif

*/

//#ifdef  __LINUX__
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
//#else
//#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
//#define FTELLO_FUNC(stream) ftello64(stream)
//#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
//#endif
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>
#include<errno.h>
#include <time.h>
#include "unzip.h"

#include <openssl/md5.h>

#include <dirent.h>
#include<fcntl.h>
#define MAXFILENAME (256)
#define WRITEBUFFERSIZE (8192)


int sw_install(char *,char *,char *);
