#pragma once
#include <iostream>
#include <fstream>
#include <string>

#define VIRTFILE_NO_ERROR           0
#define VIRTFILE_EOF                1
#define VIRTFILE_BUFFER_OVERRUN     2
#define VIRTFILE_BUFFER_UNDERRUN    3
#define VIRTFILE_READ_ERROR         10  // errors are fatal from 10 on

/*
    the seek and read functions return an IO error which can be read
    additionally with the getIOError() function ONCE. after calling 
    the getIOError() function the internal error register is reset.

    For the readBits function, every byte from which at least one bit
    has been read will be considered as read by the regular read()
    function.
    Always use the resetBitPtr() function before you start reading the
    file as a bitstream instead of a byte stream.

    Intel stores the bytes that make up an integer in reversed order
    in memory. For example: 

        32 bit value 0xF1E2D3C4:
        In memory, the bytes will be ordered as follows:
            byte nr 1: C4
            byte nr 2: D3
            byte nr 3: E2
            byte nr 4: F1

        Other example: 16 bit value 0xF1E2:
        In memory, the bytes will be ordered as follows:
            byte nr 1: E2
            byte nr 2: F1

        Yet another example: a struct of two 16 bit integers:
        {
            unsigned short int a = 0xF1E2;
            unsigned short int b = 0xD3C4;
        }
        In memory, the bytes will be ordered as follows:
            byte nr 1: E2
            byte nr 2: F1
            byte nr 3: C4
            byte nr 4: D3

    Because of this, the next byte that is read from the bit stream 
    is always more significant than the previous one. 

    Remember, resetBitRead() MUST be called before you start reading
    whatever comes next in the file as a bitstream!
*/
typedef int IOError;


template <class P> class MemoryBlock  // not used
{
public:
    MemoryBlock( P *source,unsigned nElements ) :
        data_( source ),
        size_( nElements )
    {
        assert( source != nullptr );
        assert( nElements != 0 );
    }
    P& operator[] ( unsigned i ) {
        if ( i < nElements_ ) return data_[i];
        else return data_[nElements_ - 1];
    }
    unsigned    nElements()
    {
        return nElements_;
    }
private:
    P           *data_ = nullptr;
    unsigned    nElements_;
};

class VirtualFile {
public:
    VirtualFile( std::string &fileName ) :
        fileName_( fileName )
    {
        fileSize_ = 0;
        data_ = nullptr;
        filePos_ = nullptr;
        fileEOF_ = nullptr;
        std::ifstream   file(
            fileName_,std::ios::in | 
            std::ios::binary | 
            std::ios::ate 
        );
        if ( !file.is_open() )
        {
            ioError_ = VIRTFILE_READ_ERROR;
            return;
        }
        fileSize_ = file.tellg();
        data_ = new char[(int)fileSize_];
        filePos_ = data_;
        fileEOF_ = data_ + (int)fileSize_;
        file.seekg( 0,std::ios::beg );
        file.read( data_,fileSize_ );
        file.close();
        ioError_ = VIRTFILE_NO_ERROR;
    }
    VirtualFile( const VirtualFile& virtualFile )
    {
        // copying this object is not allowed
        exit( -1 );
    }
    ~VirtualFile()
    {
        delete data_;
    }
    IOError     getIOError()
    {
        IOError ioError = ioError_;
        ioError_ = VIRTFILE_NO_ERROR;
        return ioError;
    }
    IOError     read( void *dest,int quantity ) 
    {
        if ( filePos_ + quantity <= fileEOF_ )
        {
            memcpy( dest,filePos_,quantity );
            filePos_ += quantity;
            ioError_ = VIRTFILE_NO_ERROR;
        } 
        else 
        {
            memset( dest,0,quantity );
            memcpy( dest,filePos_,fileEOF_ - filePos_ );
            filePos_ = fileEOF_;
            ioError_ = VIRTFILE_EOF;
        }
        return  ioError_;
    }
    IOError     resetBitRead()
    {
        bitsLeft_ = 8;
        lastBitContainer_ = 0;
        return read( &lastBitContainer_,sizeof( unsigned char ) );
    }
    IOError     bitRead( unsigned& dest, unsigned char quantity )
    {
        assert( quantity <= sizeof( unsigned ) << 3 ); // read max 32 bits
        dest = 0;
        int offset = 0;
        for ( ;quantity; )
        {
            int m = quantity;
            if ( m > bitsLeft_ )
                m = bitsLeft_;
            dest |= (lastBitContainer_ & ((1L << m) - 1)) << offset;
            lastBitContainer_ >>= m;
            quantity -= m;
            offset += m;
            if ( !(bitsLeft_ -= m) )
            {
                IOError ioError = read( &lastBitContainer_,sizeof( unsigned char ) );
                if ( ioError ) return ioError;
                bitsLeft_ = 8;
            }
        }
        return VIRTFILE_NO_ERROR;
    }


