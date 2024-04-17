/**
 * @file minimp4.h
 * @author minimp4
 * @brief Header file minimp4
 * @version 0.1
 * @date 2023-05-19
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef MINIMP4_H
#define MINIMP4_H
/*
    https://github.com/aspt/mp4
    https://github.com/lieff/minimp4
    To the extent possible under law, the author(s) have dedicated all copyright and related
    and neighboring rights to this software to the public domain worldwide.
    This software is distributed without any warranty.
    See <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MINIMP4_MIN(x, y) ((x) < (y) ? (x) : (y))

    /*********************************************/
    /*                 LOG function  //NCL       */
    /*********************************************/
#if 1
#define LEVEL_ERROR 0x00
#define LEVEL_WARN 0x01
#define LEVEL_INFO 0x02
#define LEVEL_DEBUG 0x03

#define ERROR_TAG "ERROR"
#define WARN_TAG "WARN"
#define INFO_TAG "INFO"
#define DEBUG_TAG "DEBUG"

#ifndef LOG_LEVEL
#define LOG_LEVEL LEVEL_INFO
#endif

#define LOG_PRINT(level_tag, fmt, ...) \
    fprintf(stdout, "[%s %s:%d] " fmt "\n", level_tag, __FILE__, __LINE__, ##__VA_ARGS__)

#if LOG_LEVEL >= LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) LOG_PRINT(INFO_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if LOG_LEVEL >= LEVEL_INFO
#define LOG_INFO(fmt, ...) LOG_PRINT(INFO_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL >= LEVEL_WARN
#define LOG_WARN(fmt, ...) LOG_PRINT(WARN_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)
#endif

#if LOG_LEVEL >= LEVEL_ERROR
#define LOG_ERROR(fmt, ...) LOG_PRINT(ERROR_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)
#endif

#endif

    /************************************************************************/
    /*                  Build configuration                                 */
    /************************************************************************/

#define FIX_BAD_ANDROID_META_BOX 1

#define MAX_CHUNKS_DEPTH 64 // Max chunks nesting level

#define MINIMP4_MAX_SPS 32
#define MINIMP4_MAX_PPS 256

#define MINIMP4_TRANSCODE_SPS_ID 1

// Support indexing of MP4 files over 4 GB.
// If disabled, files with 64-bit offset fields is still supported,
// but error signaled if such field contains too big offset
// This switch affect return type of MP4D_frame_offset() function
#define MINIMP4_ALLOW_64BIT 1

#define MP4D_TRACE_SUPPORTED 1 // Debug trace
#define MP4D_TRACE_TIMESTAMPS 1
// Support parsing of supplementary information, not necessary for decoding:
// duration, language, bitrate, metadata tags, etc
#define MP4D_INFO_SUPPORTED 1

// Enable code, which prints to stdout supplementary MP4 information:
#define MP4D_PRINT_INFO_SUPPORTED 1

#define MP4D_AVC_SUPPORTED 1
#define MP4D_HEVC_SUPPORTED 1
#define MP4D_TIMESTAMPS_SUPPORTED 1

// Enable TrackFragmentBaseMediaDecodeTimeBox support
#define MP4D_TFDT_SUPPORT 0

/************************************************************************/
/*          Some values of MP4(E/D)_track_t->object_type_indication     */
/************************************************************************/
// MPEG-4 AAC (all profiles)
#define MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3 0x40
// MPEG-2 AAC, Main profile
#define MP4_OBJECT_TYPE_AUDIO_ISO_IEC_13818_7_MAIN_PROFILE 0x66
// MPEG-2 AAC, LC profile
#define MP4_OBJECT_TYPE_AUDIO_ISO_IEC_13818_7_LC_PROFILE 0x67
// MPEG-2 AAC, SSR profile
#define MP4_OBJECT_TYPE_AUDIO_ISO_IEC_13818_7_SSR_PROFILE 0x68
// H.264 (AVC) video
#define MP4_OBJECT_TYPE_AVC 0x21
// H.265 (HEVC) video
#define MP4_OBJECT_TYPE_HEVC 0x23
// http://www.mp4ra.org/object.html 0xC0-E0  && 0xE2 - 0xFE are specified as "user private"
#define MP4_OBJECT_TYPE_USER_PRIVATE 0xC0

/************************************************************************/
/*          API error codes                                             */
/************************************************************************/
#define MP4E_STATUS_OK 0
#define MP4E_STATUS_BAD_ARGUMENTS -1
#define MP4E_STATUS_NO_MEMORY -2
#define MP4E_STATUS_FILE_WRITE_ERROR -3
#define MP4E_STATUS_ONLY_ONE_DSI_ALLOWED -4

/************************************************************************/
/*          Sample kind for MP4E_put_sample()                           */
/************************************************************************/
#define MP4E_SAMPLE_DEFAULT 0       // (beginning of) audio or video frame
#define MP4E_SAMPLE_RANDOM_ACCESS 1 // mark sample as random access point (key frame)
#define MP4E_SAMPLE_CONTINUATION 2  // Not a sample, but continuation of previous sample (new slice)

    /************************************************************************/
    /*                  Portable 64-bit type definition                     */
    /************************************************************************/

#if MINIMP4_ALLOW_64BIT
    /**
     * @brief uint64_t
     *
     */
    typedef uint64_t boxsize_t;
#else
typedef unsigned int boxsize_t;
#endif
    typedef boxsize_t MP4D_file_offset_t;

/************************************************************************/
/*          Some values of MP4D_track_t->handler_type              */
/************************************************************************/
// Video track : 'vide'
#define MP4D_HANDLER_TYPE_VIDE 0x76696465
// Audio track : 'soun'
#define MP4D_HANDLER_TYPE_SOUN 0x736F756E
// General MPEG-4 systems streams (without specific handler).
// Used for private stream, as suggested in http://www.mp4ra.org/handler.html
#define MP4E_HANDLER_TYPE_GESM 0x6765736D

#define HEVC_NAL_VPS 32
#define HEVC_NAL_SPS 33
#define HEVC_NAL_PPS 34
#define HEVC_NAL_BLA_W_LP 16
#define HEVC_NAL_CRA_NUT 21

    /************************************************************************/
    /*          Data structures                                             */
    /************************************************************************/

    typedef struct MP4E_mux_tag MP4E_mux_t;

    typedef enum
    {
        e_audio,
        e_video,
        e_private
    } track_media_kind_t;

    typedef struct
    {
        // MP4 object type code, which defined codec class for the track.
        // See MP4E_OBJECT_TYPE_* values for some codecs
        unsigned object_type_indication;

        // Track language: 3-char ISO 639-2T code: "und", "eng", "rus", "jpn" etc...
        unsigned char language[4];

        track_media_kind_t track_media_kind;

        // 90000 for video, sample rate for audio
        unsigned time_scale;
        unsigned default_duration;

        union
        {
            struct
            {
                // number of channels in the audio track.
                unsigned channelcount;
            } a;

            struct
            {
                int width;
                int height;
            } v;
        } u;

    } MP4E_track_t;

    typedef struct MP4D_sample_to_chunk_t_tag MP4D_sample_to_chunk_t;

    typedef struct
    {
        /************************************************************************/
        /*                 mandatory (bat buoc) public data               */
        /************************************************************************/
        // How many 'samples' in the track
        // The 'sample' is MP4 term, denoting audio or video frame
        unsigned sample_count;

        // Decoder-specific info (DSI) data
        unsigned char *dsi;

        // DSI data size
        unsigned dsi_bytes;

        // MP4 object type code
        // case 0x00: return "Forbidden";
        // case 0x01: return "Systems ISO/IEC 14496-1";
        // case 0x02: return "Systems ISO/IEC 14496-1";
        // case 0x20: return "Visual ISO/IEC 14496-2";
        // case 0x40: return "Audio ISO/IEC 14496-3";
        // case 0x60: return "Visual ISO/IEC 13818-2 Simple Profile";
        // case 0x61: return "Visual ISO/IEC 13818-2 Main Profile";
        // case 0x62: return "Visual ISO/IEC 13818-2 SNR Profile";
        // case 0x63: return "Visual ISO/IEC 13818-2 Spatial Profile";
        // case 0x64: return "Visual ISO/IEC 13818-2 High Profile";
        // case 0x65: return "Visual ISO/IEC 13818-2 422 Profile";
        // case 0x66: return "Audio ISO/IEC 13818-7 Main Profile";
        // case 0x67: return "Audio ISO/IEC 13818-7 LC Profile";
        // case 0x68: return "Audio ISO/IEC 13818-7 SSR Profile";
        // case 0x69: return "Audio ISO/IEC 13818-3";
        // case 0x6A: return "Visual ISO/IEC 11172-2";
        // case 0x6B: return "Audio ISO/IEC 11172-3";
        // case 0x6C: return "Visual ISO/IEC 10918-1";
        unsigned object_type_indication;

#if MP4D_INFO_SUPPORTED
        /************************************************************************/
        /*                 informational public data                            */
        /************************************************************************/
        // handler_type when present in a media box, is an integer containing one of
        // the following values, or a value from a derived specification:
        // 'vide' Video track
        // 'soun' Audio track
        // 'hint' Hint track - A hint track is used to describe how to send the reference media track over a particular network transport - NCL
        unsigned handler_type;

        // Track duration: 64-bit value split into 2 variables
        unsigned duration_hi;
        unsigned duration_lo;

        // duration scale: duration = timescale*seconds
        unsigned timescale;

        // Average bitrate, bits per second
        unsigned avg_bitrate_bps;

        // Track language: 3-char ISO 639-2T code: "und", "eng", "rus", "jpn" etc...
        unsigned char language[4];

        // MP4 stream type
        // case 0x00: return "Forbidden";
        // case 0x01: return "ObjectDescriptorStream";
        // case 0x02: return "ClockReferenceStream";
        // case 0x03: return "SceneDescriptionStream";
        // case 0x04: return "VisualStream";
        // case 0x05: return "AudioStream";
        // case 0x06: return "MPEG7Stream";
        // case 0x07: return "IPMPStream";
        // case 0x08: return "ObjectContentInfoStream";
        // case 0x09: return "MPEGJStream";
        unsigned stream_type;

        union
        {
            // for handler_type == 'soun' tracks
            struct
            {
                unsigned channelcount;
                unsigned samplerate_hz;
            } audio;

            // for handler_type == 'vide' tracks
            struct
            {
                unsigned width;
                unsigned height;
            } video;
        } SampleDescription;
#endif

        /************************************************************************/
        /*                 private data: MP4 indexes                            */
        /************************************************************************/
        unsigned *entry_size;

        unsigned sample_to_chunk_count;
        struct MP4D_sample_to_chunk_t_tag *sample_to_chunk;

        unsigned chunk_count;
        MP4D_file_offset_t *chunk_offset;

#if MP4D_TIMESTAMPS_SUPPORTED
        unsigned *timestamp;
        unsigned *duration;
#endif

    } MP4D_track_t;

    typedef struct MP4D_demux_tag
    {
        /************************************************************************/
        /*                 mandatory public data                                */
        /************************************************************************/
        int64_t read_pos;
        int64_t read_size;
        MP4D_track_t *track;
        int (*read_callback)(int64_t offset, void *buffer, size_t size, void *token);
        void *token;

        unsigned track_count; // number of tracks in the movie

#if MP4D_INFO_SUPPORTED
        /************************************************************************/
        /*                 informational public data                            */
        /************************************************************************/
        // Movie duration: 64-bit value split into 2 variables
        unsigned duration_hi;
        unsigned duration_lo;

        // duration scale: duration = timescale*seconds
        unsigned timescale;

        // Metadata tag (optional)
        // Tags provided 'as-is', without any re-encoding
        struct
        {
            unsigned char *title;
            unsigned char *artist;
            unsigned char *album;
            unsigned char *year;
            unsigned char *comment;
            unsigned char *genre;
        } tag;
#endif

    } MP4D_demux_t;

    /**
     * @brief struct MP4D_sample_to_chunk_t_tag
     * @param unsigned first_chunk;
     * @param unsigned samples_per_chunk;
     */
    struct MP4D_sample_to_chunk_t_tag
    {
        unsigned first_chunk;
        unsigned samples_per_chunk;
    };

    typedef struct
    {
        void *sps_cache[MINIMP4_MAX_SPS];
        void *pps_cache[MINIMP4_MAX_PPS];
        int sps_bytes[MINIMP4_MAX_SPS];
        int pps_bytes[MINIMP4_MAX_PPS];

        int map_sps[MINIMP4_MAX_SPS];
        int map_pps[MINIMP4_MAX_PPS];

    } h264_sps_id_patcher_t;

    typedef struct mp4_h26x_writer_tag
    {
#if MINIMP4_TRANSCODE_SPS_ID
        h264_sps_id_patcher_t sps_patcher;
#endif
        MP4E_mux_t *mux;
        int mux_track_id;
        int is_hevc; // hevc enable ?
        int need_vps;
        int need_sps;
        int need_pps;
        int need_idr;
    } mp4_h26x_writer_t;

    int mp4_h26x_write_init(mp4_h26x_writer_t *h, MP4E_mux_t *mux, int width, int height, int is_hevc);
    void mp4_h26x_write_close(mp4_h26x_writer_t *h);
    int mp4_h26x_write_nal(mp4_h26x_writer_t *h, const unsigned char *nal, int length,
                           unsigned timeStamp90kHz_next);

    /************************************************************************/
    /*          API                                                         */
    /************************************************************************/

    /**
     *   Parse given input stream as MP4 file. Allocate and store data indexes.
     *   return 1 on success, 0 on failure
     *   The MP4 indexes may be stored at the end of stream, so this
     *   function may parse all stream.
     *   It is guaranteed that function will read/seek sequentially,
     *   and will never jump back.
     */
    int MP4D_open(MP4D_demux_t *mp4,
                  int (*read_callback)(int64_t offset, void *buffer, size_t size, void *token),
                  void *token, int64_t file_size);

    /**
     *   Return position and size for given sample from given track. The 'sample' is a
     *   MP4 term for 'frame'
     *
     *   frame_bytes [OUT]   - return coded frame size in bytes
     *   timestamp [OUT]     - return frame timestamp (in mp4->timescale units)
     *   duration [OUT]      - return frame duration (in mp4->timescale units)
     *
     *   function return offset for the frame
     */
    MP4D_file_offset_t MP4D_frame_offset(const MP4D_demux_t *mp4, unsigned int ntrack,
                                         unsigned int nsample, unsigned int *frame_bytes, unsigned *timestamp, unsigned *duration);

    /**
     *   De-allocated memory
     */
    void MP4D_close(MP4D_demux_t *mp4);

    /**
     *   Helper functions to parse mp4.track[ntrack].dsi for H.264 SPS/PPS
     *   Return pointer to internal mp4 memory, it must not be free()-ed
     *
     *   Example: process all SPS in MP4 file:
     *       while (sps = MP4D_read_sps(mp4, num_of_avc_track, sps_count, &sps_bytes))
     *       {
     *           process(sps, sps_bytes);
     *           sps_count++;
     *       }
     */
    const void *MP4D_read_sps(const MP4D_demux_t *mp4, unsigned int ntrack, int nsps, int *sps_bytes);

    /**
     *   Helper functions to parse mp4.track[ntrack].dsi for H.264 SPS/PPS
     *   Return pointer to internal mp4 memory, it must not be free()-ed
     *
     *   Example: process all SPS in MP4 file:
     *       while (sps = MP4D_read_sps(mp4, num_of_avc_track, sps_count, &sps_bytes))
     *       {
     *           process(sps, sps_bytes);
     *           sps_count++;
     *       }
     */
    const void *MP4D_read_pps(const MP4D_demux_t *mp4, unsigned int ntrack, int npps, int *pps_bytes);

#if MP4D_PRINT_INFO_SUPPORTED
    /**
     *   Print MP4 information to stdout.
     *   Uses printf() as well as floating-point functions
     *   Given as implementation example and for test purposes
     */
    void MP4D_printf_info(const MP4D_demux_t *mp4);
#endif

    /**
     *   Allocates and initialize mp4 multiplexor
     *   Given file handler is transparent to the MP4 library, and used only as
     *   argument for given fwrite_callback() function.  By appropriate definition
     *   of callback function application may use any other file output API (for
     *   example C++ streams, or Win32 file functions)
     *
     *   return multiplexor handle on success; NULL on failure
     */
    MP4E_mux_t *MP4E_open(int sequential_mode_flag, int enable_fragmentation, void *token,
                          int (*write_callback)(int64_t offset, const void *buffer, size_t size, void *token));

    /**
     *   Add new track
     *   The track_data parameter does not referred by the multiplexer after function
     *   return, and may be allocated in short-time memory. The dsi member of
     *   track_data parameter is mandatory.
     *
     *   return ID of added track, or error code MP4E_STATUS_*
     */
    int MP4E_add_track(MP4E_mux_t *mux, const MP4E_track_t *track_data);

    /**
     *   Add new sample to specified track
     *   The tracks numbered starting with 0, according to order of MP4E_add_track() calls
     *   'kind' is one of MP4E_SAMPLE_... defines
     *
     *   return error code MP4E_STATUS_*
     *
     *   Example:
     *       MP4E_put_sample(mux, 0, data, data_bytes, duration, MP4E_SAMPLE_DEFAULT);
     */
    int MP4E_put_sample(MP4E_mux_t *mux, int track_num, const void *data,
                        int data_bytes, int duration, int kind);

    /**
     *   Finalize MP4 file, de-allocated memory, and closes MP4 multiplexer.
     *   The close operation takes a time and disk space, since it writes MP4 file
     *   indexes.  Please note that this function does not closes file handle,
     *   which was passed to open function.
     *
     *   return error code MP4E_STATUS_*
     */
    int MP4E_close(MP4E_mux_t *mux);

    /**
     *   Set Decoder Specific Info (DSI)
     *   Can be used for audio and private tracks.
     *   MUST be used for AAC track.
     *   Only one DSI can be set. It is an error to set DSI again
     *
     *   return error code MP4E_STATUS_*
     */
    int MP4E_set_dsi(MP4E_mux_t *mux, int track_id, const void *dsi, int bytes);

    /**
     *   Set VPS data. MUST be used for HEVC (H.265) track.
     *
     *   return error code MP4E_STATUS_*
     */
    int MP4E_set_vps(MP4E_mux_t *mux, int track_id, const void *vps, int bytes);

    /**
     *   Set SPS data. MUST be used for AVC (H.264) track. Up to 32 different SPS can be used in one track.
     *
     *   return error code MP4E_STATUS_*
     */
    int MP4E_set_sps(MP4E_mux_t *mux, int track_id, const void *sps, int bytes);

    /**
     *   Set PPS data. MUST be used for AVC (H.264) track. Up to 256 different PPS can be used in one track.
     *
     *   return error code MP4E_STATUS_*
     */
    int MP4E_set_pps(MP4E_mux_t *mux, int track_id, const void *pps, int bytes);

    /**
     *   Set or replace ASCII test comment for the file. Set comment to NULL to remove comment.
     *
     *   return error code MP4E_STATUS_*
     */
    int MP4E_set_text_comment(MP4E_mux_t *mux, const char *comment);

#ifdef __cplusplus
}
#endif
#endif // MINIMP4_H

