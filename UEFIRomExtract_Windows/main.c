//
//  main.c
//  UEFIRomExtract
//
//  Created by Andy Vandijck on 18/07/14.
//  Copyright (c) 2014 AnV Software. All rights reserved.
//
			
#include "main.h"

UINT16 ReadUnaligned16 (CONST UINT16 *Buffer)
{
    ASSERT (Buffer != NULL);
    
    return (UINT16)(((UINT8*)Buffer)[0] | (((UINT8*)Buffer)[1] << 8));
}

UINT32 ReadUnaligned32 (CONST UINT32 *Buffer)
{
    UINT16  LowerBytes;
    UINT16  HigherBytes;
    
    ASSERT (Buffer != NULL);
    
    LowerBytes  = ReadUnaligned16 ((UINT16*) Buffer);
    HigherBytes = ReadUnaligned16 ((UINT16*) Buffer + 1);
    
    return (UINT32) (LowerBytes | (HigherBytes << 16));
}

/**
 Read NumOfBit of bits from source into mBitBuf.
 
 Shift mBitBuf NumOfBits left. Read NumOfBits of bits from source.
 
 @param  Sd        The global scratch data.
 @param  NumOfBits The number of bits to shift and read.
 
 **/
VOID
FillBuf (
          SCRATCH_DATA  *Sd,
          UINT16        NumOfBits
         )
{
    //
    // Left shift NumOfBits of bits advance
    //
    Sd->mBitBuf = (UINT32) (Sd->mBitBuf << NumOfBits);
    
    //
    // Copy data needed bytes into mSbuBitBuf
    //
    while (NumOfBits > Sd->mBitCount) {
        
        Sd->mBitBuf |= (UINT32) (Sd->mSubBitBuf << (NumOfBits = (UINT16) (NumOfBits - Sd->mBitCount)));
        
        if (Sd->mCompSize > 0) {
            //
            // Get 1 byte into SubBitBuf
            //
            Sd->mCompSize--;
            Sd->mSubBitBuf  = Sd->mSrcBase[Sd->mInBuf++];
            Sd->mBitCount   = 8;
            
        } else {
            //
            // No more bits from the source, just pad zero bit.
            //
            Sd->mSubBitBuf  = 0;
            Sd->mBitCount   = 8;
            
        }
    }
    
    //
    // Caculate additional bit count read to update mBitCount
    //
    Sd->mBitCount = (UINT16) (Sd->mBitCount - NumOfBits);
    
    //
    // Copy NumOfBits of bits from mSubBitBuf into mBitBuf
    //
    Sd->mBitBuf |= Sd->mSubBitBuf >> Sd->mBitCount;
}

/**
 Get NumOfBits of bits from mBitBuf.
 
 Get NumOfBits of bits from mBitBuf. Fill mBitBuf with subsequent
 NumOfBits of bits from source. Returns NumOfBits of bits that are
 popped out.
 
 @param  Sd        The global scratch data.
 @param  NumOfBits The number of bits to pop and read.
 
 @return The bits that are popped out.
 
 **/
static UINT32
GetBits (
          SCRATCH_DATA  *Sd,
          UINT16        NumOfBits
         )
{
    UINT32  OutBits;
    
    //
    // Pop NumOfBits of Bits from Left
    //
    OutBits = (UINT32) (Sd->mBitBuf >> (BITBUFSIZ - NumOfBits));
    
    //
    // Fill up mBitBuf from source
    //
    FillBuf (Sd, NumOfBits);
    
    return OutBits;
}

/**
 Decodes a position value.
 
 Get a position value according to Position Huffman Table.
 
 @param  Sd The global scratch data.
 
 @return The position value decoded.
 
 **/
