/**
 *  @file     typedefs.h
 *  @brief    basic types definition
 *
 *  @author   Jan Soucek (soucek@ufa.cas.cz)
 */

#ifndef TYPEDEFS_H_
#define TYPEDEFS_H_

#include <limits.h>

typedef enum { false = 0, true = 1} Boolean;

#ifndef LOGISCOPE
    /* Define U8 */
    #if (UCHAR_MAX != 0xff)
    #error "Unsigned short not 8-bit long"
    #endif

    /* Define U16 */
    #if (USHRT_MAX != 0xffff)
    #error "Unsigned short not 16-bit long"
    #endif

    /* Define U32 */
    #if (ULONG_MAX != 0xffffffff)
    #error "Unsigned int not 32-bit long"
    #endif

    /* Define U64 */
    #if (ULLONG_MAX != 0xffffffffffffffff)
    #error "Unsigned long not 64-bit long"
    #endif

    /* Define I8 */
    #if (SCHAR_MAX != 0x7f)
    #error "Signed short not 8-bit long"
    #endif

    /* Define I16 */
    #if (SHRT_MAX != 0x7fff)
    #error "Signed short not 16-bit long"
    #endif

    /* Define I32 */
    #if (LONG_MAX != 0x7fffffff)
    #error "Signed int not 32-bit long"
    #endif

    /* Define I64 */
    #if (LLONG_MAX != 0x7fffffffffffffff)
    #error "Signed long not 64-bit long"
    #endif
#endif

/** @brief Define unsigned char as U8. */
typedef unsigned char       U8;
/** @brief Define unsigned short as U16. */
typedef unsigned short      U16;
/** @brief Define unsigned int as U32. */
typedef unsigned long       U32;
/** @brief Define unsigned long long as U64. */
typedef unsigned long long  U64;
/** @brief Define char as I8. */
typedef signed char         I8;
/** @brief Define short as I16. */
typedef short               I16;
/** @brief Define int as I32. */
typedef long                I32;
/** @brief Define long long as I64. */
typedef long long           I64;

#endif /* TYPEDEFS_H_ */