#define MINIMP4_IMPLEMENTATION
#if defined(MINIMP4_IMPLEMENTATION) && !defined(MINIMP4_IMPLEMENTATION_GUARD)
#define MINIMP4_IMPLEMENTATION_GUARD

#define FOUR_CHAR_INT(a, b, c, d) (((uint32_t)(a) << 24) | ((b) << 16) | ((c) << 8) | (d))
enum
{
    BOX_co64 = FOUR_CHAR_INT('c', 'o', '6', '4'), // ChunkLargeOffsetAtomType
    BOX_stco = FOUR_CHAR_INT('s', 't', 'c', 'o'), // ChunkOffsetAtomType
    BOX_crhd = FOUR_CHAR_INT('c', 'r', 'h', 'd'), // ClockReferenceMediaHeaderAtomType
    BOX_ctts = FOUR_CHAR_INT('c', 't', 't', 's'), // CompositionOffsetAtomType
    BOX_cprt = FOUR_CHAR_INT('c', 'p', 'r', 't'), // CopyrightAtomType
    BOX_url_ = FOUR_CHAR_INT('u', 'r', 'l', ' '), // DataEntryURLAtomType
    BOX_urn_ = FOUR_CHAR_INT('u', 'r', 'n', ' '), // DataEntryURNAtomType
    BOX_dinf = FOUR_CHAR_INT('d', 'i', 'n', 'f'), // DataInformationAtomType
    BOX_dref = FOUR_CHAR_INT('d', 'r', 'e', 'f'), // DataReferenceAtomType
    BOX_stdp = FOUR_CHAR_INT('s', 't', 'd', 'p'), // DegradationPriorityAtomType
    BOX_edts = FOUR_CHAR_INT('e', 'd', 't', 's'), // EditAtomType
    BOX_elst = FOUR_CHAR_INT('e', 'l', 's', 't'), // EditListAtomType
    BOX_uuid = FOUR_CHAR_INT('u', 'u', 'i', 'd'), // ExtendedAtomType
    BOX_free = FOUR_CHAR_INT('f', 'r', 'e', 'e'), // FreeSpaceAtomType
    BOX_hdlr = FOUR_CHAR_INT('h', 'd', 'l', 'r'), // HandlerAtomType
    BOX_hmhd = FOUR_CHAR_INT('h', 'm', 'h', 'd'), // HintMediaHeaderAtomType
    BOX_hint = FOUR_CHAR_INT('h', 'i', 'n', 't'), // HintTrackReferenceAtomType
    BOX_mdia = FOUR_CHAR_INT('m', 'd', 'i', 'a'), // MediaAtomType
    BOX_mdat = FOUR_CHAR_INT('m', 'd', 'a', 't'), // MediaDataAtomType
    BOX_mdhd = FOUR_CHAR_INT('m', 'd', 'h', 'd'), // MediaHeaderAtomType
    BOX_minf = FOUR_CHAR_INT('m', 'i', 'n', 'f'), // MediaInformationAtomType
    BOX_moov = FOUR_CHAR_INT('m', 'o', 'o', 'v'), // MovieAtomType
    BOX_mvhd = FOUR_CHAR_INT('m', 'v', 'h', 'd'), // MovieHeaderAtomType
    BOX_stsd = FOUR_CHAR_INT('s', 't', 's', 'd'), // SampleDescriptionAtomType
    BOX_stsz = FOUR_CHAR_INT('s', 't', 's', 'z'), // SampleSizeAtomType
    BOX_stz2 = FOUR_CHAR_INT('s', 't', 'z', '2'), // CompactSampleSizeAtomType
    BOX_stbl = FOUR_CHAR_INT('s', 't', 'b', 'l'), // SampleTableAtomType
    BOX_stsc = FOUR_CHAR_INT('s', 't', 's', 'c'), // SampleToChunkAtomType
    BOX_stsh = FOUR_CHAR_INT('s', 't', 's', 'h'), // ShadowSyncAtomType
    BOX_skip = FOUR_CHAR_INT('s', 'k', 'i', 'p'), // SkipAtomType
    BOX_smhd = FOUR_CHAR_INT('s', 'm', 'h', 'd'), // SoundMediaHeaderAtomType
    BOX_stss = FOUR_CHAR_INT('s', 't', 's', 's'), // SyncSampleAtomType
    BOX_stts = FOUR_CHAR_INT('s', 't', 't', 's'), // TimeToSampleAtomType
    BOX_trak = FOUR_CHAR_INT('t', 'r', 'a', 'k'), // TrackAtomType
    BOX_tkhd = FOUR_CHAR_INT('t', 'k', 'h', 'd'), // TrackHeaderAtomType
    BOX_tref = FOUR_CHAR_INT('t', 'r', 'e', 'f'), // TrackReferenceAtomType
    BOX_udta = FOUR_CHAR_INT('u', 'd', 't', 'a'), // UserDataAtomType
    BOX_vmhd = FOUR_CHAR_INT('v', 'm', 'h', 'd'), // VideoMediaHeaderAtomType
    BOX_url = FOUR_CHAR_INT('u', 'r', 'l', ' '),
    BOX_urn = FOUR_CHAR_INT('u', 'r', 'n', ' '),

