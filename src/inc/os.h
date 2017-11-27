#ifndef OS_H
#define OS_H

#define MAXPATH                 256     // Maximum filename length (including trailing zero)
#define MFSNAMELEN              16      // Length of fs type name

#define PS1                     '/'     // Primary path separator
#define PS2                     '\\'    // Alternate path separator

#define O_BINARY 0
#define S_IREAD  0

struct section *osconfig();

#define INFINITE  0xFFFFFFFF

#include <fcntl.h>
#include <sys/stat.h>

#define fdin stdin
#define fdout stdout
#define fderr stderr
#endif
