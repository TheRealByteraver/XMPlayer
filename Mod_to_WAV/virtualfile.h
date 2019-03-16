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
*/
typedef int IOError;

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
        if ( !file.is_open() ) ioError_ = VIRTFILE_READ_ERROR;
        fileSize_ = file.tellg();
        data_ = new char[(int)fileSize_];
        filePos_ = data_;
        fileEOF_ = data_ + (int)fileSize_;
        file.seekg( 0,std::ios::beg );
        file.read( data_,fileSize_ );
        file.close();
        ioError_ = VIRTFILE_NO_ERROR;
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
    IOError     relSeek( int position )
    {
        if ( data_ == nullptr )
        {
            ioError_ = VIRTFILE_READ_ERROR;
            return ioError_;
        }
        if ( (position < 0) && 
            ( (- position) > (int)filePos_ ) ) filePos_ = 0;
        else filePos_ += position;
        ioError_ = VIRTFILE_NO_ERROR;
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

private:
    std::string     fileName_;
    IOError         ioError_ = VIRTFILE_READ_ERROR;
    char            *data_;
    char            *filePos_;
    char            *fileEOF_;
    std::ifstream::pos_type  fileSize_;
};