    BOX_gnrv = FOUR_CHAR_INT('g', 'n', 'r', 'v'), // GenericVisualSampleEntryAtomType
    BOX_gnra = FOUR_CHAR_INT('g', 'n', 'r', 'a'), // GenericAudioSampleEntryAtomType

    // V2 atoms
    BOX_ftyp = FOUR_CHAR_INT('f', 't', 'y', 'p'), // FileTypeAtomType
    BOX_padb = FOUR_CHAR_INT('p', 'a', 'd', 'b'), // PaddingBitsAtomType

    // MP4 Atoms
    BOX_sdhd = FOUR_CHAR_INT('s', 'd', 'h', 'd'), // SceneDescriptionMediaHeaderAtomType
    BOX_dpnd = FOUR_CHAR_INT('d', 'p', 'n', 'd'), // StreamDependenceAtomType
    BOX_iods = FOUR_CHAR_INT('i', 'o', 'd', 's'), // ObjectDescriptorAtomType
    BOX_odhd = FOUR_CHAR_INT('o', 'd', 'h', 'd'), // ObjectDescriptorMediaHeaderAtomType
    BOX_mpod = FOUR_CHAR_INT('m', 'p', 'o', 'd'), // ODTrackReferenceAtomType
    BOX_nmhd = FOUR_CHAR_INT('n', 'm', 'h', 'd'), // MPEGMediaHeaderAtomType
    BOX_esds = FOUR_CHAR_INT('e', 's', 'd', 's'), // ESDAtomType
    BOX_sync = FOUR_CHAR_INT('s', 'y', 'n', 'c'), // OCRReferenceAtomType
    BOX_ipir = FOUR_CHAR_INT('i', 'p', 'i', 'r'), // IPIReferenceAtomType
    BOX_mp4s = FOUR_CHAR_INT('m', 'p', '4', 's'), // MPEGSampleEntryAtomType
    BOX_mp4a = FOUR_CHAR_INT('m', 'p', '4', 'a'), // MPEGAudioSampleEntryAtomType
    BOX_mp4v = FOUR_CHAR_INT('m', 'p', '4', 'v'), // MPEGVisualSampleEntryAtomType

