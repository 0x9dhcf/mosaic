#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <stdlib.h>

#define FATAL(fmt, ...) do {\
    fprintf (stderr, "FATAL - [%s: %d]: " fmt "\n", __FILE__,__LINE__, ##__VA_ARGS__);\
    exit (EXIT_FAILURE);\
} while(0)

#define ERROR(fmt, ...) do {\
    fprintf (stderr, "ERROR - [%s: %d]: " fmt "\n", __FILE__,__LINE__, ##__VA_ARGS__);\
} while (0)

#define INFO(fmt, ...) do {\
    fprintf (stdout, "INFO: " fmt "\n", ##__VA_ARGS__);\
} while (0)

#ifndef NDEBUG
#define DEBUG(fmt, ...) do {\
    fprintf (stdout, "DEBUG - [%s: %d]: " fmt "\n", __FILE__,__LINE__, ##__VA_ARGS__);\
    fflush(stdout);\
} while (0)

#define DEBUG_FUNCTION DEBUG("%s", __FUNCTION__)
#else
#define DEBUG(fmt, ...)
#define DEBUG_FUNCTION
#endif

#endif
