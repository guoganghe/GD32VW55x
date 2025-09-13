// "License": Public Domain
// I, Mathias Panzenböck, place this file hereby into the public domain. Use it at your own risk for whatever you like.

#ifndef PORTABLE_ENDIAN_H__
#define PORTABLE_ENDIAN_H__

#if BYTE_ORDER == BIG_ENDIAN
#define HTONLL(n) (n)
#define NTOHLL(n) (n)
#else
#define HTONLL(n) ((((unsigned long long)(n) & 0xFF) << 56) | \
                   (((unsigned long long)(n) & 0xFF00) << 40) | \
                   (((unsigned long long)(n) & 0xFF0000) << 24) | \
                   (((unsigned long long)(n) & 0xFF000000) << 8) | \
                   (((unsigned long long)(n) & 0xFF00000000) >> 8) | \
                   (((unsigned long long)(n) & 0xFF0000000000) >> 24) | \
                   (((unsigned long long)(n) & 0xFF000000000000) >> 40) | \
                   (((unsigned long long)(n) & 0xFF00000000000000) >> 56))

#define NTOHLL(n) ((((unsigned long long)(n) & 0xFF) << 56) | \
                   (((unsigned long long)(n) & 0xFF00) << 40) | \
                   (((unsigned long long)(n) & 0xFF0000) << 24) | \
                   (((unsigned long long)(n) & 0xFF000000) << 8) | \
                   (((unsigned long long)(n) & 0xFF00000000) >> 8) | \
                   (((unsigned long long)(n) & 0xFF0000000000) >> 24) | \
                   (((unsigned long long)(n) & 0xFF000000000000) >> 40) | \
                   (((unsigned long long)(n) & 0xFF00000000000000) >> 56))
#endif

#define htobe64(n) HTONLL(n)
#define be64toh(n) NTOHLL(n)

#endif