    // http://www.itscj.ipsj.or.jp/sc29/open/29view/29n7644t.doc
    BOX_avc1 = FOUR_CHAR_INT('a', 'v', 'c', '1'),
    BOX_avc2 = FOUR_CHAR_INT('a', 'v', 'c', '2'),
    BOX_svc1 = FOUR_CHAR_INT('s', 'v', 'c', '1'),
    BOX_avcC = FOUR_CHAR_INT('a', 'v', 'c', 'C'),
    BOX_svcC = FOUR_CHAR_INT('s', 'v', 'c', 'C'),
    BOX_btrt = FOUR_CHAR_INT('b', 't', 'r', 't'),
    BOX_m4ds = FOUR_CHAR_INT('m', '4', 'd', 's'),
    BOX_seib = FOUR_CHAR_INT('s', 'e', 'i', 'b'),

    // H264/HEVC
    BOX_hev1 = FOUR_CHAR_INT('h', 'e', 'v', '1'),
    BOX_hvc1 = FOUR_CHAR_INT('h', 'v', 'c', '1'),
    BOX_hvcC = FOUR_CHAR_INT('h', 'v', 'c', 'C'),

    // 3GPP atoms
    BOX_samr = FOUR_CHAR_INT('s', 'a', 'm', 'r'), // AMRSampleEntryAtomType
    BOX_sawb = FOUR_CHAR_INT('s', 'a', 'w', 'b'), // WB_AMRSampleEntryAtomType
    BOX_damr = FOUR_CHAR_INT('d', 'a', 'm', 'r'), // AMRConfigAtomType
    BOX_s263 = FOUR_CHAR_INT('s', '2', '6', '3'), // H263SampleEntryAtomType
    BOX_d263 = FOUR_CHAR_INT('d', '2', '6', '3'), // H263ConfigAtomType

