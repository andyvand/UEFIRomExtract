//
//  ma.h
//  UEFIRomExtract
//
//  Created by Andy Vandijck on 18/07/14.
//  Copyright (c) 2014 AnV Software. All rights reserved.
//

#ifndef UEFIRomExtract_ma_h
#define UEFIRomExtract_ma_h

#ifdef _MSC_VER
#define _M_CEE_SAFE
#include <CodeAnalysis/sourceannotations.h>
#undef _M_CEE_SAFE

#include "targetver.h"

#pragma pack(push, 1)
typedef struct _iobuf {
        char *_ptr;
        int   _cnt;
        char *_base;
        int   _flag;
        int   _file;
        int   _charbuf;
        int   _bufsiz;
        char *_tmpfname;
} FILE;
#pragma pack(pop)

#define _FILE_DEFINED
#endif

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#ifdef __x86_64__
#define MAX_ADDRESS   0xFFFFFFFFFFFFFFFFULL
#else
#define MAX_ADDRESS   0xFFFFFFFF
#endif

#define VOID void
#define UINT8 unsigned char
#define UINT16 unsigned short
#define UINT32 unsigned long
#define UINT64 unsigned __int64_t
#define INT8 char
#define INT16 short
#define INT32 long
#define INT64 __int64_t
#define ASSERT assert
#define CONST const

typedef UINT32 RETURN_STATUS;

#define RETURN_SUCCESS 0
#define RETURN_INVALID_PARAMETER 2

#define  BIT8     0x00000100

//
// Decompression algorithm begs here
//
#define BITBUFSIZ 32
#define MAXMATCH  256
#define THRESHOLD 3
#define CODE_BIT  16
#define BAD_TABLE - 1

//
// C: Char&Len Set; P: Position Set; T: exTra Set
//
#define NC      (0xff + MAXMATCH + 2 - THRESHOLD)
#define CBIT    9
#define MAXPBIT 5
#define TBIT    5
#define MAXNP   ((1U << MAXPBIT) - 1)
#define NT      (CODE_BIT + 3)
#if NT > MAXNP
#define NPT NT
#else
#define NPT MAXNP
#endif

#pragma pack(push, 1)
typedef struct {
    UINT8   *mSrcBase;  // The starting address of compressed data
    UINT8   *mDstBase;  // The starting address of decompressed data
    UINT32  mOutBuf;
    UINT32  mInBuf;
    UINT16  mBitCount;
    UINT32  mBitBuf;
    UINT32  mSubBitBuf;
    UINT16  mBlockSize;
    UINT32  mCompSize;
    UINT32  mOrigSize;
    UINT16  mBadTableFlag;
    UINT16  mLeft[2 * NC - 1];
    UINT16  mRight[2 * NC - 1];
    UINT8   mCLen[NC];
    UINT8   mPTLen[NPT];
    UINT16  mCTable[4096];
    UINT16  mPTTable[256];
    UINT8   mPBit;
} SCRATCH_DATA;

typedef struct {
    UINT16  Signature;    // 0xaa55
    UINT8   Reserved[0x16];
    UINT16  PcirOffset;
} PCI_EXPANSION_ROM_HEADER;

typedef struct {
    UINT16  Signature;    // 0xaa55
    UINT16  InitializationSize;
    UINT32  EfiSignature; // 0x0EF1
    UINT16  EfiSubsystem;
    UINT16  EfiMachineType;
    UINT16  CompressionType;
    UINT8   Reserved[8];
    UINT16  EfiImageHeaderOffset;
    UINT16  PcirOffset;
} EFI_PCI_EXPANSION_ROM_HEADER;

typedef struct {
    UINT32  Signature;    ///< "PCIR"
    UINT16  VendorId;
    UINT16  DeviceId;
    UINT16  Reserved0;
    UINT16  Length;
    UINT8   Revision;
    UINT8   ClassCode[3];
    UINT16  ImageLength;
    UINT16  CodeRevision;
    UINT8   CodeType;
    UINT8   Indicator;
    UINT16  Reserved1;
} PCI_DATA_STRUCTURE;

typedef struct {
    UINT32  Signature;    // "PCIR"
    UINT16  VendorId;
    UINT16  DeviceId;
    UINT16  DeviceListOffset;
    UINT16  Length;
    UINT8   Revision;
    UINT8   ClassCode[3];
    UINT16  ImageLength;
    UINT16  CodeRevision;
    UINT8   CodeType;
    UINT8   Indicator;
    UINT16  MaxRuntimeImageLength;
    UINT16  ConfigUtilityCodeHeaderOffset;
    UINT16  DMTFCLPEntryPointOffset;
} PCI_3_0_DATA_STRUCTURE;
#pragma pack(pop)

