
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "../../thirdparty/log/log.h"
#include "ipc.h"

typedef struct
{
    int running;
    ipc_dev_t *dev;
    char *video_file;
    char *audio_file;

    int audio_codec;
    int video_codec;
    int video_fps;
    int audio_sample;

    char *pic_file;
    int (*video_cb)(uint8_t *frame, int len, int iskey, int64_t timestamp);
    int (*audio_cb)(uint8_t *frame, int len, int64_t timestamp);
    int (*event_cb)(int event, void *data);
} sim_ipc_t;

typedef struct _LinkADTSFixheader
{
    unsigned short syncword : 12;
    unsigned char id : 1;
    unsigned char layer : 2;
    unsigned char protection_absent : 1;
    unsigned char profile : 2;
    unsigned char sampling_frequency_index : 4;
    unsigned char private_bit : 1;
    unsigned char channel_configuration : 3;
    unsigned char original_copy : 1;
    unsigned char home : 1;
} LinkADTSFixheader;

typedef struct _LinkADTSVariableHeader
{
    unsigned char copyright_identification_bit : 1;
    unsigned char copyright_identification_start : 1;
    unsigned short aac_frame_length : 13;
    unsigned short adts_buffer_fullness : 11;
    unsigned char number_of_raw_data_blocks_in_frame : 2;
} LinkADTSVariableHeader;

static int aacfreq[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                          22050, 16000, 12000, 11025, 8000, 7350};

static void LinkParseAdtsfixedHeader(const unsigned char *pData, LinkADTSFixheader *_pHeader)
{
    unsigned long long adts = 0;
    const unsigned char *p = pData;
    adts |= *p++;
    adts <<= 8;
    adts |= *p++;
    adts <<= 8;
    adts |= *p++;
    adts <<= 8;
    adts |= *p++;
    adts <<= 8;
    adts |= *p++;
    adts <<= 8;
    adts |= *p++;
    adts <<= 8;
    adts |= *p++;

    _pHeader->syncword = (adts >> 44);
    _pHeader->id = (adts >> 43) & 0x01;
    _pHeader->layer = (adts >> 41) & 0x03;
    _pHeader->protection_absent = (adts >> 40) & 0x01;
    _pHeader->profile = (adts >> 38) & 0x03;
    _pHeader->sampling_frequency_index = (adts >> 34) & 0x0f;
    _pHeader->private_bit = (adts >> 33) & 0x01;
    _pHeader->channel_configuration = (adts >> 30) & 0x07;
    _pHeader->original_copy = (adts >> 29) & 0x01;
    _pHeader->home = (adts >> 28) & 0x01;
}

static void LinkParseAdtsVariableHeader(const unsigned char *pData, LinkADTSVariableHeader *_pHeader)
{
    unsigned long long adts = 0;
    adts = pData[0];
    adts <<= 8;
    adts |= pData[1];
    adts <<= 8;
    adts |= pData[2];
    adts <<= 8;
    adts |= pData[3];
    adts <<= 8;
    adts |= pData[4];
    adts <<= 8;
    adts |= pData[5];
    adts <<= 8;
    adts |= pData[6];

    _pHeader->copyright_identification_bit = (adts >> 27) & 0x01;
    _pHeader->copyright_identification_start = (adts >> 26) & 0x01;
    _pHeader->aac_frame_length = (adts >> 13) & ((int)pow(2, 14) - 1);
    _pHeader->adts_buffer_fullness = (adts >> 2) & ((int)pow(2, 11) - 1);
    _pHeader->number_of_raw_data_blocks_in_frame = adts & 0x03;
}

