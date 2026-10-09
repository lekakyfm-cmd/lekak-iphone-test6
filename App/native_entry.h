#ifndef LEKAK_NATIVE_ENTRY_H
#define LEKAK_NATIVE_ENTRY_H
/* One game session per process until upstream state teardown is adapted.
 * Paths are app-private absolute paths, never security-scoped picker URLs. */
int LekakNative_Run(const char *disc,const char *program_root,const char *user_root);
void LekakNative_StopGame(void);
#endif
