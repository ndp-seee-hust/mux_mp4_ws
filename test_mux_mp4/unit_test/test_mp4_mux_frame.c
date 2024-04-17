#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "../thirdparty/h264reader/h264reader.h"
#include "../thirdparty/minimp4/include/minimp4.h"
#include "../thirdparty/codec_sim/ipc.h"
#include "../thirdparty/log/log.h"

static int write_callback(int64_t offset, const void *buffer, size_t size, void *token)
{
    FILE *f = (FILE *)token;
    fseek(f, offset, SEEK_SET);
    return fwrite(buffer, 1, size, f) != size;
}

int main()
{
    char *av_frame = NULL;
    int av_frame_size = 0;
    int av_max_frame_size = 1024 * 1024;
    void *pH264Reader = NULL;
    int ret = 0;

    FILE *mp4_file_mux = fopen("../test_file/test_mp4_video.mp4", "wb");
    if (mp4_file_mux == NULL)
    {
        LOG_ERROR("Can't open file mp4");
        return -1;
    }

    pH264Reader = H264FileReaderCreate("../test_file/test.h264");
    if (pH264Reader == NULL)
    {
        LOG_ERROR("Can't open file h264 for test");
        return -1;
    }

#define VIDEO_FPS 25 // based on file test.h264

    av_frame = malloc(sizeof(char) * av_max_frame_size);
    if (av_frame == NULL)
    {
        H264FileReaderRemove(pH264Reader);
        return -3;
    }

    int sequential_mode = 1;
    int fragmentation_mode = 0;
    int do_demux = 0;
    int is_hevc = 0;

    MP4E_mux_t *mp4_mux = NULL;
    mp4_h26x_writer_t mp4wr;
    mp4_mux = MP4E_open(sequential_mode, fragmentation_mode, mp4_file_mux, write_callback);
    if (MP4E_STATUS_OK != mp4_h26x_write_init(&mp4wr, mp4_mux, 1920, 1080, is_hevc))
    {
        printf("error: mp4_h26x_write_init failed\n");
        return -1;
    }
    else
        printf("Create mp4 file ok\n");

    while (1)
    {
        av_frame_size = av_max_frame_size;
        memset(av_frame, 0, av_max_frame_size);

        usleep((int)((1.0 / VIDEO_FPS) * 100000));

        ret = H264FileReaderGetFrame(pH264Reader, av_frame, &av_frame_size);
        if (ret == 0)
        {
            printf("No data to read\n");
            break;
        }

        printf("===> %x %x %x %x %x\n", av_frame[0], av_frame[1], av_frame[2], av_frame[3], av_frame[4]);

        if (MP4E_STATUS_OK != mp4_h26x_write_nal(&mp4wr, av_frame, av_frame_size, 90000 / VIDEO_FPS)) //<=== ????
        {
            printf("error: mp4_h26x_write_nal failed\n");
            return -1;
        }
    }

    if (av_frame != NULL)
        free(av_frame);
    MP4E_close(mp4_mux);
    mp4_h26x_write_close(&mp4wr);
    if (mp4_file_mux)
        fclose(mp4_file_mux);
    H264FileReaderRemove(pH264Reader);

    return 0;
}