static void *sim_ipc_video_task1(void *instance)
{
    sim_ipc_t *ipc = (sim_ipc_t *)instance;
    char *p_video_buf = NULL, *p_end = NULL;
    char *tmp = NULL;
    char start_code[3] = {0x00, 0x00, 0x01};
    char start_code2[4] = {0x00, 0x00, 0x00, 0x01};
    int idr = 0;

    time_t timestamp;
    time(&timestamp);

    FILE *fp = NULL;
    int size = 0;
    log_debug("sim receive frame video");

    if (!ipc || !ipc->video_file)
    {
        log_error("the h264 file is NULL,should pass h264 file first\n");
        return NULL;
    }

    fp = fopen(ipc->video_file, "r");
    if (!fp)
    {
        log_error("open file %s error\n", ipc->video_file);
        return NULL;
    }

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    log_debug("video file size is %d\n", size);
    fseek(fp, 0L, SEEK_SET);
    p_video_buf = (char *)malloc(size);
    if (!p_video_buf)
    {
        log_error("malloc error\n");
        return NULL;
    }
    fread(p_video_buf, 1, size, fp);
    tmp = p_video_buf;
    p_end = p_video_buf + size;

    log_warn("%ld", p_video_buf);
    log_warn("%ld", p_end);

    while (ipc->running && (long)p_video_buf < (long)p_end)
    {
        int frame_len = 0, type = 0;

        memcpy(&frame_len, p_video_buf, 4);

        p_video_buf += 4;

        if (p_video_buf + frame_len < p_end)
        {
            if (memcmp(start_code, p_video_buf, 3) == 0)
            {
                log_debug("Type start code");
                type = p_video_buf[3] & 0x1f;
            }
            else if (memcmp(start_code2, p_video_buf, 4) == 0)
            {
                log_debug("Type start code 2");
                type = p_video_buf[4] & 0x1f;
            }
            else
            {
                log_error("get nalu start code fail\n");
                return NULL;
            }

            int i;
            for (i = 0; i < 5; i++)
            {
                printf("%2x", p_video_buf[i]);
                if (i == 4)
                    printf("\n");
            }

            log_error("type frame %d", type);
            int is_key_frame = 0;
            if (type == 5)
            {
                idr++;
                is_key_frame = 1;
            }

            if (ipc->video_cb)
                ipc->video_cb((uint8_t *)p_video_buf, frame_len, is_key_frame, timestamp);

            p_video_buf += frame_len;
            timestamp += 40;
            usleep(40 * 1000);
        }
        else
        {
            p_video_buf = tmp;
        }
    }

    free(p_video_buf);
    return NULL;
}

#if 0
static void *sim_ipc_video_task2(void *instance)
{
    sim_ipc_t *ipc = (sim_ipc_t *)instance;

    int ret = 0;
    char *video_frame = NULL;
    int video_frame_size = 0;
    int video_max_frame_size = 1024 * 1024;
    int frame_cnt = 0;
    void *pH264Reader = NULL;

    if (!ipc || !ipc->video_file)
    {
        log_error("the h264 file is NULL,should pass h264 file first\n");
        return NULL;
    }

    time_t timestamp;
    time(&timestamp);

    while (ipc->running)
    {
        pH264Reader = H264FileReaderCreate(ipc->video_file);
        if (pH264Reader == NULL)
            return NULL;

        video_frame = malloc(sizeof(char) * video_max_frame_size);
        if (video_frame == NULL)
        {
            H264FileReaderRemove(pH264Reader);
            return NULL;
        }

        do
        {
            int is_key_frame = 0;

            video_frame_size = video_max_frame_size;
            memset(video_frame, 0, video_max_frame_size);
            log_warn("%ld", video_frame);

            ret = H264FileReaderGetFrame(pH264Reader, video_frame, &video_frame_size);

            if (ret < 0)
            {
                log_warn("End VIDEO file ");
                free(video_frame);
                H264FileReaderRemove(pH264Reader);
                break;
            }

            int i;
            /* Check I frame */
            // for (i = 0; i < 5; i++)
            // {
            //     printf("%2x", video_frame[i]);
            //     if (i == 4)
            //         printf("\n");
            // }

            int frame_type = H264CheckNalType(video_frame[4]);
            if (frame_type == 5)
                is_key_frame = 1;

            // if (frame_type == NALU_TYPE_SPS)
            //     log_debug("SPS frame");

            /* Callback */
            if (ipc->video_cb)
                ipc->video_cb((uint8_t *)video_frame, video_frame_size, is_key_frame, timestamp);

            log_warn("%ld", video_frame);
            timestamp += 40;
            usleep(40 * 1000);

        } while (1);

        free(video_frame);
        H264FileReaderRemove(pH264Reader);
    }

    return NULL;
}
#endif

