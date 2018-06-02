// jrra 2018

#ifndef _ENDIAN_H_
#define _ENDIAN_H_

#include <arpa/inet.h>

#ifndef be32toh
	#define be32toh ntohl
#endif

#ifndef be16toh
	#define be16toh ntohs
#endif

#endif // _ENDIAN_H_
