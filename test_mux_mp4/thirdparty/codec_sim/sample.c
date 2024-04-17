#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "ipc.h"
#include "../thirdparty/log/log.h"

int VideoFrameCallBack(uint8_t *frame, int len, int iskey, int64_t timestamp)
{
    frame_info_t frame_info;

    frame_info.frame = frame;
    frame_info.timestamp = timestamp;
    frame_info.len = len;
    frame_info.frame_type = FRAME_TYPE_VIDEO;

    /**TODO - Get frame here and handle */
    // if (iskey == 1)
    log_debug("Timestamp: %ld , frame VIDEO len: %d , is key frame: %d \n", timestamp, len, iskey);

    return 0;
}

int AudioFrameCallBack(uint8_t *frame, int len, int64_t timestamp)
{
    frame_info_t frame_info;

    frame_info.frame = frame;
    frame_info.timestamp = timestamp;
    frame_info.len = len;
    frame_info.frame_type = FRAME_TYPE_AUDIO;

    /**TODO - Get frame here and handle */
    log_debug("Timestamp: %ld , frame AUDIO len: %d \n", timestamp, len);

    return 0;
}

int main()
{

    ipc_dev_register(&sim_ipc);
    log_set_level(LOG_DEBUG);

    ipc_param_t param =
        {
            .audio_codec = AUDIO_AAC,
            .video_codec = H264,
            .video_fps = 25,
            .audio_sample = 48000, // Don't need set this param, program auto define base on audio file
            .video_file = "../test_file/big_file.h264", // Test with big_file becase this file have many I frame
            .audio_file = "../test_file/frame_to_file_aac.aac",
            .pic_file = NULL,
            .video_cb = VideoFrameCallBack,
            .audio_cb = AudioFrameCallBack,
            .event_cb = NULL,
        };

    ipc_init(&param);
    ipc_run();

    while (1)
    {
        sleep(2);
    }
}