#define CACH_LEN (1024 * 1024)
static uint8_t *g_cach[2] = {NULL, NULL};
static FILE *fp_inH264 = NULL;
static int icach = 0;
static int ioffset = 0;
static int bLoop = 0;
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static int check_start_frame(uint8_t *buffer, int offset)
{
    static uint8_t startcode[4] = {0x00, 0x00, 0x00, 0x01};
    return !memcmp(buffer + offset, startcode, 4);
}

int get_one_frame(uint8_t *buf, int buf_len)
{
    int i = 0;
    int start_point = ioffset;
    int end_point = ioffset;
    for (i = ioffset + 4; i <= CACH_LEN - 4; i++)
    {
        if (check_start_frame(g_cach[icach], i))
        {
            start_point = ioffset;
            end_point = i;
            break;
        }
    }

    if (end_point - start_point > 0)
    {
        int data_len = end_point - start_point;
        if (buf_len < data_len)
        {
            log_error("recv buffer too short, need %d byte", data_len);
        }
        memcpy(buf, g_cach[icach] + start_point, MIN(data_len, buf_len));
        ioffset = end_point;
        return MIN(data_len, buf_len);
    }
    else
    {
        int old_len = CACH_LEN - start_point;
        memcpy(g_cach[(icach + 1) % 2], g_cach[icach] + start_point, old_len);

        int new_len = 0;
        new_len = fread(g_cach[(icach + 1) % 2] + old_len, 1, CACH_LEN - (old_len), fp_inH264);
        if (new_len < CACH_LEN - old_len)
        {
            if (bLoop)
            {
                fseek(fp_inH264, 0, SEEK_SET);
                ioffset = 0;
                icach = 0;
                fread(g_cach[icach], 1, CACH_LEN, fp_inH264);
                return get_one_frame(buf, buf_len);
            }
            else
            {
                if (new_len <= 0)
                {
                    return -1;
                }
                memset(g_cach[(icach + 1) % 2] + old_len + new_len, 0, CACH_LEN - old_len - new_len);
            }
        }
        ioffset = 0;
        icach = (icach + 1) % 2;
        return get_one_frame(buf, buf_len);
    }
}

int init(char *file_h264)
{
    g_cach[0] = (uint8_t *)malloc(CACH_LEN);
    g_cach[1] = (uint8_t *)malloc(CACH_LEN);

    fp_inH264 = fopen(file_h264, "r");
    if (fp_inH264 == NULL)
        return -1;

    if (fread(g_cach[icach], 1, CACH_LEN, fp_inH264) < CACH_LEN)
    {
        log_error("input file too short");
        return -1;
    }

    return 0;
}

int deinit()
{
    if (g_cach[0])
    {
        free(g_cach[0]);
        g_cach[0] = NULL;
    }

    if (g_cach[1])
    {
        free(g_cach[1]);
        g_cach[1] = NULL;
    }

    if (fp_inH264)
    {
        fclose(fp_inH264);
        fp_inH264 = NULL;
    }
}

int H264CheckNalType(uint8_t buff)
{
    return (buff && 0x1F);
}

