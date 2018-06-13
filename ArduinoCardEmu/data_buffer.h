#ifndef DATA_BUFFER_H
#define DATA_BUFFER_H

//TODO
class DataBuffer
{
  public:
    //rewind to the start
    void WriteRewind();
    //make written data available for reading
    void WriteCommit();
    //discard data that was read
    void ReadTrim();
    //rewind to the start
    void ReadRewind();
};

#endif
