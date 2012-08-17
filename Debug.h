#ifndef __Debug__h__
#define __Debug__h__

#include <stdio.h>

#define LOG(fmt, args...)	fprintf(stderr, fmt "\n", ## args)

#endif/*__Debug__h__*/