static void *sim_ipc_video_task(void *instance)
{
    sim_ipc_t *ipc = (sim_ipc_t *)instance;

    uint8_t *buffer = (uint8_t *)malloc(CACH_LEN);

    while (ipc->running)
    {
        if (init(ipc->video_file))
            return NULL;

        int I_frame_cnt = 0;
        struct timeval tv;

        int len = 0;
        while ((len = get_one_frame(buffer, CACH_LEN)) > 0)
        {
            int is_key_frame = 0;
            int frame_type = H264CheckNalType(buffer[4]);
            if (frame_type == 5)
            {
                is_key_frame = 1;
                I_frame_cnt++;
            }

            gettimeofday(&tv, NULL);
            uint64_t timestamp = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000; // in milliseconds

            if (ipc->video_cb)
                ipc->video_cb((uint8_t *)buffer, len, is_key_frame, timestamp);

            // timestamp += 40;

            // if (frame_type == NALU_TYPE_AUD || frame_type == NALU_TYPE_DPA || frame_type == NALU_TYPE_DPB ||
            //     frame_type == NALU_TYPE_DPC || frame_type == NALU_TYPE_EOSEQ || frame_type == NALU_TYPE_EOSTREAM ||
            //     frame_type == NALU_TYPE_FILL || frame_type == NALU_TYPE_SEI || frame_type == NALU_TYPE_SLICE)
            // {
            long sleep = (1.0 / ipc->video_fps) * 1000 * 1000;
            // log_debug("%d", ipc->video_fps);
            // log_debug("Sleep: %ld", sleep);
            usleep(sleep);
            // usleep(40 * 1000); // fps = 1 / (0.04 s) = 25}
            // }
        }

        free(buffer);
        deinit();
    }

    return NULL;
}

int GetFileSize(char *_pFileName)
{
    FILE *fp = fopen(_pFileName, "r");
    int size = 0;

    if (fp == NULL)
    {
        log_error("fopen file %s error\n", _pFileName);
        return -1;
    }

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    fclose(fp);

    return size;
}

void *sim_ipc_audio_task(void *param)
{
    sim_ipc_t *ipc = (sim_ipc_t *)param;
    if (!ipc)
    {
        log_error("check param error\n");
        return NULL;
    }

    int len = GetFileSize(ipc->audio_file);
    if (len <= 0)
    {
        log_error("GetFileSize error\n");
        exit(1);
    }
    log_info("Audio file size: %d bytes", len);

    FILE *fp = fopen(ipc->audio_file, "r");
    if (!fp)
    {
        log_error("open file %s error\n", ipc->audio_file);
        return NULL;
    }

    char *buf_ptr = (char *)malloc(len);
    if (!buf_ptr)
    {
        log_error("malloc error\n");
        exit(1);
    }
    memset(buf_ptr, 0, len);
    fread(buf_ptr, 1, len, fp);

    int offset = 0;
    // int64_t timeStamp = 0;

    int64_t interval = 0;
    int count = 0;

    struct timeval tv;

    while (ipc->running)
    {
        LinkADTSFixheader fix;
        LinkADTSVariableHeader var;

        if (offset + 7 <= len)
        {
            LinkParseAdtsfixedHeader((unsigned char *)(buf_ptr + offset), &fix);
            int size = fix.protection_absent == 1 ? 7 : 9;
            // LOGI("size = %d\n", size );
            LinkParseAdtsVariableHeader((unsigned char *)(buf_ptr + offset), &var);
            if (offset + size + var.aac_frame_length <= len)
            {
                if (ipc->audio_cb)
                {
                    count++;
                    // log_debug("fix.sampling_frequency_index = %d", fix.sampling_frequency_index);

                    // 1024 is number sample of each audio frame, 1000 for convert to ms
                    interval = ((1024 * 1000.0) / aacfreq[fix.sampling_frequency_index]);
                    // log_debug("interval = %ld", interval);
                    // timeStamp += interval * count;

                    gettimeofday(&tv, NULL);
                    uint64_t timeStamp = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000; // in milliseconds

                    // callback
                    ipc->audio_cb((uint8_t *)buf_ptr + offset, var.aac_frame_length, timeStamp);
                }
            }

            offset += var.aac_frame_length;
            // log_debug("var.aac_frame_length = %d\n", var.aac_frame_length);
            usleep(interval * 1000);
        }
        else
        {
            offset = 0;
        }
    }

    return NULL;
}

