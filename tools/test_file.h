#ifndef LEKAK_TEST_FILE_H
#define LEKAK_TEST_FILE_H
#include <stdio.h>
FILE *LekakTest_Tmpfile(void);
/* Test fixtures only: use the runner-owned writable temporary directory. */
#define tmpfile LekakTest_Tmpfile
#endif