UINT32
DecodeP (
          SCRATCH_DATA  *Sd
         )
{
    UINT16  Val;
    UINT32  Mask;
    UINT32  Pos;
    
    Val = Sd->mPTTable[Sd->mBitBuf >> (BITBUFSIZ - 8)];
    
    if (Val >= MAXNP) {
        Mask = 1U << (BITBUFSIZ - 1 - 8);
        
        do {
            
            if ((Sd->mBitBuf & Mask) != 0) {
                Val = Sd->mRight[Val];
            } else {
                Val = Sd->mLeft[Val];
            }
            
            Mask >>= 1;
        } while (Val >= MAXNP);
    }
    //
    // Advance what we have read
    //
    FillBuf (Sd, Sd->mPTLen[Val]);
    
    Pos = Val;
    if (Val > 1) {
        Pos = (UINT32) ((1U << (Val - 1)) + GetBits (Sd, (UINT16) (Val - 1)));
    }
    
    return Pos;
}

/**
 Reads code lengths for the Extra Set or the Position Set.
 
 Read the Extra Set or Pointion Set Length Arrary, then
 generate the Huffman code mapping for them.
 
 @param  Sd      The global scratch data.
 @param  nn      The number of symbols.
 @param  nbit    The number of bits needed to represent nn.
 @param  Special The special symbol that needs to be taken care of.
 
 @retval  0 OK.
 @retval  BAD_TABLE Table is corrupted.
 
 **/
UINT16
ReadPTLen (
            SCRATCH_DATA  *Sd,
            UINT16        nn,
            UINT16        nbit,
            UINT16        Special
           )
{
    UINT16  Number;
    UINT16  CharC;
    UINT16  Index;
    UINT32  Mask;
    
    //
    // Read Extra Set Code Length Array size
    //
    Number = (UINT16) GetBits (Sd, nbit);
    
    if (Number == 0) {
        //
        // This represents only Huffman code used
        //
        CharC = (UINT16) GetBits (Sd, nbit);
        
        SetMem16 (&Sd->mPTTable[0] , sizeof (Sd->mPTTable), CharC);
        
        memset (Sd->mPTLen, 0, nn);
        
        return 0;
    }
    
    Index = 0;
    
    while (Index < Number && Index < NPT) {
        
        CharC = (UINT16) (Sd->mBitBuf >> (BITBUFSIZ - 3));
        
        //
        // If a code length is less than 7, then it is encoded as a 3-bit
        // value. Or it is encoded as a series of "1"s followed by a
        // terminating "0". The number of "1"s = Code length - 4.
        //
        if (CharC == 7) {
            Mask = 1U << (BITBUFSIZ - 1 - 3);
            while (Mask & Sd->mBitBuf) {
                Mask >>= 1;
                CharC += 1;
            }
        }
        
        FillBuf (Sd, (UINT16) ((CharC < 7) ? 3 : CharC - 3));
        
        Sd->mPTLen[Index++] = (UINT8) CharC;
        
        //
        // For Code&Len Set,
        // After the third length of the code length concatenation,
        // a 2-bit value is used to indicated the number of consecutive
        // zero lengths after the third length.
        //
        if (Index == Special) {
            CharC = (UINT16) GetBits (Sd, 2);
            while ((INT16) (--CharC) >= 0 && Index < NPT) {
                Sd->mPTLen[Index++] = 0;
            }
        }
    }
    
    while (Index < nn && Index < NPT) {
        Sd->mPTLen[Index++] = 0;
    }
    
    return MakeTable (Sd, nn, Sd->mPTLen, 8, Sd->mPTTable);
}

/**
 Reads code lengths for Char&Len Set.
 
 Read and decode the Char&Len Set Code Length Array, then
 generate the Huffman Code mapping table for the Char&Len Set.
 
 @param  Sd The global scratch data.
 
 **/