    // V2 atoms - Movie Fragments
    BOX_mvex = FOUR_CHAR_INT('m', 'v', 'e', 'x'), // MovieExtendsAtomType
    BOX_trex = FOUR_CHAR_INT('t', 'r', 'e', 'x'), // TrackExtendsAtomType
    BOX_moof = FOUR_CHAR_INT('m', 'o', 'o', 'f'), // MovieFragmentAtomType
    BOX_mfhd = FOUR_CHAR_INT('m', 'f', 'h', 'd'), // MovieFragmentHeaderAtomType
    BOX_traf = FOUR_CHAR_INT('t', 'r', 'a', 'f'), // TrackFragmentAtomType
    BOX_tfhd = FOUR_CHAR_INT('t', 'f', 'h', 'd'), // TrackFragmentHeaderAtomType
    BOX_tfdt = FOUR_CHAR_INT('t', 'f', 'd', 't'), // TrackFragmentBaseMediaDecodeTimeBox
    BOX_trun = FOUR_CHAR_INT('t', 'r', 'u', 'n'), // TrackFragmentRunAtomType
    BOX_mehd = FOUR_CHAR_INT('m', 'e', 'h', 'd'), // MovieExtendsHeaderBox

    // Object Descriptors (OD) data coding
    // These takes only 1 byte; this implementation translate <od_tag> to
    // <od_tag> + OD_BASE to keep API uniform and safe for string functions
    OD_BASE = FOUR_CHAR_INT('$', '$', '$', '0'), //
    OD_ESD = FOUR_CHAR_INT('$', '$', '$', '3'),  // SDescriptor_Tag
    OD_DCD = FOUR_CHAR_INT('$', '$', '$', '4'),  // DecoderConfigDescriptor_Tag
    OD_DSI = FOUR_CHAR_INT('$', '$', '$', '5'),  // DecoderSpecificInfo_Tag
    OD_SLC = FOUR_CHAR_INT('$', '$', '$', '6'),  // SLConfigDescriptor_Tag

