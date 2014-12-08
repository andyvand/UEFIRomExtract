#include "main.h"

VOID *
InternalMemSetMem16 (
                     VOID                      *Buffer,
                     UINT32                     Length,
                     UINT16                    Value
                     )
{
    do {
        ((UINT16*)Buffer)[--Length] = Value;
    } while (Length != 0);
    return Buffer;
}

VOID *
SetMem16 (
          VOID   *Buffer,
          UINT32   Length,
          UINT16  Value
          )
{
    if (Length == 0) {
        return Buffer;
    }
    
    ASSERT (Buffer != NULL);
    ASSERT ((Length - 1) <= (MAX_ADDRESS - (UINT32)Buffer));
    ASSERT ((((UINT32)Buffer) & (sizeof (Value) - 1)) == 0);
    ASSERT ((Length & (sizeof (Value) - 1)) == 0);
    
    return InternalMemSetMem16 (Buffer, Length / sizeof (Value), Value);
}