VOID
ReadCLen (
          SCRATCH_DATA  *Sd
          )
{
    UINT16           Number;
    UINT16           CharC;
    UINT16           Index;
    UINT32           Mask;
    
    Number = (UINT16) GetBits (Sd, CBIT);
    
    if (Number == 0) {
        //
        // This represents only Huffman code used
        //
        CharC = (UINT16) GetBits (Sd, CBIT);
        
        memset (Sd->mCLen, 0, NC);
        SetMem16 (&Sd->mCTable[0], sizeof (Sd->mCTable) - 1, CharC);
        
        return ;
    }
    
    Index = 0;
    while (Index < Number && Index < NC) {
        CharC = Sd->mPTTable[Sd->mBitBuf >> (BITBUFSIZ - 8)];
        if (CharC >= NT) {
            Mask = 1U << (BITBUFSIZ - 1 - 8);
            
            do {
                
                if (Mask & Sd->mBitBuf) {
                    CharC = Sd->mRight[CharC];
                } else {
                    CharC = Sd->mLeft[CharC];
                }
                
                Mask >>= 1;
                
            } while (CharC >= NT);
        }
        //
        // Advance what we have read
        //
        FillBuf (Sd, Sd->mPTLen[CharC]);
        
        if (CharC <= 2) {
            
            if (CharC == 0) {
                CharC = 1;
            } else if (CharC == 1) {
                CharC = (UINT16) (GetBits (Sd, 4) + 3);
            } else if (CharC == 2) {
                CharC = (UINT16) (GetBits (Sd, CBIT) + 20);
            }
            
            while ((INT16) (--CharC) >= 0 && Index < NC) {
                Sd->mCLen[Index++] = 0;
            }
            
        } else {
            
            Sd->mCLen[Index++] = (UINT8) (CharC - 2);
            
        }
    }
    
    memset (Sd->mCLen + Index, 0, NC - Index);
    
    MakeTable (Sd, NC, Sd->mCLen, 12, Sd->mCTable);
    
    return ;
}

/**
 Decode a character/length value.
 
 Read one value from mBitBuf, Get one code from mBitBuf. If it is at block boundary, generates
 Huffman code mapping table for Extra Set, Code&Len Set and
 Position Set.
 
 @param  Sd The global scratch data.
 
 @return The value decoded.
 
 **/
UINT16
DecodeC (
         SCRATCH_DATA  *Sd
         )
{
    UINT16  Index2;
    UINT32  Mask;
    
    if (Sd->mBlockSize == 0) {
        //
        // Starting a new block
        // Read BlockSize from block header
        //
        Sd->mBlockSize    = (UINT16) GetBits (Sd, 16);
        
        //
        // Read the Extra Set Code Length Arrary,
        // Generate the Huffman code mapping table for Extra Set.
        //
        Sd->mBadTableFlag = ReadPTLen (Sd, NT, TBIT, 3);
        if (Sd->mBadTableFlag != 0) {
            return 0;
        }
        
        //
        // Read and decode the Char&Len Set Code Length Arrary,
        // Generate the Huffman code mapping table for Char&Len Set.
        //
        ReadCLen (Sd);
        
        //
        // Read the Position Set Code Length Arrary,
        // Generate the Huffman code mapping table for the Position Set.
        //
        Sd->mBadTableFlag = ReadPTLen (Sd, MAXNP, Sd->mPBit, (UINT16) (-1));
        if (Sd->mBadTableFlag != 0) {
            return 0;
        }
    }
    
    //
    // Get one code according to Code&Set Huffman Table
    //
    Sd->mBlockSize--;
    Index2 = Sd->mCTable[Sd->mBitBuf >> (BITBUFSIZ - 12)];
    
    if (Index2 >= NC) {
        Mask = 1U << (BITBUFSIZ - 1 - 12);
        
        do {
            if ((Sd->mBitBuf & Mask) != 0) {
                Index2 = Sd->mRight[Index2];
            } else {
                Index2 = Sd->mLeft[Index2];
            }
            
            Mask >>= 1;
        } while (Index2 >= NC);
    }
    //
    // Advance what we have read
    //
    FillBuf (Sd, Sd->mCLen[Index2]);
    
    return Index2;
}

/**
 Decode the source data and put the resulting data into the destination buffer.
 
 @param  Sd The global scratch data.
 
 **/