    BOX_meta = FOUR_CHAR_INT('m', 'e', 't', 'a'),
    BOX_ilst = FOUR_CHAR_INT('i', 'l', 's', 't'),

    // Metagata tags, see http://atomicparsley.sourceforge.net/mpeg-4files.html
    BOX_calb = FOUR_CHAR_INT('\xa9', 'a', 'l', 'b'), // album
    BOX_cart = FOUR_CHAR_INT('\xa9', 'a', 'r', 't'), // artist
    BOX_aART = FOUR_CHAR_INT('a', 'A', 'R', 'T'),    // album artist
    BOX_ccmt = FOUR_CHAR_INT('\xa9', 'c', 'm', 't'), // comment
    BOX_cday = FOUR_CHAR_INT('\xa9', 'd', 'a', 'y'), // year (as string)
    BOX_cnam = FOUR_CHAR_INT('\xa9', 'n', 'a', 'm'), // title
    BOX_cgen = FOUR_CHAR_INT('\xa9', 'g', 'e', 'n'), // custom genre (as string or as byte!)
    BOX_trkn = FOUR_CHAR_INT('t', 'r', 'k', 'n'),    // track number (byte)
    BOX_disk = FOUR_CHAR_INT('d', 'i', 's', 'k'),    // disk number (byte)
    BOX_cwrt = FOUR_CHAR_INT('\xa9', 'w', 'r', 't'), // composer
    BOX_ctoo = FOUR_CHAR_INT('\xa9', 't', 'o', 'o'), // encoder
    BOX_tmpo = FOUR_CHAR_INT('t', 'm', 'p', 'o'),    // bpm (byte)
    BOX_cpil = FOUR_CHAR_INT('c', 'p', 'i', 'l'),    // compilation (byte)
    BOX_covr = FOUR_CHAR_INT('c', 'o', 'v', 'r'),    // cover art (JPEG/PNG)
    BOX_rtng = FOUR_CHAR_INT('r', 't', 'n', 'g'),    // rating/advisory (byte)
    BOX_cgrp = FOUR_CHAR_INT('\xa9', 'g', 'r', 'p'), // grouping
    BOX_stik = FOUR_CHAR_INT('s', 't', 'i', 'k'),    // stik (byte)  0 = Movie   1 = Normal  2 = Audiobook  5 = Whacked Bookmark  6 = Music Video  9 = Short Film  10 = TV Show  11 = Booklet  14 = Ringtone
    BOX_pcst = FOUR_CHAR_INT('p', 'c', 's', 't'),    // podcast (byte)
    BOX_catg = FOUR_CHAR_INT('c', 'a', 't', 'g'),    // category
    BOX_keyw = FOUR_CHAR_INT('k', 'e', 'y', 'w'),    // keyword
    BOX_purl = FOUR_CHAR_INT('p', 'u', 'r', 'l'),    // podcast URL (byte)
    BOX_egid = FOUR_CHAR_INT('e', 'g', 'i', 'd'),    // episode global unique ID (byte)
    BOX_desc = FOUR_CHAR_INT('d', 'e', 's', 'c'),    // description
    BOX_clyr = FOUR_CHAR_INT('\xa9', 'l', 'y', 'r'), // lyrics (may be > 255 bytes)
    BOX_tven = FOUR_CHAR_INT('t', 'v', 'e', 'n'),    // tv episode number
    BOX_tves = FOUR_CHAR_INT('t', 'v', 'e', 's'),    // tv episode (byte)
    BOX_tvnn = FOUR_CHAR_INT('t', 'v', 'n', 'n'),    // tv network name
    BOX_tvsh = FOUR_CHAR_INT('t', 'v', 's', 'h'),    // tv show name
    BOX_tvsn = FOUR_CHAR_INT('t', 'v', 's', 'n'),    // tv season (byte)
    BOX_purd = FOUR_CHAR_INT('p', 'u', 'r', 'd'),    // purchase date
    BOX_pgap = FOUR_CHAR_INT('p', 'g', 'a', 'p'),    // Gapless Playback (byte)

