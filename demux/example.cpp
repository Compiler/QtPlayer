#include "demuxer.h"
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <media_file>" << std::endl;
        return -1;
    }
    
    // Create demuxer
    Demuxer *demuxer = new Demuxer();
    
    // Open a stream using demux_open_filename which internally calls demux_open()
    if (!demuxer->demux_open_filename(argv[1])) {
        std::cerr << "Failed to open stream: " << argv[1] << std::endl;
        delete demuxer;
        return -1;
    }
    
    std::cout << "Stream opened successfully!" << std::endl;
    std::cout << "Duration: " << demuxer->get_duration() << " seconds" << std::endl;
    std::cout << "Start time: " << demuxer->get_start_time() << " seconds" << std::endl;
    std::cout << "Seekable: " << (demuxer->is_seekable() ? "yes" : "no") << std::endl;
    
    // Read some packets
    int packet_count = 0;
    std::cout << "\nReading first 10 packets..." << std::endl;
    while (packet_count < 10) {
        DemuxPacket *packet = nullptr;
        if (demuxer->read_packet(&packet)) {
            std::cout << "Packet " << packet_count 
                      << " - stream: " << packet->stream
                      << ", PTS: " << packet->pts 
                      << ", DTS: " << packet->dts
                      << ", duration: " << packet->duration
                      << ", size: " << packet->len 
                      << ", pos: " << packet->pos
                      << ", keyframe: " << (packet->keyframe ? "yes" : "no") << std::endl;
            
            delete packet;
            packet_count++;
        } else {
            std::cout << "End of stream or error" << std::endl;
            break;
        }
    }
    
    // Test seeking if the stream is seekable
    if (demuxer->is_seekable() && demuxer->get_duration() > 5.0) {
        std::cout << "\nSeeking to 5.0 seconds..." << std::endl;
        if (demuxer->seek(5.0)) {
            // Read a few packets after seek
            std::cout << "Reading 5 packets after seek..." << std::endl;
            for (int i = 0; i < 5; i++) {
                DemuxPacket *packet = nullptr;
                if (demuxer->read_packet(&packet)) {
                    std::cout << "After seek - packet " << i 
                              << ", stream: " << packet->stream
                              << ", PTS: " << packet->pts 
                              << ", keyframe: " << (packet->keyframe ? "yes" : "no") << std::endl;
                    delete packet;
                } else {
                    break;
                }
            }
        } else {
            std::cerr << "Seek failed" << std::endl;
        }
    }
    
    // Close demuxer
    demuxer->close();
    delete demuxer;
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}