#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "../thirdparty/h264reader/h264reader.h"
#include "../thirdparty/minimp4/include/minimp4.h"
#include "../thirdparty/log/log.h"

static uint8_t *load_segment(FILE *file_input, int ofs, ssize_t *segment_size)
{
    uint8_t *segment_data;
    *segment_size = 0;
    ssize_t file_size;
    if(file_input == NULL)
        return 0;
    if (fseek(file_input, 0, SEEK_END))
        exit(1);
    file_size = (ssize_t)ftell(file_input);
    log_debug("file size: %ld", file_size);
    *segment_size = file_size - ofs;
    if(*segment_size < 0)
        exit(1);
    segment_data = (unsigned char*)malloc(*segment_size);
    if (fseek(file_input, ofs, SEEK_SET))
        exit(1);
    if (segment_data == NULL)
        exit(1);
    if ((ssize_t)fread(segment_data, 1, *segment_size, file_input) != *segment_size)
    {
        log_debug("Fail");
        exit(1);
    }
    else
        log_debug("OK");
    log_debug("%p", segment_data);
    return segment_data;
}

typedef struct
{
    uint8_t *buffer;
    ssize_t size;
} INPUT_BUFFER;

static int read_callback(int64_t offset, void *buffer, size_t size, void *token)
{
    INPUT_BUFFER *buf = (INPUT_BUFFER*)token;
    size_t to_copy = MINIMP4_MIN(size, buf->size - offset - size);
    memcpy(buffer, buf->buffer + offset, to_copy);
    return to_copy != size;
}

int main()
{
    // LOAD SEGMENT TO BUFFER

    ssize_t segment_size;
    //int offset = 19386929; //frame 280
    int offset = 0;

    FILE *file_input = fopen("/home/ndp/Documents/workspace/test_mux_mp4/test_file/test_mp4_video.mp4", "rb");
    uint8_t *input_buf = load_segment(file_input, offset, &segment_size );
    if (input_buf == NULL)
    {
        log_error("Can't open mp4 file to demux\n");
        return -1;
    }
    else
    {
        log_debug("Opened mp4 file [OK]\n");
    }
    
    log_debug("segment size: %ld", segment_size);
    log_debug("%p", file_input);
    log_debug("%p", input_buf);
    
   
    FILE *h264_file_output = fopen("/home/ndp/Documents/workspace/test_mux_mp4/demux_output_file/test_demux_ofs.h264", "wb");
    if (h264_file_output == NULL)
    {
        log_error("Can't open output h264 file\n");
        return -1;
    }
    else
    {
        log_debug("Opened output h264 file [OK]\n");
    }

    // OPEN DEMUX

    MP4D_demux_t mp4_demux ;
    MP4D_open(&mp4_demux, read_callback, &input_buf, segment_size);

   
    int i = 0; 
    int spspps_bytes;
    int ntrack = 0;
    const void *spspps;

    // WRITE SPS

    char sync[4] = { 0, 0, 0, 1 };
    while (spspps = MP4D_read_sps(&mp4_demux, ntrack, i, &spspps_bytes))
    {
        log_debug("%ld",spspps_bytes);
        fwrite(sync , 1, 4 , h264_file_output);
        fwrite(spspps, 1, spspps_bytes, h264_file_output);
        i++;
    }
    
    // WRITE PPS
    i = 0;
    while (spspps = MP4D_read_pps(&mp4_demux, ntrack, i, &spspps_bytes))
    {
        log_debug("%ld",spspps_bytes);
        fwrite(sync , 1, 4 , h264_file_output);
        fwrite(spspps, 1, spspps_bytes, h264_file_output);
        i++;
    }

    // WRITE FRAME

    for (i = 0; i < mp4_demux.track[ntrack].sample_count; i++)
    {
       
        log_debug("Sample: %d", i);
        unsigned frame_bytes, timestamp, duration;
        MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4_demux, ntrack, i, &frame_bytes, &timestamp, &duration);
       
    
            
        uint8_t *mem = input_buf + ofs - offset;
        log_debug("mem: %x, input buffer: %x", *(mem+4), *(input_buf+4));
        log_debug("frame bytes: %ld, offset: %ld", frame_bytes, ofs);
    
        while (frame_bytes)
        {
            //log_debug("mem[0]: %x, mem[1]: %x, mem[2]: %x, mem[3]: %x,", mem[0], mem[1], mem[2], mem[3]);
            uint32_t size = ((uint32_t)mem[0] << 24) | ((uint32_t)mem[1] << 16) | ((uint32_t)mem[2] << 8) | mem[3];
            size += 4;
            log_debug("Size: %ld", size);
            mem[0] = 0; mem[1] = 0; mem[2] = 0; mem[3] = 1;
            fwrite(mem, 1, size, h264_file_output);
            if (frame_bytes < size)
            {
                log_error("demux sample failed\n");
                return -1;
            }
            //log_debug("mem: %x", *(mem+size+3));
            frame_bytes -= size;
            mem += size;
            
        }
        
    }
        
        //sleep(1000);
    
    log_debug("segment size: %ld", segment_size);
    MP4D_close(&mp4_demux);
    if (input_buf)
        free(input_buf);
    fclose(h264_file_output);
    fclose(file_input);
    return 0;
}