    // BOX_aart   = FOUR_CHAR_INT( 'a', 'a', 'r', 't' ),     // Album artist
    BOX_cART = FOUR_CHAR_INT('\xa9', 'A', 'R', 'T'), // artist
    BOX_gnre = FOUR_CHAR_INT('g', 'n', 'r', 'e'),

    // 3GPP metatags  (http://cpansearch.perl.org/src/JHAR/MP4-Info-1.12/Info.pm)
    BOX_auth = FOUR_CHAR_INT('a', 'u', 't', 'h'), // author
    BOX_titl = FOUR_CHAR_INT('t', 'i', 't', 'l'), // title
    BOX_dscp = FOUR_CHAR_INT('d', 's', 'c', 'p'), // description
    BOX_perf = FOUR_CHAR_INT('p', 'e', 'r', 'f'), // performer
    BOX_mean = FOUR_CHAR_INT('m', 'e', 'a', 'n'), //
    BOX_name = FOUR_CHAR_INT('n', 'a', 'm', 'e'), //
    BOX_data = FOUR_CHAR_INT('d', 'a', 't', 'a'), //

    // these from http://lists.mplayerhq.hu/pipermail/ffmpeg-devel/2008-September/053151.html
    BOX_albm = FOUR_CHAR_INT('a', 'l', 'b', 'm'), // album
    BOX_yrrc = FOUR_CHAR_INT('y', 'r', 'r', 'c')  // album
};

