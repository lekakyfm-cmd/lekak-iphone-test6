#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
FILE *LekakTest_Tmpfile(void){
 const char *directory=getenv("LEKAK_TEST_TMPDIR");if(!directory)directory=".";
 size_t size=strlen(directory)+32;char *name=malloc(size);if(!name)return NULL;
 snprintf(name,size,"%s/lekak-fixture-XXXXXX",directory);
 int descriptor=mkstemp(name);if(descriptor<0){free(name);return NULL;}
 unlink(name);free(name);FILE *file=fdopen(descriptor,"w+b");
 if(!file)close(descriptor);
 return file;
}