#define PCI_CODE_TYPE_EFI_IMAGE 0x03
#define EFI_PCI_EXPANSION_ROM_HEADER_COMPRESSED 0x0001
#define INDICATOR_LAST  0x80

extern VOID Usage(const char *appname);

extern UINT8 GetEfiCompressedROM(const char *InFile, UINT8 Pci23, UINT32 *EFIIMGStart);

extern VOID *
InternalMemSetMem16 (
                     VOID                      *Buffer,
                     UINT32                     Length,
                     UINT16                    Value
                     );

extern VOID *
SetMem16 (
          VOID   *Buffer,
          UINT32   Length,
          UINT16  Value
          );

extern UINT16 ReadUnaligned16 (CONST UINT16 *Buffer);
extern UINT32 ReadUnaligned32 (CONST UINT32 *Buffer);

/**
 Read NumOfBit of bits from source to mBitBuf.
 
 Shift mBitBuf NumOfBits left. Read  NumOfBits of bits from source.
 
 @param  Sd        The global scratch data.
 @param  NumOfBits The number of bits to shift and read.
 
 **/
extern VOID
FillBuf (
           SCRATCH_DATA  *Sd,
           UINT16        NumOfBits
         );

/**
 Get NumOfBits of bits  from mBitBuf.
 
 Get NumOfBits of bits  from mBitBuf. Fill mBitBuf with subsequent
 NumOfBits of bits from source. Returns NumOfBits of bits that are
 popped .
 
 @param  Sd        The global scratch data.
 @param  NumOfBits The number of bits to pop and read.
 
 @return The bits that are popped .
 
 **/
/*extern UINT32
GetBits (
           SCRATCH_DATA  *Sd,
           UINT16        NumOfBits
         );*/

/**
 Creates Huffman Code mappg table accordg to code length array.
 
 Creates Huffman Code mappg table for Extra Set, Char&Len Set
 and Position Set accordg to code length array.
 If TableBits > 16, then ASSERT ().
 
 @param  Sd        The global scratch data.
 @param  NumOfChar The number of symbols  the symbol set.
 @param  BitLen    Code length array.
 @param  TableBits The width of the mappg table.
 @param  Table     The table to be created.
 
 @retval  0 OK.
 @retval  BAD_TABLE The table is corrupted.
 
 **/
extern UINT16
MakeTable (
             SCRATCH_DATA  *Sd,
             UINT16        NumOfChar,
             UINT8         *BitLen,
             UINT16        TableBits,
            UINT16        *Table
           );

/**
 Decodes a position value.
 
 Get a position value accordg to Position Huffman Table.
 
 @param  Sd The global scratch data.
 
 @return The position value decoded.
 
 **/
extern UINT32
DecodeP (
           SCRATCH_DATA  *Sd
         );

/**
 Reads code lengths for the Extra Set or the Position Set.
 
 Read  the Extra Set or Potion Set Length Arrary, then
 generate the Huffman code mappg for them.
 
 @param  Sd      The global scratch data.
 @param  nn      The number of symbols.
 @param  nbit    The number of bits needed to represent nn.
 @param  Special The special symbol that needs to be taken care of.
 
 @retval  0 OK.
 @retval  BAD_TABLE Table is corrupted.
 
 **/
extern UINT16
ReadPTLen (
             SCRATCH_DATA  *Sd,
             UINT16        nn,
             UINT16        nbit,
             UINT16        Special
           );

/**
 Reads code lengths for Char&Len Set.
 
 Read  and decode the Char&Len Set Code Length Array, then
 generate the Huffman Code mappg table for the Char&Len Set.
 
 @param  Sd The global scratch data.
 
 **/
extern VOID
ReadCLen (
          SCRATCH_DATA  *Sd
          );

/**
 Decode a character/length value.
 
 Read one value from mBitBuf, Get one code from mBitBuf. If it is at block boundary, generates
 Huffman code mappg table for Extra Set, Code&Len Set and
 Position Set.
 
 @param  Sd The global scratch data.
 
 @return The value decoded.
 
 **/
extern UINT16
DecodeC (
         SCRATCH_DATA  *Sd
         );

/**
 Decode the source data and put the resultg data to the destation buffer.
 
 @param  Sd The global scratch data.
 
 **/
extern VOID
Decode (
        SCRATCH_DATA  *Sd
        );

#endif
