#include "main.h"

/**
 Creates Huffman Code mapping table according to code length array.
 
 Creates Huffman Code mapping table for Extra Set, Char&Len Set
 and Position Set according to code length array.
 If TableBits > 16, then ASSERT ().
 
 @param  Sd        The global scratch data.
 @param  NumOfChar The number of symbols the symbol set.
 @param  BitLen    Code length array.
 @param  TableBits The width of the mapping table.
 @param  Table     The table to be created.
 
 @retval  0 OK.
 @retval  BAD_TABLE The table is corrupted.
 
 **/
UINT16
MakeTable (
            SCRATCH_DATA  *Sd,
            UINT16        NumOfChar,
            UINT8         *BitLen,
            UINT16        TableBits,
           UINT16        *Table
           )
{
    UINT16  Count[17];
    UINT16  Weight[17];
    UINT16  Start[18];
    UINT16  *Pointer;
    UINT16  Index3;
    UINT16  Index;
    UINT16  Len;
    UINT16  Char;
    UINT16  JuBits;
    UINT16  Avail;
    UINT16  NextCode;
    UINT16  Mask;
    UINT16  WordOfStart;
    UINT16  WordOfCount;
    
    //
    // The maximum mapping table width supported by this internal
    // working function is 16.
    //
    ASSERT (TableBits <= 16);
    
    for (Index = 0; Index <= 16; Index++) {
        Count[Index] = 0;
    }
    
    for (Index = 0; Index < NumOfChar; Index++) {
        Count[BitLen[Index]]++;
    }
    
    Start[0] = 0;
    Start[1] = 0;
    
    for (Index = 1; Index <= 16; Index++) {
        WordOfStart = Start[Index];
        WordOfCount = Count[Index];
        Start[Index + 1] = (UINT16) (WordOfStart + (WordOfCount << (16 - Index)));
    }
    
    if (Start[17] != 0) {
        /*(1U << 16)*/
        return (UINT16) BAD_TABLE;
    }
    
    JuBits = (UINT16) (16 - TableBits);
    
    Weight[0] = 0;
    for (Index = 1; Index <= TableBits; Index++) {
        Start[Index] >>= JuBits;
        Weight[Index] = (UINT16) (1U << (TableBits - Index));
    }
    
    while (Index <= 16) {
        Weight[Index] = (UINT16) (1U << (16 - Index));
        Index++;
    }
    
    Index = (UINT16) (Start[TableBits + 1] >> JuBits);
    
    if (Index != 0) {
        Index3 = (UINT16) (1U << TableBits);
        if (Index < Index3) {
            SetMem16 (Table + Index, (Index3 - Index) * sizeof (*Table), 0);
        }
    }
    
    Avail = NumOfChar;
    Mask  = (UINT16) (1U << (15 - TableBits));
    
    for (Char = 0; Char < NumOfChar; Char++) {
        
        Len = BitLen[Char];
        if (Len == 0 || Len >= 17) {
            continue;
        }
        
        NextCode = (UINT16) (Start[Len] + Weight[Len]);
        
        if (Len <= TableBits) {
            
            for (Index = Start[Len]; Index < NextCode; Index++) {
                Table[Index] = Char;
            }
            
        } else {
            
            Index3  = Start[Len];
            Pointer = &Table[Index3 >> JuBits];
            Index   = (UINT16) (Len - TableBits);
            
            while (Index != 0) {
                if (*Pointer == 0 && Avail < (2 * NC - 1)) {
                    Sd->mRight[Avail] = Sd->mLeft[Avail] = 0;
                    *Pointer = Avail++;
                }
                
                if (*Pointer < (2 * NC - 1)) {
                    if ((Index3 & Mask) != 0) {
                        Pointer = &Sd->mRight[*Pointer];
                    } else {
                        Pointer = &Sd->mLeft[*Pointer];
                    }
                }
                
                Index3 <<= 1;
                Index--;
            }
            
            *Pointer = Char;
            
        }
        
        Start[Len] = NextCode;
    }
    //
    // Succeeds
    //
    return 0;
}