VOID
Decode (
        SCRATCH_DATA  *Sd
        )
{
    UINT16  BytesRemain;
    UINT32  DataIdx;
    UINT16  CharC;
    
    BytesRemain = (UINT16) (-1);
    
    DataIdx     = 0;
    
    for (;;) {
        //
        // Get one code from mBitBuf
        //
        CharC = DecodeC (Sd);
        if (Sd->mBadTableFlag != 0) {
            goto Done;
        }
        
        if (CharC < 256) {
            //
            // Process an Original character
            //
            if (Sd->mOutBuf >= Sd->mOrigSize) {
                goto Done;
            } else {
                //
                // Write orignal character into mDstBase
                //
                Sd->mDstBase[Sd->mOutBuf++] = (UINT8) CharC;
            }
            
        } else {
            //
            // Process a Pointer
            //
            CharC       = (UINT16) (CharC - (BIT8 - THRESHOLD));
            
            //
            // Get string length
            //
            BytesRemain = CharC;
            
            //
            // Locate string position
            //
            DataIdx     = Sd->mOutBuf - DecodeP (Sd) - 1;
            
            //
            // Write BytesRemaof bytes into mDstBase
            //
            BytesRemain--;
            while ((INT16) (BytesRemain) >= 0) {
                Sd->mDstBase[Sd->mOutBuf++] = Sd->mDstBase[DataIdx++];
                if (Sd->mOutBuf >= Sd->mOrigSize) {
                    goto Done;
                }
                
                BytesRemain--;
            }
        }
    }
    
Done:
    return ;
}

/**
 Given a compressed source buffer, this function retrieves the size of
 the uncompressed buffer and the size of the scratch buffer required
 to decompress the compressed source buffer.
 
 Retrieves the size of the uncompressed buffer and the temporary scratch buffer
 required to decompress the buffer specified by Source and SourceSize.
 If the size of the uncompressed buffer or the size of the scratch buffer cannot
 be determined from the compressed data specified by Source and SourceData,
 then RETURN_INVALID_PARAMETER is returned.  Otherwise, the size of the uncompressed
 buffer is returned DestinationSize, the size of the scratch buffer is returned
 ScratchSize, and RETURN_SUCCESS is returned.
 This function does not have scratch buffer available to perform a thorough
 checking of the validity of the source data.  It just retrieves the "Original Size"
 field from the beginning bytes of the source data and output it as DestinationSize.
 And ScratchSize is specific to the decompression implementation.
 
 If Source is NULL, then ASSERT().
 If DestinationSize is NULL, then ASSERT().
 If ScratchSize is NULL, then ASSERT().
 
 @param  Source          The source buffer containing the compressed data.
 @param  SourceSize      The size, bytes, of the source buffer.
 @param  DestinationSize A pointer to the size, bytes, of the uncompressed buffer
 that will be generated when the compressed buffer specified
 by Source and SourceSize is decompressed.
 @param  ScratchSize     A pointer to the size, bytes, of the scratch buffer that
 is required to decompress the compressed buffer specified
 by Source and SourceSize.
 
 @retval  RETURN_SUCCESS The size of the uncompressed data was returned
 DestinationSize, and the size of the scratch
 buffer was returned ScratchSize.
 @retval  RETURN_INVALID_PARAMETER
 The size of the uncompressed data or the size of
 the scratch buffer cannot be determined from
 the compressed data specified by Source
 and SourceSize.
 **/
RETURN_STATUS
UefiDecompressGetInfo (
                        CONST VOID  *Source,
                        UINT32      SourceSize,
                       UINT32      *DestinationSize,
                       UINT32      *ScratchSize
                       )
{
    UINT32  CompressedSize;
    
    ASSERT (Source != NULL);
    ASSERT (DestinationSize != NULL);
    ASSERT (ScratchSize != NULL);
    
    if (SourceSize < 8) {
        return RETURN_INVALID_PARAMETER;
    }
    
    CompressedSize   = ReadUnaligned32 ((UINT32 *)Source);
    if (SourceSize < (CompressedSize + 8)) {
        return RETURN_INVALID_PARAMETER;
    }
    
    *ScratchSize  = sizeof (SCRATCH_DATA);
    *DestinationSize = ReadUnaligned32 ((UINT32 *)Source + 1);
    
    return RETURN_SUCCESS;
}

