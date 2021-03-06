
#ifndef DART_TYPES_H_INCLUDED
#define DART_TYPES_H_INCLUDED

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DART_INTERFACE_ON

/*
   --- DART Types ---
*/

typedef enum
  {
    DART_OK           = 0,
    DART_PENDING      = 1,
    DART_ERR_INVAL    = 2,
    DART_ERR_NOTFOUND = 3,
    DART_ERR_NOTINIT  = 4,
    DART_ERR_OTHER    = 999,
    /* add error codes as needed */
  } dart_ret_t;

typedef enum
  {
    DART_OP_UNDEFINED = 0,
    DART_OP_MIN,
    DART_OP_MAX,
    DART_OP_SUM,
    DART_OP_PROD,
    DART_OP_BAND,
    DART_OP_LAND,
    DART_OP_BOR,
    DART_OP_LOR,
    DART_OP_BXOR,
    DART_OP_LXOR
  } dart_operation_t;

typedef enum
  {
    DART_TYPE_UNDEFINED = 0,
    DART_TYPE_BYTE,
    DART_TYPE_SHORT,
    DART_TYPE_INT,
    DART_TYPE_UINT,
    DART_TYPE_LONG,
    DART_TYPE_ULONG,
    DART_TYPE_LONGLONG,
    DART_TYPE_FLOAT,
    DART_TYPE_DOUBLE
  } dart_datatype_t;

typedef int32_t dart_unit_t;
typedef int32_t dart_team_t;

#define DART_UNDEFINED_UNIT_ID ((dart_unit_t)(-1))

#define DART_INTERFACE_OFF

#ifdef __cplusplus
}
#endif

#endif /* DART_TYPES_H_INCLUDED */
