#include "demuxer.h"
#include <iostream>

int main() {
    // Create demuxer
    Demuxer *demuxer = new Demuxer();
    
    // Open a stream
    if (!demuxer->open_stream("test.mp4")) {
        std::cerr << "Failed to open stream" << std::endl;
        delete demuxer;
        return -1;
    }
    
    std::cout << "Stream opened successfully!" << std::endl;
    
    // Read some packets
    int packet_count = 0;
    while (packet_count < 10) {
        DemuxPacket *packet = nullptr;
        if (demuxer->read_packet(&packet)) {
            std::cout << "Got packet " << packet_count 
                      << ", stream: " << packet->stream
                      << ", PTS: " << packet->pts 
                      << ", size: " << packet->len 
                      << ", keyframe: " << (packet->keyframe ? "yes" : "no") << std::endl;
            
            delete packet;
            packet_count++;
        } else {
            std::cout << "End of stream reached" << std::endl;
            break;
        }
    }
    
    // Test seeking
    std::cout << "Seeking to 5.0 seconds..." << std::endl;
    demuxer->seek(5.0);
    
    // Read a few more packets after seek
    for (int i = 0; i < 3; i++) {
        DemuxPacket *packet = nullptr;
        if (demuxer->read_packet(&packet)) {
            std::cout << "After seek - packet " << i 
                      << ", PTS: " << packet->pts << std::endl;
            delete packet;
        }
    }
    
    // Close demuxer
    demuxer->close();
    delete demuxer;
    
    std::cout << "Example completed successfully!" << std::endl;
    return 0;
}