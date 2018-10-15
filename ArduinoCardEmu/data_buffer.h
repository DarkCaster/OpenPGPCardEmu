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
    //put data to the buffer
    void PutData(const uint8_t * const data, const uint16_t len);
    //discard data that was read
    void ReadTrim();
    //rewind to the start
    void ReadRewind();
};

#endif
