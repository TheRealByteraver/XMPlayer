/*
    Implementation of itsex.h, see header file for details
*/

#include "itsex.h"

// gets block of compressed data from file 
int ItSex::readblock( VirtualFile& file )
{    
    unsigned short size;
    file.read( &size,sizeof( unsigned short ) );
    if ( !size ) 
        return 0;
    //sourcebuffer = new unsigned char[size];
    sourcebuffer = std::make_unique < unsigned char[] >( size );
    if ( file.read( sourcebuffer.get(),size ) ) {
        //delete[] sourcebuffer;
        //sourcebuffer = nullptr;
        return 0;
    }
    ibuf = sourcebuffer.get();
    bitnum = 8;
    bitlen = size;
    return 1;
}

// frees that block again
int ItSex::freeblock() 
{
    //delete[] sourcebuffer;
    //sourcebuffer = nullptr;
    return 1;
}

// The Intel byte ordering is quite counter-intuitive:
unsigned ItSex::readbits( unsigned char n )
{
    unsigned retval = 0;
    int offset = 0;
    while ( n ) {
        int m = n;
        if ( !bitlen ) 
            return 0;
        if ( m > bitnum )
            m = bitnum;
        retval |= (*ibuf & ((1L << m) - 1)) << offset;
        *ibuf >>= m;
        n -= m;
        offset += m;
        if ( !(bitnum -= m) ) {
            bitlen--;
            ibuf++;
            bitnum = 8;
        }
    }
    return retval;
}

// ----------------------------------------------------------------------
//  decompression routines
// ----------------------------------------------------------------------
//
// decompresses 8-bit sample (params : file, outbuffer, lenght of
//                                     uncompressed sample, IT2.15
//                                     compression flag
//                            returns: status                     )    
int ItSex::decompress8( VirtualFile& module,void *dst,int len )
{
    char*           destbuf;    // the destination buffer which will be returned 
    unsigned short  blklen;     // length of compressed data block in samples 
    unsigned short  blkpos;		// position in block 
    unsigned char   width;		// actual "bit width" 
    unsigned short  value;		// value read from file to be processed 
    char            d1,d2;		// integrator buffers (d2 for it2.15) 
    char*           destpos;

    destbuf = (char *)dst;
    if ( !destbuf )
        return 0;
    memset( destbuf,0,len );
    destpos = destbuf;	// position in output buffer 
                        // now unpack data till the dest buffer is full 
    while ( len ) {
        // read a new block of compressed data and reset variables 
        if ( !readblock( module ) )
            return 0;
        blklen = (len < 0x8000) ? len : 0x8000;
        blkpos = 0;
        width = 9;	    // start with width of 9 bits 
        d1 = d2 = 0;	// reset integrator buffers 
                        // now uncompress the data block 
        while ( blkpos < blklen ) {
            char    v;
            value = readbits( width );	// read bits 
            if ( width < 7 ) {	                        // method 1 (1-6 bits) 
                if ( value == (1 << (width - 1)) ) {	// check for "100..." 
                    value = readbits( 3 ) + 1;	        // yes -> read new width; 
                    width = (value < width) ? value : value + 1;	// and expand it 
                    continue;	                        // ... next value 
                }
            } else if ( width < 9 ) {	                // method 2 (7-8 bits) 
                unsigned char border = (0xFF >> (9 - width)) - 4;	// lower border for width chg 

                if ( value > border && value <= (border + 8) ) {
                    value -= border;	                // convert width to 1-8 
                    width = (value < width) ? value : value + 1;	// and expand it 
                    continue;	                        // ... next value 
                }
            } else if ( width == 9 ) {	                // method 3 (9 bits) 
                if ( value & 0x100 ) {	                // bit 8 set? 
                    width = (value + 1) & 0xff;	        // new width... 
                    continue;	                        // ... and next value 
                }
            } else {	                                // illegal width, abort 
                freeblock();
                return 0;
            }

            // now expand value to signed byte 
            if ( width < 8 ) {
                unsigned char shift = 8 - width;
                v = (value << shift);
                v >>= shift;
            } else
                v = (char)value;

            // integrate upon the sample values 
            d1 += v;
            d2 += d1;

            // ... and store it into the buffer 
            *(destpos++) = isIt215Compression_ ? d2 : d1;
            blkpos++;

        }
        // now subtract block length from total length and go on 
        freeblock();
        len -= blklen;
    }
    return 1;
}

// decompresses 16-bit sample (params : file, outbuffer, lenght of
//                                     uncompressed sample, IT2.15
//                                     compression flag
//                             returns: status                     )
int ItSex::decompress16( VirtualFile& module,void *dst,int len )
{
    short*          destbuf;	// the destination buffer which will be returned 
    unsigned        blklen;		// length of compressed data block in samples 
    unsigned        blkpos;		// position in block 
    unsigned char   width;		// actual "bit width" 
    unsigned        value;		// value read from file to be processed 
    int             d1,d2;		// integrator buffers (d2 for it2.15) 
    short*          destpos;

    destbuf = (short *)dst;
    if ( !destbuf )
        return 0;

    memset( destbuf,0,len << 1 );
    destpos = destbuf;	// position in output buffer 
                        // now unpack data till the dest buffer is full 
    while ( len )
    {
        // read a new block of compressed data and reset variables 
        if ( !readblock( module ) )
            return 0;
        blklen = (len < 0x4000) ? len : 0x4000;	// 0x4000 samples => 0x8000 bytes again 
        blkpos = 0;

        width = 17;	    // start with width of 17 bits 
        d1 = d2 = 0;	// reset integrator buffers 

                        // now uncompress the data block 
        while ( blkpos < blklen ) {
            short v;

            value = readbits( width );	                            // read bits 

            if ( width < 7 ) {	                                    // method 1 (1-6 bits) 
                if ( value == (1 << (width - 1)) ) {	            // check for "100..." 
                    value = readbits( 4 ) + 1;	                    // yes -> read new width; 
                    width = (value < width) ? value : value + 1;	// and expand it 
                    continue;	                                    // ... next value 
                }
            } 
            else if ( width < 17 ) {	                            // method 2 (7-16 bits) 
                unsigned short border = (0xFFFF >> (17 - width)) - 8;// lower border for width chg 
                if ( value > ( unsigned )border && value <= (unsigned)(border + 16) ) {
                    value -= border;	                            // convert width to 1-8 
                    width = (value < width) ? value : value + 1;	// and expand it 
                    continue;	                                    // ... next value 
                }
            } 
            else if ( width == 17 ) {	                            // method 3 (17 bits) 
                if ( value & 0x10000 ) {                            // bit 16 set? 
                    width = (value + 1) & 0xff;	                    // new width... 
                    continue;	                                    // ... and next value 
                }
            } 
            else {	                                                // illegal width, abort 
                freeblock();
                return 0;
            }            

            // now expand value to signed word 
            if ( width < 16 ) {
                unsigned char shift = 16 - width;
                v = (value << shift);
                v >>= shift;
            } else
                v = (int)value;

            // integrate upon the sample values 
            d1 += v;
            d2 += d1;

            // ... and store it into the buffer 
            *(destpos++) = isIt215Compression_ ? d2 : d1;
            blkpos++;
        }
        // now subtract block length from total length and go on 
        freeblock();
        len -= blklen;
    }
    return 1;
}