// Video track : 'vide'
#define MP4E_HANDLER_TYPE_VIDE 0x76696465
// Audio track : 'soun'
#define MP4E_HANDLER_TYPE_SOUN 0x736F756E
// General MPEG-4 systems streams (without specific handler).
// Used for private stream, as suggested in http://www.mp4ra.org/handler.html
#define MP4E_HANDLER_TYPE_GESM 0x6765736D

/**
 * @brief struct sample
 * @param boxsize_t size
 * @param boxsize_t offset
 * @param unsigned duration
 * @param unsigned flag_random_access
 *
 */
typedef struct
{
    boxsize_t size;
    boxsize_t offset;
    unsigned duration;
    unsigned flag_random_access;
} sample_t;

typedef struct
{
    unsigned char *data;
    int bytes;
    int capacity;
} minimp4_vector_t;

typedef struct
{
    MP4E_track_t info;
    minimp4_vector_t smpl; // sample descriptor
    minimp4_vector_t pending_sample;

    minimp4_vector_t vsps; // or dsi for audio
    minimp4_vector_t vpps; // not used for audio
    minimp4_vector_t vvps; // used for HEVC

} track_t;

typedef struct MP4E_mux_tag
{
    minimp4_vector_t tracks;

    int64_t write_pos;
    int (*write_callback)(int64_t offset, const void *buffer, size_t size, void *token);
    void *token;
    char *text_comment;

    int sequential_mode_flag; // sequential mode
    int enable_fragmentation; // flag, indicating streaming-friendly 'fragmentation' mode
    int fragments_count;      // # of fragments in 'fragmentation' mode

} MP4E_mux_t;

static const unsigned char box_ftyp[] = {
#if 1
    0,
    0,
    0,
    0x18,
    'f',
    't',
    'y',
    'p',
    'm',
    'p',
    '4',
    '2',
    0,
    0,
    0,
    0,
    'm',
    'p',
    '4',
    '2',
    'i',
    's',
    'o',
    'm',
#else
    // as in ffmpeg
    0,
    0,
    0,
    0x20,
    'f',
    't',
    'y',
    'p',
    'i',
    's',
    'o',
    'm',
    0,
    0,
    2,
    0,
    'm',
    'p',
    '4',
    '1',
    'i',
    's',
    'o',
    'm',
    'i',
    's',
    'o',
    '2',
    'a',
    'v',
    'c',
    '1',
#endif
};

/**
 *   Endian-independent byte-write macros
 */
#define WR(x, n) *p++ = (unsigned char)((x) >> 8 * n)
#define WRITE_1(x) WR(x, 0);
#define WRITE_2(x) \
    WR(x, 1);      \
    WR(x, 0);
#define WRITE_3(x) \
    WR(x, 2);      \
    WR(x, 1);      \
    WR(x, 0);
#define WRITE_4(x) \
    WR(x, 3);      \
    WR(x, 2);      \
    WR(x, 1);      \
    WR(x, 0);
#define WR4(p, x)                  \
    (p)[0] = (char)((x) >> 8 * 3); \
    (p)[1] = (char)((x) >> 8 * 2); \
    (p)[2] = (char)((x) >> 8 * 1); \
    (p)[3] = (char)((x));

// Finish atom: update atom size field
#define END_ATOM \
    --stack;     \
    WR4((unsigned char *)*stack, p - *stack);

// Initiate atom: save position of size field on stack
#define ATOM(x)   \
    *stack++ = p; \
    p += 4;       \
    WRITE_4(x);

// Atom with 'FullAtomVersionFlags' field
#define ATOM_FULL(x, flag) \
    ATOM(x);               \
    WRITE_4(flag);

#define ERR(func)       \
    {                   \
        int err = func; \
        if (err)        \
            return err; \
    }

#endif // MP4D_PRINT_INFO_SUPPORTED
