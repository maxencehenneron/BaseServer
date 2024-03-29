#ifndef _TYPES_H
#define _TYPES_H

#include <string.h>

#ifdef INT_2_BYTES
typedef char int8;
typedef unsigned char uint8;
typedef int int16;
typedef unsigned int uint16;
typedef long int32;
typedef unsigned long uint32;
#else
typedef signed char     int8;
typedef unsigned char	uint8;
typedef short           int16;
typedef unsigned short  uint16;
typedef int             int32;
typedef unsigned int    uint32;
typedef long long int64;
typedef unsigned long long uint64;
#endif

typedef struct{
	std::string username;
	std::string password;
	std::string email;
} game_account;

typedef struct{
	int account_id;
	int friend_id;
} friendship;
#endif