    IOError     relSeek( int position )
    {
        if ( data_ == nullptr )
        {
            ioError_ = VIRTFILE_READ_ERROR;
            return ioError_;
        }
        ioError_ = VIRTFILE_NO_ERROR;
        if ( (position < 0) &&
            ( (- position) > (int)filePos_ ) ) filePos_ = 0;
        else filePos_ += position;
        if ( filePos_ < data_ )
        {
            filePos_ = data_;
            ioError_ = VIRTFILE_BUFFER_UNDERRUN;
        } 
        else if ( filePos_ > fileEOF_ )
        {
            filePos_ = fileEOF_;
            ioError_ = VIRTFILE_BUFFER_OVERRUN;
        } 
        else if ( filePos_ == fileEOF_ )
        { 
            ioError_ = VIRTFILE_EOF;
        }
        return ioError_;
    }
    IOError     absSeek( unsigned position )
    {
        filePos_ = data_ + position;
        ioError_ = VIRTFILE_NO_ERROR;
        if ( filePos_ > fileEOF_ )
        {
            filePos_ = fileEOF_;
            ioError_ = VIRTFILE_BUFFER_OVERRUN;
        } 
        else if ( filePos_ == fileEOF_ )
        {
            ioError_ = VIRTFILE_EOF;
        }        
        return ioError_;
    }
    unsigned    fileSize()
    {
        ioError_ = VIRTFILE_NO_ERROR;
        if ( data_ == nullptr )
        {
            ioError_ = VIRTFILE_READ_ERROR;
            return 0;
        }
        return (unsigned)fileSize_;
    }
    unsigned    dataLeft()
    {
        ioError_ = VIRTFILE_NO_ERROR;
        if ( data_ == nullptr )
        {
            ioError_ = VIRTFILE_READ_ERROR;
            return 0;
        }
        return fileEOF_ - filePos_;
    }
    template<class PTR> MemoryBlock<PTR> getPointer( unsigned nElements )
    {
        unsigned byteSize = nElements * sizeof( *PTR );
        if ( filePos_ + byteSize <= fileEOF_ )
        {
            ioError_ = VIRTFILE_NO_ERROR;
            MemoryBlock<PTR> memoryBlock( (PTR *)filePos_,nElements );
            return memoryBlock;
        } 
        else
        {
            ioError_ = VIRTFILE_EOF;
            MemoryBlock<PTR> memoryBlock( nullptr,0 );
            return memoryBlock;
        }
    }
    //const void * const getSafePointer( unsigned byteSize )
    void       *getSafePointer( unsigned byteSize )
    {
        if ( filePos_ + byteSize <= fileEOF_ )
        {
            ioError_ = VIRTFILE_NO_ERROR;
            return filePos_;
        } else
        {
            ioError_ = VIRTFILE_EOF;
            return nullptr;
        }
    }

private:
    std::string     fileName_;
    IOError         ioError_ = VIRTFILE_READ_ERROR;
    char            *data_;
    char            *filePos_;
    char            *fileEOF_;
    std::ifstream::pos_type  fileSize_;
    unsigned char   bitsLeft_;
    unsigned char   lastBitContainer_; // last byte read with leftover bits 
};