int sim_ipc_init(struct ipc_dev_t *dev, ipc_param_t *param)
{
    sim_ipc_t *sim = NULL;

    if (!dev || !param)
    {
        log_error("check param error\n");
        return -1;
    }

    log_debug("sim ipc init");

    if (!dev)
    {
        log_error("check param error\n");
        return -1;
    }
    sim = (sim_ipc_t *)malloc(sizeof(sim_ipc_t));
    if (!sim)
    {
        log_error("malloc error\n");
        return -1;
    }

    sim->running = 1;
    sim->dev = dev;
    sim->audio_codec = param->audio_codec;
    sim->video_codec = param->video_codec;
    sim->video_fps = param->video_fps;
    sim->audio_sample = param->audio_sample;

    sim->video_file = param->video_file;
    sim->audio_file = param->audio_file;
    sim->pic_file = param->pic_file;
    sim->video_cb = param->video_cb;
    sim->audio_cb = param->audio_cb;
    sim->event_cb = param->event_cb;
    dev->priv = (void *)sim;

    return 0;
}

void *sim_ipc_motion_detect_task(void *arg)
{
    sim_ipc_t *ipc = (sim_ipc_t *)arg;

    if (!ipc->event_cb)
    {
        log_error("check event_cb fail\n");
        return NULL;
    }

    for (;;)
    {
        sleep(5);
        ipc->event_cb(EVENT_MOTION_DETECTION, NULL);
        sleep(8);
        ipc->event_cb(EVENT_MOTION_DETECTION_DISAPEER, NULL);
    }
    return NULL;
}

void sim_ipc_run(ipc_dev_t *dev)
{
    pthread_t thread;
    log_debug("===> Create threads ");
    int ret = pthread_create(&thread, NULL, sim_ipc_video_task, dev->priv);
    if (ret != 0)
        log_error("Create thread fail");
    ret = pthread_create(&thread, NULL, sim_ipc_audio_task, dev->priv);
    if (ret != 0)
        log_error("Create thread fail");
    ret = pthread_create(&thread, NULL, sim_ipc_motion_detect_task, dev->priv);
    if (ret != 0)
        log_error("Create thread fail");
    else
        log_debug("===> Create thread ok");
}

int sim_ipc_capture_picture(ipc_dev_t *dev, char *file)
{
    sim_ipc_t *sim = (sim_ipc_t *)dev->priv;
    char buf[512] = {0};

    log_debug("Sim capture pic");

    if (!file || !sim)
    {
        log_error("check param error\n");
        return -1;
    }
    if (!sim->pic_file)
    {
        log_error("check pic_file error\n");
        return -1;
    }

    sprintf(buf, "cp %s %s", sim->pic_file, file);
    log_info("cmd : %s\n", buf);
    system(buf);
    if (sim->event_cb)
    {
        sim->event_cb(EVENT_CAPTURE_PICTURE_SUCCESS, file);
    }
    return 0;
}

void sim_ipc_deinit(ipc_dev_t *dev)
{
    log_debug("Sim ipc deiinit");
    if (dev->priv)
    {
        free(dev->priv);
    }
}

ipc_dev_t sim_ipc =
    {
        .init = sim_ipc_init,
        .deinit = sim_ipc_deinit,
        .capture_picture = sim_ipc_capture_picture,
        .run = sim_ipc_run,
};

/**NOTE - This function auto call before main() is excuted*/
static void __attribute__((constructor)) sim_ipc_register()
{
    log_debug("IPC register");
    ipc_dev_register(&sim_ipc);
}
