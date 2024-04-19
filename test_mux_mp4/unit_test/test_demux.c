#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "../thirdparty/h264reader/h264reader.h"
#include "../thirdparty/minimp4/include/minimp4.h"
#include "../thirdparty/log/log.h"


static uint8_t *preload(const char *path, ssize_t *data_size)
{
    FILE *file = fopen(path, "rb");
    uint8_t *data;
    *data_size = 0;
    if (!file)
        return 0;
    if (fseek(file, 0, SEEK_END))
        exit(1);
    *data_size = (ssize_t)ftell(file);
    if (*data_size < 0)
        exit(1);
    if (fseek(file, 0, SEEK_SET))
        exit(1);
    data = (unsigned char*)malloc(*data_size);
    if (!data)
        exit(1);
    if ((ssize_t)fread(data, 1, *data_size, file) != *data_size)
        exit(1);
    fclose(file);
    return data;
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
    
    ssize_t h264_size;
    uint8_t *input_buf = preload("/home/ndp/Documents/workspace/test_mux_mp4/test_file/test_mp4_video.mp4", &h264_size);
    if (input_buf == NULL)
    {
        log_error("Can't open mp4 file to demux\n");
        return -1;
    }
    else
    {
        log_debug("Opened mp4 file [OK]\n");
    }
    
    
    FILE *h264_file_output = fopen("/home/ndp/Documents/workspace/test_mux_mp4/demux_output_file/test_demux.h264", "wb");
    if (h264_file_output == NULL)
    {
        log_error("Can't open output h264 file\n");
        return -1;
    }
    else
    {
        log_debug("Opened output h264 file [OK]\n");
    }
    
    INPUT_BUFFER buf = { input_buf, h264_size };
    MP4D_demux_t mp4_demux ;
    MP4D_open(&mp4_demux, read_callback, &buf, h264_size);


    int i = 0;
    int ntrack = 0;
    
    for (i = 0; i < mp4_demux.track[ntrack].sample_count; i++)
        {
            unsigned frame_bytes, timestamp, duration;
            MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4_demux, ntrack, i, &frame_bytes, &timestamp, &duration);
            uint8_t *mem = input_buf + ofs;
            
            log_debug("sample: %d, frame_bytes: %d, timestamp: %d, duration: %d", i, frame_bytes, timestamp, duration);
            
            // fwrite(mem, 1, frame_bytes, h264_file_output);
            // mem += frame_bytes;

            while (frame_bytes)
            {
                uint32_t size = ((uint32_t)mem[0] << 24) | ((uint32_t)mem[1] << 16) | ((uint32_t)mem[2] << 8) | mem[3];
                size += 4;
                log_debug("size: %d", size);
                mem[0] = 0; mem[1] = 0; mem[2] = 0; mem[3] = 1;

                fwrite(mem, 1, size, h264_file_output);
                log_debug("write to h264 file");
                if (frame_bytes < size)
                {
                    log_error("demux sample failed\n");
                    return -1;
                }
                frame_bytes -= size;
                mem += size;
               
            }
            printf("\n");
        }
        
    MP4D_close(&mp4_demux);
    if (input_buf)
        free(input_buf);
    fclose(h264_file_output);
    // if(h264_file_output != NULL)
    //     free(h264_file_output);
    if(h264_file_output != NULL)
    {
        log_debug("Can't close h264 file output\n");
    }
    else
        log_debug("Closed h264 file output\n");

    return 0;
}