/**
 Decompresses a compressed source buffer.
 
 Extracts decompressed data to its original form.
 This function is designed so that the decompression algorithm can be implemented
 withusing any memory services.  As a result, this function is not allowed to
 call any memory allocation services its implementation.  It is the caller's
 responsibility to allocate and free the Destination and Scratch buffers.
 If the compressed source data specified by Source is successfully decompressed
 into Destination, then RETURN_SUCCESS is returned.  If the compressed source data
 specified by Source is not a valid compressed data format,
 then RETURN_INVALID_PARAMETER is returned.
 
 If Source is NULL, then ASSERT().
 If Destination is NULL, then ASSERT().
 If the required scratch buffer size > 0 and Scratch is NULL, then ASSERT().
 
 @param  Source      The source buffer containing the compressed data.
 @param  Destination The destination buffer to store the decompressed data.
 @param  Scratch     A temporary scratch buffer that is used to perform the decompression.
 This is an optional parameter that may be NULL if the 
 required scratch buffer size is 0.
 
 @retval  RETURN_SUCCESS Decompression completed successfully, and 
 the uncompressed buffer is returned Destination.
 @retval  RETURN_INVALID_PARAMETER 
 The source buffer specified by Source is corrupted 
 (not a valid compressed format).
 **/
RETURN_STATUS
UefiDecompress (
                CONST VOID  *Source,
                VOID    *Destination,
                VOID    *Scratch
                )
{
    UINT32           CompSize;
    UINT32           OrigSize;
    SCRATCH_DATA     *Sd;
    CONST UINT8      *Src;
    UINT8            *Dst;
    
    ASSERT (Source != NULL);
    ASSERT (Destination != NULL);
    ASSERT (Scratch != NULL);
    
    Src     = (CONST UINT8 *)Source;
    Dst     = (UINT8 *)Destination;
    
    Sd = (SCRATCH_DATA *) Scratch;
    
    CompSize  = Src[0] + (Src[1] << 8) + (Src[2] << 16) + (Src[3] << 24);
    OrigSize  = Src[4] + (Src[5] << 8) + (Src[6] << 16) + (Src[7] << 24);
    
    //
    // If compressed file size is 0, return
    //
    if (OrigSize == 0) {
        return RETURN_SUCCESS;
    }
    
    Src = Src + 8;
    memset (Sd, 0, sizeof (SCRATCH_DATA));
    
    //
    // The length of the field 'Position Set Code Length Array Size' Block Header.
    // For UEFI 2.0 de/compression algorithm(Version 1), mPBit = 4
    //
    Sd->mPBit     = 4;
    Sd->mSrcBase  = (UINT8 *)Src;
    Sd->mDstBase  = Dst;
    //
    // CompSize and OrigSize are caculated bytes
    //
    Sd->mCompSize = CompSize;
    Sd->mOrigSize = OrigSize;
    
    //
    // Fill the first BITBUFSIZ bits
    //
    FillBuf (Sd, BITBUFSIZ);
    
    //
    // Decompress it
    //
    Decode (Sd);
    
    if (Sd->mBadTableFlag != 0) {
        //
        // Something wrong with the source
        //
        return RETURN_INVALID_PARAMETER;
    }
    
    return RETURN_SUCCESS;
}

VOID Usage(const char *appname)
{
    printf("UEFI option ROM extractor and decompressor V1.0\n");
    printf("This program extracts and decompresses UEFI .rom files in their .efi files\n");
    printf("Usage: %s <In_File> <Out_File>\n\n", appname);
    printf("Copyright (C) 2014 - AnV Software, all rights reserved\n");
}

