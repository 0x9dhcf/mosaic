/*
 * Copyright (c) 2019 Pierre Evenou
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __MWM_LOG_H__
#define __MWM_LOG_H__

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
