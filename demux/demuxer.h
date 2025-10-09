#ifndef DEMUXER_H
#define DEMUXER_H

#include <cstdint>
#include <string>
#include "stream.h"

// Simple packet structure
struct DemuxPacket {
    double pts;           // Presentation timestamp
    double dts;           // Decode timestamp  
    double duration;      // Packet duration
    int64_t pos;          // Position in source file
    
    unsigned char *buffer; // Packet data
    size_t len;           // Packet data length
    
    int stream;           // Source stream index
    bool keyframe;        // Is this a keyframe
    
    DemuxPacket();
    ~DemuxPacket();
};

class Demuxer
{
private:
    void *avfc;           // AVFormatContext*
    void *pb;             // AVIOContext*
    std::string filename;
    bool is_open;

public:
    Demuxer();
    ~Demuxer();
    
    // Open stream and create AVFormatContext
    bool demux_open_filename(const std::string &filename);
    
    // Seek using avio_seek or av_seek_frame
    bool seek(double timestamp_seconds);
    
    // Read packet using av_read_frame
    bool read_packet(DemuxPacket **packet);
    
    // Close and cleanup
    void close();
    
    // Check if demuxer is open
    bool is_open_stream() const { return is_open; }
};

#endif // DEMUXER_H