UINT8 GetEfiCompressedROM(const char *InFile, UINT8 Pci23, UINT32 *EFIIMGStart)
{
    PCI_EXPANSION_ROM_HEADER      PciRomHdr;
    FILE                          *InFptr;
    UINT32                        ImageStart;
    UINT32                        ImageCount;
    EFI_PCI_EXPANSION_ROM_HEADER  EfiRomHdr;
    PCI_DATA_STRUCTURE            PciDs23;
    PCI_3_0_DATA_STRUCTURE        PciDs30;
    
    //
    // Open the input file
    //
#ifdef _MSC_VER
	if (fopen_s (&InFptr, InFile, "rb")) {
#else
    if ((InFptr = fopen (InFile, "rb")) == NULL) {
#endif
        printf("Error opening file %s!\n", InFile);

        return 0;
    }
    //
    // Go through the image and dump the header stuff for each
    //
    ImageCount = 0;
    for (;;) {
        //
        // Save our postition in the file, since offsets in the headers
        // are relative to the particular image.
        //
        ImageStart = (UINT32)ftell (InFptr);
        ImageCount++;
        
        //
        // Read the option ROM header. Have to assume a raw binary image for now.
        //
#ifdef _MSC_VER
        if (fread_s (&PciRomHdr, sizeof(PCI_EXPANSION_ROM_HEADER), sizeof (PciRomHdr), 1, InFptr) != 1) {
#else
        if (fread (&PciRomHdr, sizeof (PciRomHdr), 1, InFptr) != 1) {
#endif
			printf("Failed to read PCI ROM header from file!\n");
            goto BailOut;
        }
        
        //
        // Find PCI data structure
        //
        if (fseek (InFptr, ImageStart + PciRomHdr.PcirOffset, SEEK_SET)) {
            printf("Failed to seek to PCI data structure!\n");
            goto BailOut;
        }
        //
        // Read and dump the PCI data structure
        //
        memset (&PciDs23, 0, sizeof (PciDs23));
        memset (&PciDs30, 0, sizeof (PciDs30));
        if (Pci23 == 1) {
#ifdef _MSC_VER
            if (fread_s (&PciDs23, sizeof(PCI_DATA_STRUCTURE), sizeof (PciDs23), 1, InFptr) != 1) {
#else
            if (fread (&PciDs23, sizeof (PciDs23), 1, InFptr) != 1) {
#endif
				printf("Failed to read PCI data structure from file %s!\n", InFile);
                goto BailOut;
            }
        } else {
#ifdef _MSC_VER
            if (fread_s (&PciDs30, sizeof(PCI_3_0_DATA_STRUCTURE), sizeof (PciDs30), 1, InFptr) != 1) {
#else
            if (fread (&PciDs30, sizeof (PciDs30), 1, InFptr) != 1) {
#endif
				printf("Failed to read PCI data structure from file %s!\n", InFile);
                goto BailOut;
            }
        }
        if ((PciDs23.CodeType == PCI_CODE_TYPE_EFI_IMAGE) || (PciDs30.CodeType == PCI_CODE_TYPE_EFI_IMAGE))
        {
            //
            // Re-read the header as an EFI ROM header, then dump more info
            //
            if (fseek (InFptr, ImageStart, SEEK_SET)) {
                printf("Failed to re-seek to ROM header structure!\n");
                goto BailOut;
            }

#ifdef _MSC_VER
            if (fread_s (&EfiRomHdr, sizeof(EFI_PCI_EXPANSION_ROM_HEADER), sizeof (PciDs23), 1, InFptr) != 1) {
#else
            if (fread (&EfiRomHdr, sizeof (EfiRomHdr), 1, InFptr) != 1) {
#endif
				printf("Failed to read EFI PCI ROM header from file!\n");
                goto BailOut;
            }
            //

            if (EfiRomHdr.CompressionType == EFI_PCI_EXPANSION_ROM_HEADER_COMPRESSED) {
                EFIIMGStart[0] = EfiRomHdr.EfiImageHeaderOffset + (unsigned) ImageStart;

                printf("Found compressed EFI ROM start at 0x%x\n", EFIIMGStart[0]);

                fclose(InFptr);

                return 1;
            } else {
                fclose(InFptr);

                printf("Found non-compressed EFI ROM start at 0x%x, exiting...\n", EfiRomHdr.EfiImageHeaderOffset+(unsigned)ImageStart);

                exit(-1);
            }
        }
        //
        // If code type is EFI image, then dump it as well?
        //
        // if (PciDs.CodeType == PCI_CODE_TYPE_EFI_IMAGE) {
        // }
        //
        // If last image, then we're done
        //
        if ((PciDs23.Indicator == INDICATOR_LAST) || (PciDs30.Indicator == INDICATOR_LAST)) {
            goto BailOut;
        }
        //
        // Seek to the start of the next image
        //
        if (Pci23 == 1) {
            if (fseek (InFptr, ImageStart + (PciDs23.ImageLength * 512), SEEK_SET)) {
                printf("Failed to seek to next image!\n");
                goto BailOut;
            }
        } else {
            if (fseek (InFptr, ImageStart + (PciDs30.ImageLength * 512), SEEK_SET)) {
                printf("Failed to seek to next image!\n");
                goto BailOut;
            }
        }
    }
    
BailOut:
    printf("No compressed EFI ROM found!\n");

    fclose (InFptr);

    EFIIMGStart[0] = 0;

    return 0;
}

int main(int argc, const char * argv[])
{
    VOID                *Buffer;
    VOID                *OutBuffer;
    VOID                *ROMBuffer;
    VOID                *ScratchBuffer;
    long                fInSize = 0;
    long                fROMStart = 0;
    long                fOutSize = 0;
    long                ScratchSize = 0;
    FILE                *fIn;
    FILE                *fOut;

    if (argc != 3)
    {
        Usage(argv[0]);

        return 1;
    }

#ifdef _MSC_VER
	fopen_s(&fIn, argv[1], "rb");
#else
    fIn = fopen(argv[1], "rb");
#endif

    fseek(fIn, 0, SEEK_END);
    fInSize = ftell(fIn);
    fseek(fIn, 0, SEEK_SET);

    Buffer = malloc(fInSize);

    if (Buffer == NULL)
    {
        printf("Input buffer allocation failed!\n");

        return -1;
    }

#ifdef _MSC_VER
    fread_s(Buffer, fInSize, fInSize, 1, fIn);
#else
    fread(Buffer, fInSize, 1, fIn);
#endif

	fclose(fIn);

    if (!GetEfiCompressedROM(argv[1], 0, (UINT32 *)&fROMStart))
    {
        if (!GetEfiCompressedROM(argv[1], 1, (UINT32 *)&fROMStart))
        {
            printf("Not an EFI ROM file, attempting decompression of data directly...\n");
        }
    }

    if (fROMStart > 0)
    {
        ROMBuffer = malloc(fInSize - fROMStart);

        if (ROMBuffer == NULL)
        {
            printf("Could not allocation new ROM buffer!\n");

            free(Buffer);

            return -2;
        }

        memcpy(ROMBuffer, (UINT8 *)Buffer+fROMStart, fInSize - fROMStart);

        free(Buffer);

        Buffer = ROMBuffer;
        fInSize -= fROMStart;
    }

    if (UefiDecompressGetInfo(Buffer, (UINT32)fInSize, (UINT32 *)&fOutSize, (UINT32 *)&ScratchSize))
    {
        printf("get UEFI decompression info failed!\n");

        free(Buffer);

        return -3;
    }

    printf("Input size: %lu, Output size: %lu, Scratch size: %lu\n", fInSize, fOutSize, ScratchSize);

    if (fOutSize <= 0)
    {
        printf("Incorrect output size!\n");

        free(Buffer);

        return -4;
    }

    if (ScratchSize <= 0)
    {
        printf("Incorrect scratch buffer size!\n");

        free(Buffer);

        return -5;
    }

    ScratchBuffer = malloc(ScratchSize);

    if (ScratchBuffer == NULL)
    {
        printf("Scratch buffer allocation failed!\n");

        free(Buffer);

        return -6;
    }

    OutBuffer = malloc(fOutSize);

    if (OutBuffer == NULL)
    {
        printf("Output buffer buffer allocation failed!\n");

        free(Buffer);
        free(ScratchBuffer);

        return -7;
    }

    if (UefiDecompress(Buffer, OutBuffer, ScratchBuffer))
    {
        printf ("UEFI decompression failed!\n");
        
        free(Buffer);
        free(OutBuffer);
        free(ScratchBuffer);

        return -8;
    }

#ifdef _MSC_VER
	fopen_s(&fOut, argv[2], "wb");
#else
    fOut = fopen(argv[2], "wb");
#endif

    fwrite(OutBuffer, fOutSize, 1, fOut);
    fclose(fOut);

    free(Buffer);
    free(OutBuffer);
    free(ScratchBuffer);

    return 0;
}
