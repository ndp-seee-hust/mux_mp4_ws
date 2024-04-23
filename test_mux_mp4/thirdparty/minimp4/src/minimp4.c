#include "minimp4.h"

/**
    Allocate vector with given size, return 1 on success, 0 on fail
*/
static int minimp4_vector_init(minimp4_vector_t *h, int capacity)
{
    LOG_INFO("Allocate vector with given size");
    h->bytes = 0;
    h->capacity = capacity;
    h->data = capacity ? (unsigned char *)malloc(capacity) : NULL;
    return !capacity || !!h->data;
}

/**
    Deallocates vector memory
*/
static void minimp4_vector_reset(minimp4_vector_t *h)
{
    LOG_INFO("Deallocates vector memory");
    if (h->data)
        free(h->data);
    memset(h, 0, sizeof(minimp4_vector_t));
}

/**
    Reallocate vector memory to the given size
*/
static int minimp4_vector_grow(minimp4_vector_t *h, int bytes)
{
    LOG_INFO("Reallocate vector memory to the given size");
    void *p;
    int new_size = h->capacity * 2 + 1024;
    if (new_size < h->capacity + bytes)
        new_size = h->capacity + bytes + 1024;
    p = realloc(h->data, new_size);
    if (!p)
        return 0;
    h->data = (unsigned char *)p;
    h->capacity = new_size;
    return 1;
}

/**
    Allocates given number of bytes at the end of vector data, increasing
    vector memory if necessary.
    Return allocated memory.
*/
static unsigned char *minimp4_vector_alloc_tail(minimp4_vector_t *h, int bytes)
{
    // LOG_INFO("Allocates given number of bytes at the end of vector data, increasing vector memory if necessary");
    unsigned char *p;
    if (!h->data && !minimp4_vector_init(h, 2 * bytes + 1024))
        return NULL;
    if ((h->capacity - h->bytes) < bytes && !minimp4_vector_grow(h, bytes))
        return NULL;
    assert(h->data);
    assert((h->capacity - h->bytes) >= bytes);
    p = h->data + h->bytes;
    h->bytes += bytes;
    return p;
}

/**
    Append data to the end of the vector (accumulate ot enqueue)
*/
static unsigned char *minimp4_vector_put(minimp4_vector_t *h, const void *buf, int bytes)
{
    // LOG_INFO("Append data to the end of the vector (accumulate ot enqueue)");
    unsigned char *tail = minimp4_vector_alloc_tail(h, bytes);
    if (tail)
        memcpy(tail, buf, bytes);
    return tail;
}

/**
 * @brief Allocates and initialize mp4 multiplexer
 *   return multiplexor handle on success; NULL on failure
 *
 * @param sequential_mode_flag
 * @param enable_fragmentation
 * @param token
 * @param write_callback
 * @return MP4E_mux_t*
 */
MP4E_mux_t *MP4E_open(int sequential_mode_flag, int enable_fragmentation, void *token,
                      int (*write_callback)(int64_t offset, const void *buffer, size_t size, void *token))
{
    LOG_INFO("Allocates and initialize mp4 multiplexer return multiplexor handle on success; NULL on failure");
    if (write_callback(0, box_ftyp, sizeof(box_ftyp), token)) // Write fixed header: 'ftyp' box
        return 0;
    MP4E_mux_t *mux = (MP4E_mux_t *)malloc(sizeof(MP4E_mux_t));
    if (!mux)
        return mux;
    mux->sequential_mode_flag = sequential_mode_flag || enable_fragmentation;
    mux->enable_fragmentation = enable_fragmentation;
    mux->fragments_count = 0;
    mux->write_callback = write_callback;
    mux->token = token;
    mux->text_comment = NULL;
    mux->write_pos = sizeof(box_ftyp);

    if (!mux->sequential_mode_flag)
    { // Write filler, which would be updated later
        if (mux->write_callback(mux->write_pos, box_ftyp, 8, mux->token))
        {
            free(mux);
            return 0;
        }
        mux->write_pos += 16; // box_ftyp + box_free for 32bit or 64bit size encoding
    }
    minimp4_vector_init(&mux->tracks, 2 * sizeof(track_t));
    return mux;
}

/**
 * @brief Add new track
 *
 * @param MP4E_mux_t mux
 * @param MP4E_track_t track_data
 * @return int
 */
int MP4E_add_track(MP4E_mux_t *mux, const MP4E_track_t *track_data)
{
    LOG_INFO("Add new track");
    track_t *tr;
    int ntr = mux->tracks.bytes / sizeof(track_t);

    if (!mux || !track_data)
        return MP4E_STATUS_BAD_ARGUMENTS;

    tr = (track_t *)minimp4_vector_alloc_tail(&mux->tracks, sizeof(track_t));
    if (!tr)
        return MP4E_STATUS_NO_MEMORY;
    memset(tr, 0, sizeof(track_t));
    memcpy(&tr->info, track_data, sizeof(*track_data));
    if (!minimp4_vector_init(&tr->smpl, 256))
        return MP4E_STATUS_NO_MEMORY;
    minimp4_vector_init(&tr->vsps, 0);
    minimp4_vector_init(&tr->vpps, 0);
    minimp4_vector_init(&tr->pending_sample, 0);
    return ntr;
}

/**
 * @brief next dsi
 *
 * @param const_unsigned_char *p
 * @param const_unsigned_char *end
 * @param int bytes
 * @return const unsigned char*
 */
static const unsigned char *next_dsi(const unsigned char *p, const unsigned char *end, int *bytes)
{
    LOG_INFO("Next dsi");
    if (p < end + 2)
    {
        *bytes = p[0] * 256 + p[1];
        return p + 2;
    }
    else
        return NULL;
}

/**
 * @brief Append mem
 *
 * @param minimp4_vector_t *v
 * @param void mem
 * @param int bytes
 * @return int
 */
static int append_mem(minimp4_vector_t *v, const void *mem, int bytes)
{
    LOG_INFO("Append mem");
    int i;
    unsigned char size[2];
    const unsigned char *p = v->data;
    for (i = 0; i + 2 < v->bytes;)
    {
        int cb = p[i] * 256 + p[i + 1];
        if (cb == bytes && !memcmp(p + i + 2, mem, cb))
            return 1;
        i += 2 + cb;
    }
    size[0] = bytes >> 8;
    size[1] = bytes;
    return minimp4_vector_put(v, size, 2) && minimp4_vector_put(v, mem, bytes);
}

/**
 * @brief items_count
 *
 * @param minimp4_vector_t v
 * @return int
 */
static int items_count(minimp4_vector_t *v)
{
    LOG_INFO("items count");
    int i, count = 0;
    const unsigned char *p = v->data;
    for (i = 0; i + 2 < v->bytes;)
    {
        int cb = p[i] * 256 + p[i + 1];
        count++;
        i += 2 + cb;
    }
    return count;
}

/**
 * @brief MP4E set dsi
 *
 * @param MP4E_mux_t mux
 * @param int track_id
 * @param void *dsi
 * @param int bytes
 * @return int
 */
int MP4E_set_dsi(MP4E_mux_t *mux, int track_id, const void *dsi, int bytes)
{
    LOG_INFO("MP$E set dsi");
    track_t *tr = ((track_t *)mux->tracks.data) + track_id;
    assert(tr->info.track_media_kind == e_audio ||
           tr->info.track_media_kind == e_private);
    if (tr->vsps.bytes)
        return MP4E_STATUS_ONLY_ONE_DSI_ALLOWED; // only one DSI allowed
    return append_mem(&tr->vsps, dsi, bytes) ? MP4E_STATUS_OK : MP4E_STATUS_NO_MEMORY;
}

/**
 * @brief MP4E set vps
 *
 * @param MP4E_mux_t mux
 * @param int track_id
 * @param const *vps
 * @param int bytes
 * @return int
 */
int MP4E_set_vps(MP4E_mux_t *mux, int track_id, const void *vps, int bytes)
{
    LOG_INFO("MP4E set vps");
    track_t *tr = ((track_t *)mux->tracks.data) + track_id;
    assert(tr->info.track_media_kind == e_video);
    return append_mem(&tr->vvps, vps, bytes) ? MP4E_STATUS_OK : MP4E_STATUS_NO_MEMORY;
}

/**
 * @brief MP4E set sps
 *
 * @param mux
 * @param track_id
 * @param sps
 * @param bytes
 * @return int
 */
int MP4E_set_sps(MP4E_mux_t *mux, int track_id, const void *sps, int bytes)
{
    if (mux == NULL || track_id < 0 || sps == NULL || bytes < 0)
    {
        LOG_ERROR("Invalid params");
        return MP4E_STATUS_NO_MEMORY;
    }

    LOG_INFO("MP4E set sps");
    track_t *tr = ((track_t *)mux->tracks.data) + track_id;
    assert(tr->info.track_media_kind == e_video);
    return append_mem(&tr->vsps, sps, bytes) ? MP4E_STATUS_OK : MP4E_STATUS_NO_MEMORY;
}

/**
 * @brief MP4E set pps
 *
 * @param MP4E_mux_t *mux
 * @param int track_id
 * @param void *pps
 * @param int bytes
 * @return int
 */
int MP4E_set_pps(MP4E_mux_t *mux, int track_id, const void *pps, int bytes)
{
    track_t *tr = ((track_t *)mux->tracks.data) + track_id;
    assert(tr->info.track_media_kind == e_video);
    return append_mem(&tr->vpps, pps, bytes) ? MP4E_STATUS_OK : MP4E_STATUS_NO_MEMORY;
}

/**
 * @brief Get the duration object
 *
 * @param track_t *tr
 * @return unsigned int
 */
static unsigned get_duration(const track_t *tr)
{
    unsigned i, sum_duration = 0;
    const sample_t *s = (const sample_t *)tr->smpl.data;
    for (i = 0; i < tr->smpl.bytes / sizeof(sample_t); i++)
    {
        sum_duration += s[i].duration;
    }
    return sum_duration;
}

/**
 * @brief Write pending data
 *
 * @param MP4E_mux_t *mux
 * @param track_t *tr
 * @return int
 */
static int write_pending_data(MP4E_mux_t *mux, track_t *tr)
{
    // if have pending sample && have at least one sample in the index
    if (tr->pending_sample.bytes > 0 && tr->smpl.bytes >= sizeof(sample_t))
    {
        // Complete pending sample
        sample_t *smpl_desc;
        unsigned char base[8], *p = base;

        assert(mux->sequential_mode_flag);

        // Write each sample to a separate atom
        assert(mux->sequential_mode_flag); // Separate atom needed for sequential_mode only
        WRITE_4(tr->pending_sample.bytes + 8);
        WRITE_4(BOX_mdat);
        ERR(mux->write_callback(mux->write_pos, base, p - base, mux->token));
        mux->write_pos += p - base;

        // Update sample descriptor with size and offset
        smpl_desc = ((sample_t *)minimp4_vector_alloc_tail(&tr->smpl, 0)) - 1;
        smpl_desc->size = tr->pending_sample.bytes;
        smpl_desc->offset = (boxsize_t)mux->write_pos;

        // Write data
        ERR(mux->write_callback(mux->write_pos, tr->pending_sample.data, tr->pending_sample.bytes, mux->token));
        mux->write_pos += tr->pending_sample.bytes;

        // reset buffer
        tr->pending_sample.bytes = 0;
    }
    return MP4E_STATUS_OK;
}

/**
 * @brief Add sample descriptor
 *
 * @param MP4E_mux_t *mux
 * @param track_t tr
 * @param int data_bytes
 * @param int duration
 * @param int kind
 * @return int
 */
static int add_sample_descriptor(MP4E_mux_t *mux, track_t *tr, int data_bytes, int duration, int kind)
{
    sample_t smp;
    smp.size = data_bytes;
    smp.offset = (boxsize_t)mux->write_pos;
    smp.duration = (duration ? duration : tr->info.default_duration);
    smp.flag_random_access = (kind == MP4E_SAMPLE_RANDOM_ACCESS);
    return NULL != minimp4_vector_put(&tr->smpl, &smp, sizeof(sample_t));
}

static int mp4e_flush_index(MP4E_mux_t *mux);

/**
 * @brief Write Movie Fragment: 'moof' box
 * @param MP4E_mux_t *mux
 * @param int track_num
 * @param int data_bytes
 * @param int duration
 * @param int kind
 *
 */
static int mp4e_write_fragment_header(MP4E_mux_t *mux, int track_num, int data_bytes, int duration, int kind
#if MP4D_TFDT_SUPPORT
                                      ,
                                      uint64_t timestamp
#endif
)
{
    LOG_INFO("MP4E write fragment header");
    unsigned char base[888], *p = base;
    unsigned char *stack_base[20]; // atoms nesting stack
    unsigned char **stack = stack_base;
    unsigned char *pdata_offset;
    unsigned flags;
    enum
    {
        default_sample_duration_present = 0x000008,
        default_sample_flags_present = 0x000020,
    } e;

    track_t *tr = ((track_t *)mux->tracks.data) + track_num;

    ATOM(BOX_moof)
    ATOM_FULL(BOX_mfhd, 0)
    WRITE_4(mux->fragments_count); // start from 1
    END_ATOM
    ATOM(BOX_traf)
    flags = 0;
    if (tr->info.track_media_kind == e_video)
        flags |= 0x20; // default-sample-flags-present
    else
        flags |= 0x08; // default-sample-duration-present
    flags = (tr->info.track_media_kind == e_video) ? 0x20020 : 0x20008;

    ATOM_FULL(BOX_tfhd, flags)
    WRITE_4(track_num + 1); // track_ID
    if (tr->info.track_media_kind == e_video)
    {
        WRITE_4(0x1010000); // default_sample_flags
    }
    else
    {
        WRITE_4(duration);
    }
    END_ATOM
#if MP4D_TFDT_SUPPORT
    ATOM_FULL(BOX_tfdt, 0x01000000)  // version 1
    WRITE_4(timestamp >> 32);        // upper timestamp
    WRITE_4(timestamp & 0xffffffff); // lower timestamp
    END_ATOM
#endif
    if (tr->info.track_media_kind == e_audio)
    {
        flags = 0;
        flags |= 0x001; // data-offset-present
        flags |= 0x200; // sample-size-present
        ATOM_FULL(BOX_trun, flags)
        WRITE_4(1); // sample_count
        pdata_offset = p;
        p += 4;              // save ptr to data_offset
        WRITE_4(data_bytes); // sample_size
        END_ATOM
    }
    else if (kind == MP4E_SAMPLE_RANDOM_ACCESS)
    {
        flags = 0;
        flags |= 0x001; // data-offset-present
        flags |= 0x004; // first-sample-flags-present
        flags |= 0x100; // sample-duration-present
        flags |= 0x200; // sample-size-present
        ATOM_FULL(BOX_trun, flags)
        WRITE_4(1); // sample_count
        pdata_offset = p;
        p += 4;              // save ptr to data_offset
        WRITE_4(0x2000000);  // first_sample_flags
        WRITE_4(duration);   // sample_duration
        WRITE_4(data_bytes); // sample_size
        END_ATOM
    }
    else
    {
        flags = 0;
        flags |= 0x001; // data-offset-present
        flags |= 0x100; // sample-duration-present
        flags |= 0x200; // sample-size-present
        ATOM_FULL(BOX_trun, flags)
        WRITE_4(1); // sample_count
        pdata_offset = p;
        p += 4;              // save ptr to data_offset
        WRITE_4(duration);   // sample_duration
        WRITE_4(data_bytes); // sample_size
        END_ATOM
    }
    END_ATOM
    END_ATOM
    WR4(pdata_offset, (p - base) + 8);

    ERR(mux->write_callback(mux->write_pos, base, p - base, mux->token));
    mux->write_pos += p - base;
    return MP4E_STATUS_OK;
}

/**
 * @brief MP4E write mdat box
 *
 * @param MP4E_mux_t *mux
 * @param uint32_t size
 * @return int
 */
static int mp4e_write_mdat_box(MP4E_mux_t *mux, uint32_t size)
{
    LOG_INFO("MP4E write mdat box");
    unsigned char base[8], *p = base;
    WRITE_4(size);
    WRITE_4(BOX_mdat);
    ERR(mux->write_callback(mux->write_pos, base, p - base, mux->token));
    mux->write_pos += p - base;
    return MP4E_STATUS_OK;
}

/**
 * @brief new sample to specified track
 * @param MP4E_mux_t *mux
 * @param int track_num
 * @param void *data
 * @param int data_bytes
 * @param int duration
 * @param int kind
 */
int MP4E_put_sample(MP4E_mux_t *mux, int track_num, const void *data, int data_bytes, int duration, int kind)
{
    LOG_INFO("MP4E put sample");
    track_t *tr;
    if (!mux || !data)
        return MP4E_STATUS_BAD_ARGUMENTS;
    tr = ((track_t *)mux->tracks.data) + track_num;

    if (mux->enable_fragmentation)
    {
#if MP4D_TFDT_SUPPORT
        // NOTE: assume a constant `duration` to calculate current timestamp
        uint64_t timestamp = (uint64_t)mux->fragments_count * duration;
#endif
        if (!mux->fragments_count++)
            ERR(mp4e_flush_index(mux)); // write file headers before 1st sample
// write MOOF + MDAT + sample data
#if MP4D_TFDT_SUPPORT
        ERR(mp4e_write_fragment_header(mux, track_num, data_bytes, duration, kind, timestamp));
#else
        ERR(mp4e_write_fragment_header(mux, track_num, data_bytes, duration, kind));
#endif
        // write MDAT box for each sample
        ERR(mp4e_write_mdat_box(mux, data_bytes + 8));
        ERR(mux->write_callback(mux->write_pos, data, data_bytes, mux->token));
        mux->write_pos += data_bytes;
        return MP4E_STATUS_OK;
    }

    if (kind != MP4E_SAMPLE_CONTINUATION)
    {
        if (mux->sequential_mode_flag)
            ERR(write_pending_data(mux, tr));
        if (!add_sample_descriptor(mux, tr, data_bytes, duration, kind))
            return MP4E_STATUS_NO_MEMORY;
    }
    else
    {
        if (!mux->sequential_mode_flag)
        {
            sample_t *smpl_desc;
            if (tr->smpl.bytes < sizeof(sample_t))
                return MP4E_STATUS_NO_MEMORY; // write continuation, but there are no samples in the index
            // Accumulate size of the continuation in the sample descriptor
            smpl_desc = (sample_t *)(tr->smpl.data + tr->smpl.bytes) - 1;
            smpl_desc->size += data_bytes;
        }
    }

    if (mux->sequential_mode_flag)
    {
        if (!minimp4_vector_put(&tr->pending_sample, data, data_bytes))
            return MP4E_STATUS_NO_MEMORY;
    }
    else
    {
        ERR(mux->write_callback(mux->write_pos, data, data_bytes, mux->token));
        mux->write_pos += data_bytes;
    }
    return MP4E_STATUS_OK;
}

/**
 *   calculate size of length field of OD box
 */
static int od_size_of_size(int size)
{
    LOG_INFO("calculate size of length field of OD box");
    int i, size_of_size = 1;
    for (i = size; i > 0x7F; i -= 0x7F)
        size_of_size++;
    return size_of_size;
}

/**
 *   Add or remove MP4 file text comment according to Apple specs:
 *   https://developer.apple.com/library/mac/documentation/QuickTime/QTFF/Metadata/Metadata.html#//apple_ref/doc/uid/TP40000939-CH1-SW1
 *   http://atomicparsley.sourceforge.net/mpeg-4files.html
 *   note that ISO did not specify comment format.
 */
int MP4E_set_text_comment(MP4E_mux_t *mux, const char *comment)
{
    LOG_INFO("MP4E set text comment");
    if (!mux || !comment)
        return MP4E_STATUS_BAD_ARGUMENTS;
    if (mux->text_comment)
        free(mux->text_comment);
    mux->text_comment = strdup(comment);
    if (!mux->text_comment)
        return MP4E_STATUS_NO_MEMORY;
    return MP4E_STATUS_OK;
}

/**
 *   Write file index 'moov' box with all its boxes and indexes
 */
static int mp4e_flush_index(MP4E_mux_t *mux)
{
    LOG_INFO("Write file index 'moov' box with all its boxes and indexes");
    unsigned char *stack_base[20]; // atoms nesting stack
    unsigned char **stack = stack_base;
    unsigned char *base, *p;
    unsigned int ntr, index_bytes, ntracks = mux->tracks.bytes / sizeof(track_t);
    int i, err;

    // How much memory needed for indexes
    // Experimental data:
    // file with 1 track = 560 bytes
    // file with 2 tracks = 972 bytes
    // track size = 412 bytes;
    // file header size = 148 bytes
#define FILE_HEADER_BYTES 256
#define TRACK_HEADER_BYTES 512
    index_bytes = FILE_HEADER_BYTES;
    if (mux->text_comment)
        index_bytes += 128 + strlen(mux->text_comment);
    for (ntr = 0; ntr < ntracks; ntr++)
    {
        track_t *tr = ((track_t *)mux->tracks.data) + ntr;
        index_bytes += TRACK_HEADER_BYTES; // fixed amount (implementation-dependent)
        // may need extra 4 bytes for duration field + 4 bytes for worst-case random access box
        index_bytes += tr->smpl.bytes * (sizeof(sample_t) + 4 + 4) / sizeof(sample_t);
        index_bytes += tr->vsps.bytes;
        index_bytes += tr->vpps.bytes;

        ERR(write_pending_data(mux, tr));
    }

    base = (unsigned char *)malloc(index_bytes);
    if (!base)
        return MP4E_STATUS_NO_MEMORY;
    p = base;

    if (!mux->sequential_mode_flag)
    {
        // update size of mdat box.
        // One of 2 points, which requires random file access.
        // Second is optional duration update at beginning of file in fragmentation mode.
        // This can be avoided using "till eof" size code, but in this case indexes must be
        // written before the mdat....
        int64_t size = mux->write_pos - sizeof(box_ftyp);
        const int64_t size_limit = (int64_t)(uint64_t)0xfffffffe;
        if (size > size_limit)
        {
            WRITE_4(1);
            WRITE_4(BOX_mdat);
            WRITE_4((size >> 32) & 0xffffffff);
            WRITE_4(size & 0xffffffff);
        }
        else
        {
            WRITE_4(8);
            WRITE_4(BOX_free);
            WRITE_4(size - 8);
            WRITE_4(BOX_mdat);
        }
        ERR(mux->write_callback(sizeof(box_ftyp), base, p - base, mux->token));
        p = base;
    }

    // Write index atoms; order taken from Table 1 of [1]
#define MOOV_TIMESCALE 1000
    ATOM(BOX_moov);
    ATOM_FULL(BOX_mvhd, 0);
    WRITE_4(0); // creation_time
    WRITE_4(0); // modification_time

    if (ntracks)
    {
        track_t *tr = ((track_t *)mux->tracks.data) + 0; // take 1st track
        unsigned duration = get_duration(tr);
        duration = (unsigned)(duration * 1LL * MOOV_TIMESCALE / tr->info.time_scale);
        WRITE_4(MOOV_TIMESCALE); // duration
        WRITE_4(duration);       // duration
    }

    WRITE_4(0x00010000); // rate
    WRITE_2(0x0100);     // volume
    WRITE_2(0);          // reserved
    WRITE_4(0);          // reserved
    WRITE_4(0);          // reserved

    // matrix[9]
    WRITE_4(0x00010000);
    WRITE_4(0);
    WRITE_4(0);
    WRITE_4(0);
    WRITE_4(0x00010000);
    WRITE_4(0);
    WRITE_4(0);
    WRITE_4(0);
    WRITE_4(0x40000000);

    // pre_defined[6]
    WRITE_4(0);
    WRITE_4(0);
    WRITE_4(0);
    WRITE_4(0);
    WRITE_4(0);
    WRITE_4(0);

    // next_track_ID is a non-zero integer that indicates a value to use for the track ID of the next track to be
    // added to this presentation. Zero is not a valid track ID value. The value of next_track_ID shall be
    // larger than the largest track-ID in use.
    WRITE_4(ntracks + 1);
    END_ATOM;

    for (ntr = 0; ntr < ntracks; ntr++)
    {
        track_t *tr = ((track_t *)mux->tracks.data) + ntr;
        unsigned duration = get_duration(tr);
        int samples_count = tr->smpl.bytes / sizeof(sample_t);
        const sample_t *sample = (const sample_t *)tr->smpl.data;
        unsigned handler_type;
        const char *handler_ascii = NULL;

        if (mux->enable_fragmentation)
            samples_count = 0;
        else if (samples_count <= 0)
            continue; // skip empty track

        switch (tr->info.track_media_kind)
        {
        case e_audio:
            handler_type = MP4E_HANDLER_TYPE_SOUN;
            handler_ascii = "SoundHandler";
            break;
        case e_video:
            handler_type = MP4E_HANDLER_TYPE_VIDE;
            handler_ascii = "VideoHandler";
            break;
        case e_private:
            handler_type = MP4E_HANDLER_TYPE_GESM;
            break;
        default:
            return MP4E_STATUS_BAD_ARGUMENTS;
        }

        ATOM(BOX_trak);
        ATOM_FULL(BOX_tkhd, 7); // flag: 1=trak enabled; 2=track in movie; 4=track in preview
        WRITE_4(0);             // creation_time
        WRITE_4(0);             // modification_time
        WRITE_4(ntr + 1);       // track_ID
        WRITE_4(0);             // reserved
        WRITE_4((unsigned)(duration * 1LL * MOOV_TIMESCALE / tr->info.time_scale));
        WRITE_4(0);
        WRITE_4(0);      // reserved[2]
        WRITE_2(0);      // layer
        WRITE_2(0);      // alternate_group
        WRITE_2(0x0100); // volume {if track_is_audio 0x0100 else 0};
        WRITE_2(0);      // reserved

        // matrix[9]
        WRITE_4(0x00010000);
        WRITE_4(0);
        WRITE_4(0);
        WRITE_4(0);
        WRITE_4(0x00010000);
        WRITE_4(0);
        WRITE_4(0);
        WRITE_4(0);
        WRITE_4(0x40000000);

        if (tr->info.track_media_kind == e_audio || tr->info.track_media_kind == e_private)
        {
            WRITE_4(0); // width
            WRITE_4(0); // height
        }
        else
        {
            WRITE_4(tr->info.u.v.width * 0x10000);  // width
            WRITE_4(tr->info.u.v.height * 0x10000); // height
        }
        END_ATOM;

        ATOM(BOX_mdia);
        ATOM_FULL(BOX_mdhd, 0);
        WRITE_4(0); // creation_time
        WRITE_4(0); // modification_time
        WRITE_4(tr->info.time_scale);
        WRITE_4(duration); // duration
        {
            int lang_code = ((tr->info.language[0] & 31) << 10) | ((tr->info.language[1] & 31) << 5) | (tr->info.language[2] & 31);
            WRITE_2(lang_code); // language
        }
        WRITE_2(0); // pre_defined
        END_ATOM;

        ATOM_FULL(BOX_hdlr, 0);
        WRITE_4(0);            // pre_defined
        WRITE_4(handler_type); // handler_type
        WRITE_4(0);
        WRITE_4(0);
        WRITE_4(0); // reserved[3]
        // name is a null-terminated string in UTF-8 characters which gives a human-readable name for the track type (for debugging and inspection purposes).
        // set mdia hdlr name field to what quicktime uses.
        // Sony smartphone may fail to decode short files w/o handler name
        if (handler_ascii)
        {
            for (i = 0; i < (int)strlen(handler_ascii) + 1; i++)
            {
                WRITE_1(handler_ascii[i]);
            }
        }
        else
        {
            WRITE_4(0);
        }
        END_ATOM;

        ATOM(BOX_minf);

        if (tr->info.track_media_kind == e_audio)
        {
            // Sound Media Header Box
            ATOM_FULL(BOX_smhd, 0);
            WRITE_2(0); // balance
            WRITE_2(0); // reserved
            END_ATOM;
        }
        if (tr->info.track_media_kind == e_video)
        {
            // mandatory Video Media Header Box
            ATOM_FULL(BOX_vmhd, 1);
            WRITE_2(0); // graphicsmode
            WRITE_2(0);
            WRITE_2(0);
            WRITE_2(0); // opcolor[3]
            END_ATOM;
        }

        ATOM(BOX_dinf);
        ATOM_FULL(BOX_dref, 0);
        WRITE_4(1); // entry_count
        // If the flag is set indicating that the data is in the same file as this box, then no string (not even an empty one)
        // shall be supplied in the entry field.

        // ASP the correct way to avoid supply the string, is to use flag 1
        // otherwise ISO reference demux crashes
        ATOM_FULL(BOX_url, 1);
        END_ATOM;
        END_ATOM;
        END_ATOM;

        ATOM(BOX_stbl);
        ATOM_FULL(BOX_stsd, 0);
        WRITE_4(1); // entry_count;

        if (tr->info.track_media_kind == e_audio || tr->info.track_media_kind == e_private)
        {
            // AudioSampleEntry() assume MP4E_HANDLER_TYPE_SOUN
            if (tr->info.track_media_kind == e_audio)
            {
                ATOM(BOX_mp4a);
            }
            else
            {
                ATOM(BOX_mp4s);
            }

            // SampleEntry
            WRITE_4(0);
            WRITE_2(0); // reserved[6]
            WRITE_2(1); // data_reference_index; - this is a tag for descriptor below

            if (tr->info.track_media_kind == e_audio)
            {
                // AudioSampleEntry
                WRITE_4(0);
                WRITE_4(0);                           // reserved[2]
                WRITE_2(tr->info.u.a.channelcount);   // channelcount
                WRITE_2(16);                          // samplesize
                WRITE_4(0);                           // pre_defined+reserved
                WRITE_4((tr->info.time_scale << 16)); // samplerate == = {timescale of media}<<16;
            }

            ATOM_FULL(BOX_esds, 0);
            if (tr->vsps.bytes > 0)
            {
                int dsi_bytes = tr->vsps.bytes - 2; //  - two bytes size field
                int dsi_size_size = od_size_of_size(dsi_bytes);
                int dcd_bytes = dsi_bytes + dsi_size_size + 1 + (1 + 1 + 3 + 4 + 4);
                int dcd_size_size = od_size_of_size(dcd_bytes);
                int esd_bytes = dcd_bytes + dcd_size_size + 1 + 3;

#define WRITE_OD_LEN(size)     \
    if (size > 0x7F)           \
        do                     \
        {                      \
            size -= 0x7F;      \
            WRITE_1(0x00ff);   \
        } while (size > 0x7F); \
    WRITE_1(size)
                WRITE_1(3); // OD_ESD
                WRITE_OD_LEN(esd_bytes);
                WRITE_2(0); // ES_ID(2) // TODO - what is this?
                WRITE_1(0); // flags(1)

                WRITE_1(4); // OD_DCD
                WRITE_OD_LEN(dcd_bytes);
                if (tr->info.track_media_kind == e_audio)
                {
                    WRITE_1(MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3); // OD_DCD
                    WRITE_1(5 << 2);                                // stream_type == AudioStream
                }
                else
                {
                    // http://xhelmboyx.tripod.com/formats/mp4-layout.txt
                    WRITE_1(208);     // 208 = private video
                    WRITE_1(32 << 2); // stream_type == user private
                }
                WRITE_3(tr->info.u.a.channelcount * 6144 / 8); // bufferSizeDB in bytes, constant as in reference decoder
                WRITE_4(0);                                    // maxBitrate TODO
                WRITE_4(0);                                    // avg_bitrate_bps TODO

                WRITE_1(5); // OD_DSI
                WRITE_OD_LEN(dsi_bytes);
                for (i = 0; i < dsi_bytes; i++)
                {
                    WRITE_1(tr->vsps.data[2 + i]);
                }
            }
            END_ATOM;
            END_ATOM;
        }

        if (tr->info.track_media_kind == e_video && (MP4_OBJECT_TYPE_AVC == tr->info.object_type_indication || MP4_OBJECT_TYPE_HEVC == tr->info.object_type_indication))
        {
            int numOfSequenceParameterSets = items_count(&tr->vsps);
            int numOfPictureParameterSets = items_count(&tr->vpps);
            if (MP4_OBJECT_TYPE_AVC == tr->info.object_type_indication)
            {
                ATOM(BOX_avc1);
            }
            else
            {
                ATOM(BOX_hvc1);
            }
            // VisualSampleEntry  8.16.2
            // extends SampleEntry
            WRITE_2(0); // reserved
            WRITE_2(0); // reserved
            WRITE_2(0); // reserved
            WRITE_2(1); // data_reference_index

            WRITE_2(0); // pre_defined
            WRITE_2(0); // reserved
            WRITE_4(0); // pre_defined
            WRITE_4(0); // pre_defined
            WRITE_4(0); // pre_defined
            WRITE_2(tr->info.u.v.width);
            WRITE_2(tr->info.u.v.height);
            WRITE_4(0x00480000); // horizresolution = 72 dpi
            WRITE_4(0x00480000); // vertresolution  = 72 dpi
            WRITE_4(0);          // reserved
            WRITE_2(1);          // frame_count
            for (i = 0; i < 32; i++)
            {
                WRITE_1(0); //  compressorname
            }
            WRITE_2(24); // depth
            WRITE_2(-1); // pre_defined

            if (MP4_OBJECT_TYPE_AVC == tr->info.object_type_indication)
            {
                ATOM(BOX_avcC);
                // AVCDecoderConfigurationRecord 5.2.4.1.1
                WRITE_1(1); // configurationVersion
                WRITE_1(tr->vsps.data[2 + 1]);
                WRITE_1(tr->vsps.data[2 + 2]);
                WRITE_1(tr->vsps.data[2 + 3]);
                WRITE_1(255); // 0xfc + NALU_len - 1
                WRITE_1(0xe0 | numOfSequenceParameterSets);
                for (i = 0; i < tr->vsps.bytes; i++)
                {
                    WRITE_1(tr->vsps.data[i]);
                }
                WRITE_1(numOfPictureParameterSets);
                for (i = 0; i < tr->vpps.bytes; i++)
                {
                    WRITE_1(tr->vpps.data[i]);
                }
            }
            else
            {
                int numOfVPS = items_count(&tr->vpps);
                ATOM(BOX_hvcC);
                // TODO: read actual params from stream
                WRITE_1(1);          // configurationVersion
                WRITE_1(1);          // Profile Space (2), Tier (1), Profile (5)
                WRITE_4(0x60000000); // Profile Compatibility
                WRITE_2(0);          // progressive, interlaced, non packed constraint, frame only constraint flags
                WRITE_4(0);          // constraint indicator flags
                WRITE_1(0);          // level_idc
                WRITE_2(0xf000);     // Min Spatial Segmentation
                WRITE_1(0xfc);       // Parallelism Type
                WRITE_1(0xfc);       // Chroma Format
                WRITE_1(0xf8);       // Luma Depth
                WRITE_1(0xf8);       // Chroma Depth
                WRITE_2(0);          // Avg Frame Rate
                WRITE_1(3);          // ConstantFrameRate (2), NumTemporalLayers (3), TemporalIdNested (1), LengthSizeMinusOne (2)

                WRITE_1(3);                                // Num Of Arrays
                WRITE_1((1 << 7) | (HEVC_NAL_VPS & 0x3f)); // Array Completeness + NAL Unit Type
                WRITE_2(numOfVPS);
                for (i = 0; i < tr->vvps.bytes; i++)
                {
                    WRITE_1(tr->vvps.data[i]);
                }
                WRITE_1((1 << 7) | (HEVC_NAL_SPS & 0x3f));
                WRITE_2(numOfSequenceParameterSets);
                for (i = 0; i < tr->vsps.bytes; i++)
                {
                    WRITE_1(tr->vsps.data[i]);
                }
                WRITE_1((1 << 7) | (HEVC_NAL_PPS & 0x3f));
                WRITE_2(numOfPictureParameterSets);
                for (i = 0; i < tr->vpps.bytes; i++)
                {
                    WRITE_1(tr->vpps.data[i]);
                }
            }

            END_ATOM;
            END_ATOM;
        }
        END_ATOM;

        /************************************************************************/
        /*      indexes                                                         */
        /************************************************************************/

        // Time to Sample Box
        ATOM_FULL(BOX_stts, 0);
        {
            unsigned char *pentry_count = p;
            int cnt = 1, entry_count = 0;
            WRITE_4(0);
            for (i = 0; i < samples_count; i++, cnt++)
            {
                if (i == (samples_count - 1) || sample[i].duration != sample[i + 1].duration)
                {
                    WRITE_4(cnt);
                    WRITE_4(sample[i].duration);
                    cnt = 0;
                    entry_count++;
                }
            }
            WR4(pentry_count, entry_count);
        }
        END_ATOM;

        // Sample To Chunk Box
        ATOM_FULL(BOX_stsc, 0);
        if (mux->enable_fragmentation)
        {
            WRITE_4(0); // entry_count
        }
        else
        {
            WRITE_4(1); // entry_count
            WRITE_4(1); // first_chunk;
            WRITE_4(1); // samples_per_chunk;
            WRITE_4(1); // sample_description_index;
        }
        END_ATOM;

        // Sample Size Box
        ATOM_FULL(BOX_stsz, 0);
        WRITE_4(0);             // sample_size  If this field is set to 0, then the samples have different sizes, and those sizes
                                //  are stored in the sample size table.
        WRITE_4(samples_count); // sample_count;
        for (i = 0; i < samples_count; i++)
        {
            WRITE_4(sample[i].size);
        }
        END_ATOM;

        // Chunk Offset Box
        int is_64_bit = 0;
        if (samples_count && sample[samples_count - 1].offset > 0xffffffff)
            is_64_bit = 1;
        if (!is_64_bit)
        {
            ATOM_FULL(BOX_stco, 0);
            WRITE_4(samples_count);
            for (i = 0; i < samples_count; i++)
            {
                WRITE_4(sample[i].offset);
            }
        }
        else
        {
            ATOM_FULL(BOX_co64, 0);
            WRITE_4(samples_count);
            for (i = 0; i < samples_count; i++)
            {
                WRITE_4((sample[i].offset >> 32) & 0xffffffff);
                WRITE_4(sample[i].offset & 0xffffffff);
            }
        }
        END_ATOM;

        // Sync Sample Box
        {
            int ra_count = 0;
            for (i = 0; i < samples_count; i++)
            {
                ra_count += !!sample[i].flag_random_access;
            }
            if (ra_count != samples_count)
            {
                // If the sync sample box is not present, every sample is a random access point.
                ATOM_FULL(BOX_stss, 0);
                WRITE_4(ra_count);
                for (i = 0; i < samples_count; i++)
                {
                    if (sample[i].flag_random_access)
                    {
                        WRITE_4(i + 1);
                    }
                }
                END_ATOM;
            }
        }
        END_ATOM;
        END_ATOM;
        END_ATOM;
        END_ATOM;
    } // tracks loop

    if (mux->text_comment)
    {
        ATOM(BOX_udta);
        ATOM_FULL(BOX_meta, 0);
        ATOM_FULL(BOX_hdlr, 0);
        WRITE_4(0); // pre_defined
#define MP4E_HANDLER_TYPE_MDIR 0x6d646972
        WRITE_4(MP4E_HANDLER_TYPE_MDIR); // handler_type
        WRITE_4(0);
        WRITE_4(0);
        WRITE_4(0); // reserved[3]
        WRITE_4(0); // name is a null-terminated string in UTF-8 characters which gives a human-readable name for the track type (for debugging and inspection purposes).
        END_ATOM;
        ATOM(BOX_ilst);
        ATOM(BOX_ccmt);
        ATOM(BOX_data);
        WRITE_4(1); // type
        WRITE_4(0); // lang
        for (i = 0; i < (int)strlen(mux->text_comment) + 1; i++)
        {
            WRITE_1(mux->text_comment[i]);
        }
        END_ATOM;
        END_ATOM;
        END_ATOM;
        END_ATOM;
        END_ATOM;
    }

    if (mux->enable_fragmentation)
    {
        track_t *tr = ((track_t *)mux->tracks.data) + 0;
        uint32_t movie_duration = get_duration(tr);

        ATOM(BOX_mvex);
        ATOM_FULL(BOX_mehd, 0);
        WRITE_4(movie_duration); // duration
        END_ATOM;
        for (ntr = 0; ntr < ntracks; ntr++)
        {
            ATOM_FULL(BOX_trex, 0);
            WRITE_4(ntr + 1); // track_ID
            WRITE_4(1);       // default_sample_description_index
            WRITE_4(0);       // default_sample_duration
            WRITE_4(0);       // default_sample_size
            WRITE_4(0);       // default_sample_flags
            END_ATOM;
        }
        END_ATOM;
    }
    END_ATOM; // moov atom

    assert((unsigned)(p - base) <= index_bytes);

    err = mux->write_callback(mux->write_pos, base, p - base, mux->token);
    mux->write_pos += p - base;
    free(base);
    return err;
}

int MP4E_close(MP4E_mux_t *mux)
{
    LOG_INFO("MP4E close");
    int err = MP4E_STATUS_OK;
    unsigned ntr, ntracks;
    if (!mux)
        return MP4E_STATUS_BAD_ARGUMENTS;
    if (!mux->enable_fragmentation)
        err = mp4e_flush_index(mux);
    if (mux->text_comment)
        free(mux->text_comment);
    ntracks = mux->tracks.bytes / sizeof(track_t);
    for (ntr = 0; ntr < ntracks; ntr++)
    {
        track_t *tr = ((track_t *)mux->tracks.data) + ntr;
        minimp4_vector_reset(&tr->vsps);
        minimp4_vector_reset(&tr->vpps);
        minimp4_vector_reset(&tr->smpl);
        minimp4_vector_reset(&tr->pending_sample);
    }
    minimp4_vector_reset(&mux->tracks);
    free(mux);
    return err;
}

typedef uint32_t bs_item_t;
#define BS_BITS 32

typedef struct
{
    // Look-ahead bit cache: MSB aligned, 17 bits guaranteed, zero stuffing
    unsigned int cache;

    // Bit counter = 16 - (number of bits in wCache)
    // cache refilled when cache_free_bits >= 0
    int cache_free_bits;

    // Current read position
    const uint16_t *buf;

    // original data buffer
    const uint16_t *origin;

    // original data buffer length, bytes
    unsigned origin_bytes;
} bit_reader_t;

#define LOAD_SHORT(x) ((uint16_t)(x << 8) | (x >> 8))

static unsigned int show_bits(bit_reader_t *bs, int n)
{
    // LOG_INFO("show bits");
    unsigned int retval;
    assert(n > 0 && n <= 16);
    retval = (unsigned int)(bs->cache >> (32 - n));
    return retval;
}

static void flush_bits(bit_reader_t *bs, int n)
{
    // LOG_INFO("flush bits");
    assert(n >= 0 && n <= 16);
    bs->cache <<= n;
    bs->cache_free_bits += n;
    if (bs->cache_free_bits >= 0)
    {
        bs->cache |= ((uint32_t)LOAD_SHORT(*bs->buf)) << bs->cache_free_bits;
        bs->buf++;
        bs->cache_free_bits -= 16;
    }
}

static unsigned int get_bits(bit_reader_t *bs, int n)
{
    // LOG_INFO("get bits");
    unsigned int retval = show_bits(bs, n);
    flush_bits(bs, n);
    return retval;
}

static void set_pos_bits(bit_reader_t *bs, unsigned pos_bits)
{
    // LOG_INFO("set pos bits");
    assert((int)pos_bits >= 0);

    bs->buf = bs->origin + pos_bits / 16;
    bs->cache = 0;
    bs->cache_free_bits = 16;
    flush_bits(bs, 0);
    flush_bits(bs, pos_bits & 15);
}

static unsigned get_pos_bits(const bit_reader_t *bs)
{
    // LOG_INFO("get pos bits");
    //  Current bitbuffer position =
    //  position of next wobits in the internal buffer
    //  minus bs, available in bit cache wobits
    unsigned pos_bits = (unsigned)(bs->buf - bs->origin) * 16;
    pos_bits -= 16 - bs->cache_free_bits;
    assert((int)pos_bits >= 0);
    return pos_bits;
}

static int remaining_bits(const bit_reader_t *bs)
{
    // LOG_INFO("remaining bits");
    return bs->origin_bytes * 8 - get_pos_bits(bs);
}

static void init_bits(bit_reader_t *bs, const void *data, unsigned data_bytes)
{
    // LOG_INFO("init bits");
    bs->origin = (const uint16_t *)data;
    bs->origin_bytes = data_bytes;
    set_pos_bits(bs, 0);
}

#define GetBits(n) get_bits(bs, n)

/**
 *   Unsigned Golomb code, Nó thường được sử dụng trong việc nén dữ liệu hình ảnh
 *   và video, trong đó các giá trị không âm thường xuất hiện với tần suất cao.
 */
static int ue_bits(bit_reader_t *bs)
{
    // LOG_INFO("ue bits");
    int clz;
    int val;
    for (clz = 0; !get_bits(bs, 1); clz++)
    {
    }
    // get_bits(bs, clz + 1);
    val = (1 << clz) - 1 + (clz ? get_bits(bs, clz) : 0);
    return val;
}

#if MINIMP4_TRANSCODE_SPS_ID

/**
 * @brief Output bitstream
 * @param int shift // bit position in the cache
 * @param uint32_t cache;    // bit cache
 * @param bs_item_t *buf;    // current position
 * @param bs_item_t *origin; // initial position
 *
 */
typedef struct
{
    int shift;         // bit position in the cache
    uint32_t cache;    // bit cache
    bs_item_t *buf;    // current position
    bs_item_t *origin; // initial position
} bs_t;

#define SWAP32(x) (uint32_t)((((x) >> 24) & 0xFF) | (((x) >> 8) & 0xFF00) | (((x) << 8) & 0xFF0000) | ((x & 0xFF) << 24))

static void h264e_bs_put_bits(bs_t *bs, unsigned n, unsigned val)
{
    // LOG_INFO("h264e bitstream put bits");
    assert(!(val >> n));
    bs->shift -= n;
    assert((unsigned)n <= 32);
    if (bs->shift < 0)
    {
        assert(-bs->shift < 32);
        bs->cache |= val >> -bs->shift;
        *bs->buf++ = SWAP32(bs->cache);
        bs->shift = 32 + bs->shift;
        bs->cache = 0;
    }
    bs->cache |= val << bs->shift;
}

static void h264e_bs_flush(bs_t *bs)
{
    // LOG_INFO("h264e bitstream flush");
    *bs->buf = SWAP32(bs->cache);
}

static unsigned h264e_bs_get_pos_bits(const bs_t *bs)
{
    // LOG_INFO("h264e bitstream get pos bits");
    unsigned pos_bits = (unsigned)((bs->buf - bs->origin) * BS_BITS);
    pos_bits += BS_BITS - bs->shift;
    assert((int)pos_bits >= 0);
    return pos_bits;
}

static unsigned h264e_bs_byte_align(bs_t *bs)
{
    // LOG_INFO("h264e bitstream align");
    int pos = h264e_bs_get_pos_bits(bs);
    h264e_bs_put_bits(bs, -pos & 7, 0);
    return pos + (-pos & 7);
}

/**
 *   Golomb code
 *   0 => 1
 *   1 => 01 0
 *   2 => 01 1
 *   3 => 001 00
 *   4 => 001 01
 *
 *   [0]     => 1
 *   [1..2]  => 01x
 *   [3..6]  => 001xx
 *   [7..14] => 0001xxx
 *
 */
static void h264e_bs_put_golomb(bs_t *bs, unsigned val)
{
    // LOG_INFO("h264e bitstream put golomb");
    int size = 0;
    unsigned t = val + 1;
    do
    {
        size++;
    } while (t >>= 1);

    h264e_bs_put_bits(bs, 2 * size - 1, val + 1);
}

static void h264e_bs_init_bits(bs_t *bs, void *data)
{
    // LOG_INFO("h264e bitstream init bits");
    bs->origin = (bs_item_t *)data;
    bs->buf = bs->origin;
    bs->shift = BS_BITS;
    bs->cache = 0;
}

static int find_mem_cache(void *cache[], int cache_bytes[], int cache_size, void *mem, int bytes)
{
    // LOG_INFO("find mem cache");
    int i;
    if (!bytes)
        return -1;
    for (i = 0; i < cache_size; i++)
    {
        if (cache_bytes[i] == bytes && !memcmp(mem, cache[i], bytes))
            return i; // found
    }
    for (i = 0; i < cache_size; i++)
    {
        if (!cache_bytes[i])
        {
            cache[i] = malloc(bytes);
            if (cache[i])
            {
                memcpy(cache[i], mem, bytes);
                cache_bytes[i] = bytes;
            }
            return i; // put in
        }
    }
    return -1; // no room
}

/**
 * @brief 7.4.1.1. "Encapsulation of an SODB within an RBSP"
 *
 * @param char *dst
 * @param unsigned_char *src
 * @param int h264_data_bytes
 * @return int
 */
static int remove_nal_escapes(unsigned char *dst, const unsigned char *src, int h264_data_bytes)
{
    // LOG_INFO("remove nal escapes - Encapsulation of an SODB within an RBSP");
    int i = 0, j = 0, zero_cnt = 0;
    for (j = 0; j < h264_data_bytes; j++)
    {
        if (zero_cnt == 2 && src[j] <= 3)
        {
            if (src[j] == 3)
            {
                if (j == h264_data_bytes - 1)
                {
                    // cabac_zero_word: no action
                }
                else if (src[j + 1] <= 3)
                {
                    j++;
                    zero_cnt = 0;
                }
                else
                {
                    // TODO: assume end-of-nal
                    // return 0;
                }
            }
            else
                return 0;
        }
        dst[i++] = src[j];
        if (src[j])
            zero_cnt = 0;
        else
            zero_cnt++;
    }
    // while (--j > i) src[j] = 0;
    return i;
}

/**
 * @brief Put NAL escape codes to the output bitstream
 *
 * @param uint8_t d
 * @param uint8_t s
 * @param int n
 * @return int
 */
static int nal_put_esc(uint8_t *d, const uint8_t *s, int n)
{
    int i, j = 4, cntz = 0;
    d[0] = d[1] = d[2] = 0;
    d[3] = 1; // start code
    for (i = 0; i < n; i++)
    {
        uint8_t byte = *s++;
        if (cntz == 2 && byte <= 3)
        {
            d[j++] = 3;
            cntz = 0;
        }
        if (byte)
            cntz = 0;
        else
            cntz++;
        d[j++] = byte;
    }
    return j;
}

/**
 * @brief copy bits
 *
 * @param bit_reader_t bs
 * @param bs_t bd
 */
static void copy_bits(bit_reader_t *bs, bs_t *bd)
{
    // LOG_INFO("copy bits");
    unsigned cb, bits;
    int bit_count = remaining_bits(bs);
    while (bit_count > 7)
    {
        cb = MINIMP4_MIN(bit_count - 7, 8);
        bits = GetBits(cb);
        h264e_bs_put_bits(bd, cb, bits);
        bit_count -= cb;
    }

    // cut extra zeros after stop-bit
    bits = GetBits(bit_count);
    for (; bit_count && ~bits & 1; bit_count--)
    {
        bits >>= 1;
    }
    if (bit_count)
    {
        h264e_bs_put_bits(bd, bit_count, bits);
    }
}

/**
 * @brief change sps id
 *
 * @param bit_reader_t bs
 * @param bs_t bd
 * @param int new_id
 * @param int old_id
 * @return int
 */
static int change_sps_id(bit_reader_t *bs, bs_t *bd, int new_id, int *old_id)
{
    // LOG_INFO("change sps id");
    unsigned bits, sps_id, i, bytes;
    for (i = 0; i < 3; i++)
    {
        bits = GetBits(8);
        h264e_bs_put_bits(bd, 8, bits);
    }
    sps_id = ue_bits(bs); // max = 31

    *old_id = sps_id;
    sps_id = new_id;
    assert(sps_id <= 31);

    h264e_bs_put_golomb(bd, sps_id);
    copy_bits(bs, bd);

    bytes = h264e_bs_byte_align(bd) / 8;
    h264e_bs_flush(bd);
    return bytes;
}

/**
 * @brief patch pps
 *
 * @param h264_sps_id_patcher_t *h
 * @param bit_reader_t *bs
 * @param bs_t *bd
 * @param int new_pps_id
 * @param int *old_id
 * @return int
 */
static int patch_pps(h264_sps_id_patcher_t *h, bit_reader_t *bs, bs_t *bd, int new_pps_id, int *old_id)
{
    // LOG_INFO("patch pps");
    int bytes;
    unsigned pps_id = ue_bits(bs); // max = 255
    unsigned sps_id = ue_bits(bs); // max = 31

    *old_id = pps_id;
    sps_id = h->map_sps[sps_id];
    pps_id = new_pps_id;

    assert(sps_id <= 31);
    assert(pps_id <= 255);

    h264e_bs_put_golomb(bd, pps_id);
    h264e_bs_put_golomb(bd, sps_id);
    copy_bits(bs, bd);

    bytes = h264e_bs_byte_align(bd) / 8;
    h264e_bs_flush(bd);
    return bytes;
}

/**
 * @brief patch slice header
 *
 * @param h264_sps_id_patcher_t *h
 * @param bit_reader_t *bs
 * @param bs_t *bd
 */
static void patch_slice_header(h264_sps_id_patcher_t *h, bit_reader_t *bs, bs_t *bd)
{
    // LOG_INFO("patch slice header");
    unsigned first_mb_in_slice = ue_bits(bs);
    unsigned slice_type = ue_bits(bs);
    unsigned pps_id = ue_bits(bs);

    pps_id = h->map_pps[pps_id];

    assert(pps_id <= 255);

    h264e_bs_put_golomb(bd, first_mb_in_slice);
    h264e_bs_put_golomb(bd, slice_type);
    h264e_bs_put_golomb(bd, pps_id);
    copy_bits(bs, bd);
}

/**
 * @brief transcode nalu
 *
 * @param h264_sps_id_patcher_t *h
 * @param const unsigned char *src
 * @param int nalu_bytes
 * @param unsigned char *dst
 * @return int
 */
static int transcode_nalu(h264_sps_id_patcher_t *h, const unsigned char *src, int nalu_bytes, unsigned char *dst)
{
    LOG_INFO("transcode nalu");
    int old_id;

    bit_reader_t bst[1];
    bs_t bdt[1];

    bit_reader_t bs[1];
    bs_t bd[1];
    int payload_type = src[0] & 31;

    *dst = *src;
    h264e_bs_init_bits(bd, dst + 1);
    init_bits(bs, src + 1, nalu_bytes - 1);
    h264e_bs_init_bits(bdt, dst + 1);
    init_bits(bst, src + 1, nalu_bytes - 1);

    switch (payload_type)
    {
    case 7:
    {
        int cb = change_sps_id(bst, bdt, 0, &old_id);
        int id = find_mem_cache(h->sps_cache, h->sps_bytes, MINIMP4_MAX_SPS, dst + 1, cb);
        if (id == -1)
            return 0;
        h->map_sps[old_id] = id;
        change_sps_id(bs, bd, id, &old_id);
    }
    break;
    case 8:
    {
        int cb = patch_pps(h, bst, bdt, 0, &old_id);
        int id = find_mem_cache(h->pps_cache, h->pps_bytes, MINIMP4_MAX_PPS, dst + 1, cb);
        if (id == -1)
            return 0;
        h->map_pps[old_id] = id;
        patch_pps(h, bs, bd, id, &old_id);
    }
    break;
    case 1:
    case 2:
    case 5:
        patch_slice_header(h, bs, bd);
        break;
    default:
        memcpy(dst, src, nalu_bytes);
        return nalu_bytes;
    }

    nalu_bytes = 1 + h264e_bs_byte_align(bd) / 8;
    h264e_bs_flush(bd);

    return nalu_bytes;
}

#endif

/**
 *   Find start code
 *   Set pointer just after start code (00 .. 00 01), or to EOF if not found:
 *
 *   NZ NZ ... NZ 00 00 00 00 01 xx xx ... xx (EOF)
 *                               ^            ^
 *   non-zero head.............. here ....... or here if no start code found
 *
 */
static const uint8_t *find_start_code(const uint8_t *h264_data, int h264_data_bytes, int *zcount)
{
    LOG_INFO("find start code");
    const uint8_t *eof = h264_data + h264_data_bytes;
    const uint8_t *p = h264_data;
    do
    {
        int zero_cnt = 1;
        const uint8_t *found = (uint8_t *)memchr(p, 0, eof - p);
        p = found ? found : eof;
        while (p + zero_cnt < eof && !p[zero_cnt])
            zero_cnt++;
        if (zero_cnt >= 2 && p[zero_cnt] == 1)
        {
            *zcount = zero_cnt + 1;
            return p + zero_cnt + 1;
        }
        p += zero_cnt;
    } while (p < eof);
    *zcount = 0;
    return eof;
}

/**
 *   Locate NAL unit in given buffer, and calculate it's length
 */
static const uint8_t *find_nal_unit(const uint8_t *h264_data, int h264_data_bytes, int *pnal_unit_bytes)
{
    LOG_INFO("Find nal unit");
    const uint8_t *eof = h264_data + h264_data_bytes;
    int zcount;
    const uint8_t *start = find_start_code(h264_data, h264_data_bytes, &zcount);
    const uint8_t *stop = start;
    if (start)
    {
        stop = find_start_code(start, (int)(eof - start), &zcount);
        while (stop > start && !stop[-1])
        {
            stop--;
        }
    }

    *pnal_unit_bytes = (int)(stop - start - zcount);
    return start;
}

int mp4_h26x_write_init(mp4_h26x_writer_t *h, MP4E_mux_t *mux, int width, int height, int is_hevc)
{
    LOG_INFO("mp4 h26x write init");
    MP4E_track_t tr;
    tr.track_media_kind = e_video;
    tr.language[0] = 'u';
    tr.language[1] = 'n';
    tr.language[2] = 'd';
    tr.language[3] = 0;
    tr.object_type_indication = is_hevc ? MP4_OBJECT_TYPE_HEVC : MP4_OBJECT_TYPE_AVC;
    tr.time_scale = 90000;
    tr.default_duration = 0;
    tr.u.v.width = width;
    tr.u.v.height = height;
    h->mux_track_id = MP4E_add_track(mux, &tr);
    h->mux = mux;

    h->is_hevc = is_hevc;
    h->need_vps = is_hevc;
    h->need_sps = 1;
    h->need_pps = 1;
    h->need_idr = 1;
#if MINIMP4_TRANSCODE_SPS_ID
    memset(&h->sps_patcher, 0, sizeof(h264_sps_id_patcher_t));
#endif
    return MP4E_STATUS_OK;
}

void mp4_h26x_write_close(mp4_h26x_writer_t *h)
{
    LOG_INFO("mp4 h26x write close");
#if MINIMP4_TRANSCODE_SPS_ID
    h264_sps_id_patcher_t *p = &h->sps_patcher;
    int i;
    for (i = 0; i < MINIMP4_MAX_SPS; i++)
    {
        if (p->sps_cache[i])
            free(p->sps_cache[i]);
    }
    for (i = 0; i < MINIMP4_MAX_PPS; i++)
    {
        if (p->pps_cache[i])
            free(p->pps_cache[i]);
    }
#endif
    memset(h, 0, sizeof(*h));
}

static int mp4_h265_write_nal(mp4_h26x_writer_t *h, const unsigned char *nal, int sizeof_nal, unsigned timeStamp90kHz_next)
{
    LOG_INFO("mp4 h265 write nal");
    int payload_type = (nal[0] >> 1) & 0x3f;
    int is_intra = payload_type >= HEVC_NAL_BLA_W_LP && payload_type <= HEVC_NAL_CRA_NUT;
    int err = MP4E_STATUS_OK;
    printf("---> payload_type=%d, intra=%d\n", payload_type, is_intra); // NCL

    if (is_intra && !h->need_sps && !h->need_pps && !h->need_vps)
        h->need_idr = 0;
    switch (payload_type)
    {
    case HEVC_NAL_VPS:
        MP4E_set_vps(h->mux, h->mux_track_id, nal, sizeof_nal);
        h->need_vps = 0;
        break;
    case HEVC_NAL_SPS:
        MP4E_set_sps(h->mux, h->mux_track_id, nal, sizeof_nal);
        h->need_sps = 0;
        break;
    case HEVC_NAL_PPS:
        MP4E_set_pps(h->mux, h->mux_track_id, nal, sizeof_nal);
        h->need_pps = 0;
        break;
    default:
        if (h->need_vps || h->need_sps || h->need_pps || h->need_idr)
            return MP4E_STATUS_BAD_ARGUMENTS;
        {
            unsigned char *tmp = (unsigned char *)malloc(4 + sizeof_nal);
            if (!tmp)
                return MP4E_STATUS_NO_MEMORY;
            int sample_kind = MP4E_SAMPLE_DEFAULT;
            tmp[0] = (unsigned char)(sizeof_nal >> 24);
            tmp[1] = (unsigned char)(sizeof_nal >> 16);
            tmp[2] = (unsigned char)(sizeof_nal >> 8);
            tmp[3] = (unsigned char)(sizeof_nal);
            memcpy(tmp + 4, nal, sizeof_nal);
            if (is_intra)
                sample_kind = MP4E_SAMPLE_RANDOM_ACCESS;
            err = MP4E_put_sample(h->mux, h->mux_track_id, tmp, 4 + sizeof_nal, timeStamp90kHz_next, sample_kind);
            free(tmp);
        }
        break;
    }
    return err;
}

int mp4_h26x_write_nal(mp4_h26x_writer_t *h, const unsigned char *nal, int length, unsigned timeStamp90kHz_next)
{
    if (h == NULL || nal == NULL || length <= 0)
    {
        return -1;
    }

    LOG_INFO("mp4 h26x write nal");
    const unsigned char *eof = nal + length;
    int payload_type, sizeof_nal, err = MP4E_STATUS_OK;
    for (;; nal++)
    {
#if MINIMP4_TRANSCODE_SPS_ID
        unsigned char *nal1, *nal2;
#endif
        nal = find_nal_unit(nal, (int)(eof - nal), &sizeof_nal);
        if (!sizeof_nal)
            break;
        if (h->is_hevc)
        {
            ERR(mp4_h265_write_nal(h, nal, sizeof_nal, timeStamp90kHz_next));
            continue;
        }
        payload_type = nal[0] & 31;
        if (9 == payload_type)
            continue; // access unit delimiter, nothing to be done
#if MINIMP4_TRANSCODE_SPS_ID
        // Transcode SPS, PPS and slice headers, reassigning ID's for SPS and  PPS:
        // - assign unique ID's to different SPS and PPS
        // - assign same ID's to equal (except ID) SPS and PPS
        // - save all different SPS and PPS
        nal1 = (unsigned char *)malloc(sizeof_nal * 17 / 16 + 32);
        if (!nal1)
            return MP4E_STATUS_NO_MEMORY;
        nal2 = (unsigned char *)malloc(sizeof_nal * 17 / 16 + 32);
        if (!nal2)
        {
            free(nal1);
            return MP4E_STATUS_NO_MEMORY;
        }
        sizeof_nal = remove_nal_escapes(nal2, nal, sizeof_nal);
        if (!sizeof_nal)
        {
        exit_with_free:
            free(nal1);
            free(nal2);
            return MP4E_STATUS_BAD_ARGUMENTS;
        }

        sizeof_nal = transcode_nalu(&h->sps_patcher, nal2, sizeof_nal, nal1);
        sizeof_nal = nal_put_esc(nal2, nal1, sizeof_nal);

        switch (payload_type)
        {
        case 7:
            MP4E_set_sps(h->mux, h->mux_track_id, nal2 + 4, sizeof_nal - 4);
            h->need_sps = 0;
            break;
        case 8:
            if (h->need_sps)
                goto exit_with_free;
            MP4E_set_pps(h->mux, h->mux_track_id, nal2 + 4, sizeof_nal - 4);
            h->need_pps = 0;
            break;
        case 5:
            if (h->need_sps)
                goto exit_with_free;
            h->need_idr = 0;
            // flow through
        default:
            if (h->need_sps)
                goto exit_with_free;
            if (!h->need_pps && !h->need_idr)
            {
                bit_reader_t bs[1];
                init_bits(bs, nal + 1, sizeof_nal - 4 - 1);
                unsigned first_mb_in_slice = ue_bits(bs);
                // unsigned slice_type = ue_bits(bs);
                int sample_kind = MP4E_SAMPLE_DEFAULT;
                nal2[0] = (unsigned char)((sizeof_nal - 4) >> 24);
                nal2[1] = (unsigned char)((sizeof_nal - 4) >> 16);
                nal2[2] = (unsigned char)((sizeof_nal - 4) >> 8);
                nal2[3] = (unsigned char)((sizeof_nal - 4));
                if (first_mb_in_slice)
                    sample_kind = MP4E_SAMPLE_CONTINUATION;
                else if (payload_type == 5)
                    sample_kind = MP4E_SAMPLE_RANDOM_ACCESS;
                err = MP4E_put_sample(h->mux, h->mux_track_id, nal2, sizeof_nal, timeStamp90kHz_next, sample_kind);
            }
            break;
        }
        free(nal1);
        free(nal2);
#else
        // No SPS/PPS transcoding
        // This branch assumes that encoder use correct SPS/PPS ID's
        switch (payload_type)
        {
        case 7:
            MP4E_set_sps(h->mux, h->mux_track_id, nal, sizeof_nal);
            h->need_sps = 0;
            break;
        case 8:
            MP4E_set_pps(h->mux, h->mux_track_id, nal, sizeof_nal);
            h->need_pps = 0;
            break;
        case 5:
            if (h->need_sps)
                return MP4E_STATUS_BAD_ARGUMENTS;
            h->need_idr = 0;
            // flow through
        default:
            if (h->need_sps)
                return MP4E_STATUS_BAD_ARGUMENTS;
            if (!h->need_pps && !h->need_idr)
            {
                bit_reader_t bs[1];
                unsigned char *tmp = (unsigned char *)malloc(4 + sizeof_nal);
                if (!tmp)
                    return MP4E_STATUS_NO_MEMORY;
                init_bits(bs, nal + 1, sizeof_nal - 1);
                unsigned first_mb_in_slice = ue_bits(bs);
                int sample_kind = MP4E_SAMPLE_DEFAULT;
                tmp[0] = (unsigned char)(sizeof_nal >> 24);
                tmp[1] = (unsigned char)(sizeof_nal >> 16);
                tmp[2] = (unsigned char)(sizeof_nal >> 8);
                tmp[3] = (unsigned char)(sizeof_nal);
                memcpy(tmp + 4, nal, sizeof_nal);
                if (first_mb_in_slice)
                    sample_kind = MP4E_SAMPLE_CONTINUATION;
                else if (payload_type == 5)
                    sample_kind = MP4E_SAMPLE_RANDOM_ACCESS;
                err = MP4E_put_sample(h->mux, h->mux_track_id, tmp, 4 + sizeof_nal, timeStamp90kHz_next, sample_kind);
                free(tmp);
            }
            break;
        }
#endif
        if (err)
            break;
    }
    return err;
}

#if MP4D_TRACE_SUPPORTED
#define TRACE(x) printf x
#else
#define TRACE(x)
#endif

#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

static int minimp4_fgets(MP4D_demux_t *mp4)
{
    // LOG_INFO("minimp4 fgets");
    uint8_t c;
    if (mp4->read_callback(mp4->read_pos, &c, 1, mp4->token))
        return -1;
    mp4->read_pos++;
    return c;
}

/**
 *   Read given number of bytes from input stream
 *   Used to read box headers
 */
static unsigned minimp4_read(MP4D_demux_t *mp4, int nb, int *eof_flag)
{
    LOG_INFO("Read given number of bytes from input stream, Used to read box headers");
    uint32_t v = 0;
    int last_byte;
    switch (nb)
    {
    case 4:
        v = (v << 8) | minimp4_fgets(mp4);
    case 3:
        v = (v << 8) | minimp4_fgets(mp4);
    case 2:
        v = (v << 8) | minimp4_fgets(mp4);
    default:
    case 1:
        v = (v << 8) | (last_byte = minimp4_fgets(mp4));
    }
    if (last_byte < 0)
    {
        *eof_flag = 1;
    }
    return v;
}

/**
 *   Read given number of bytes, but no more than *payload_bytes specifies...
 *   Used to read box payload
 */
static uint32_t read_payload(MP4D_demux_t *mp4, unsigned nb, boxsize_t *payload_bytes, int *eof_flag)
{
    LOG_INFO("Read given number of bytes, but no more than *payload_bytes specifies...Used to read box payload");
    if (*payload_bytes < nb)
    {
        *eof_flag = 1;
        nb = (int)*payload_bytes;
    }
    *payload_bytes -= nb;

    return minimp4_read(mp4, nb, eof_flag);
}

/**
 *   Skips given number of bytes.
 *   Avoid math operations with fpos_t
 */
static void my_fseek(MP4D_demux_t *mp4, boxsize_t pos, int *eof_flag)
{
    LOG_INFO("Skips given number of bytes. Avoid math operations with fpos_t");
    mp4->read_pos += pos;
    if (mp4->read_pos >= mp4->read_size)
        *eof_flag = 1;
}

#define READ(n) read_payload(mp4, n, &payload_bytes, &eof_flag)
#define SKIP(n)                                      \
    {                                                \
        boxsize_t t = MINIMP4_MIN(payload_bytes, n); \
        my_fseek(mp4, t, &eof_flag);                 \
        payload_bytes -= t;                          \
    }
#define MALLOC(t, p, size)      \
    p = (t)malloc(size);        \
    if (!(p))                   \
    {                           \
        ERROR("out of memory"); \
    }

/*
 *   On error: release resources.
 */
#define RETURN_ERROR(mess)             \
    {                                  \
        TRACE(("\nMP4 ERROR: " mess)); \
        MP4D_close(mp4);               \
        return 0;                      \
    }

/*
 *   Any errors, occurred on top-level hierarchy is passed to exit check: 'if (!mp4->track_count) ... '
 */
#define ERROR(mess) \
    if (!depth)     \
        break;      \
    else            \
        RETURN_ERROR(mess);

typedef enum
{
    BOX_ATOM,
    BOX_OD
} boxtype_t;

int MP4D_open(MP4D_demux_t *mp4, int (*read_callback)(int64_t offset, void *buffer, size_t size, void *token), void *token, int64_t file_size)
{
    LOG_INFO("MP4D open");
    // box stack size
    int depth = 0;

    struct
    {
        // remaining bytes for box in the stack
        boxsize_t bytes;

        // kind of box children's: OD chunks handled in the same manner as name chunks
        boxtype_t format;

    } stack[MAX_CHUNKS_DEPTH];

#if MP4D_TRACE_SUPPORTED
    // path of current element: List0/List1/... etc
    uint32_t box_path[MAX_CHUNKS_DEPTH];
#endif

    int eof_flag = 0;
    unsigned i;
    MP4D_track_t *tr = NULL;

    if (!mp4 || !read_callback)
    {
        TRACE(("\nERROR: invlaid arguments!"));
        return 0;
    }

    memset(mp4, 0, sizeof(MP4D_demux_t));
    mp4->read_callback = read_callback;
    mp4->token = token;
    mp4->read_size = file_size;

    stack[0].format = BOX_ATOM; // start with atom box
    stack[0].bytes = 0;         // never accessed

    do
    {
        // List of boxes, derived from 'FullBox'
        //                ~~~~~~~~~~~~~~~~~~~~~
        // need read version field and check version for these boxes
        static const struct
        {
            uint32_t name;
            unsigned max_version;
            unsigned use_track_flag;
        } g_fullbox[] =
        {
#if MP4D_INFO_SUPPORTED
            {BOX_mdhd, 1, 1},
            {BOX_mvhd, 1, 0},
            {BOX_hdlr, 0, 0},
            {BOX_meta, 0, 0}, // Android can produce meta box without 'FullBox' field, comment this line to simulate the bug
#endif
#if MP4D_TRACE_TIMESTAMPS
            {BOX_stts, 0, 0},
            {BOX_ctts, 0, 0},
#endif
            {BOX_stz2, 0, 1},
            {BOX_stsz, 0, 1},
            {BOX_stsc, 0, 1},
            {BOX_stco, 0, 1},
            {BOX_co64, 0, 1},
            {BOX_stsd, 0, 0},
            {BOX_esds, 0, 1} // esds does not use track, but switches to OD mode. Check here, to avoid OD check
        };

        // List of boxes, which contains other boxes ('envelopes')
        // Parser will descend down for boxes in this list, otherwise parsing will proceed to
        // the next sibling box
        // OD boxes handled in the same way as atom boxes...
        static const struct
        {
            uint32_t name;
            boxtype_t type;
        } g_envelope_box[] =
        {
            {BOX_esds, BOX_OD}, // TODO: BOX_esds can be used for both audio and video, but this code supports audio only!
            {OD_ESD, BOX_OD},
            {OD_DCD, BOX_OD},
            {OD_DSI, BOX_OD},
            {BOX_trak, BOX_ATOM},
            {BOX_moov, BOX_ATOM},
            //{BOX_moof, BOX_ATOM},
            {BOX_mdia, BOX_ATOM},
            {BOX_tref, BOX_ATOM},
            {BOX_minf, BOX_ATOM},
            {BOX_dinf, BOX_ATOM},
            {BOX_stbl, BOX_ATOM},
            {BOX_stsd, BOX_ATOM},
            {BOX_mp4a, BOX_ATOM},
            {BOX_mp4s, BOX_ATOM},
#if MP4D_AVC_SUPPORTED
            {BOX_mp4v, BOX_ATOM},
            {BOX_avc1, BOX_ATOM},
        //{BOX_avc2, BOX_ATOM},
        //{BOX_svc1, BOX_ATOM},
#endif
#if MP4D_HEVC_SUPPORTED
            {BOX_hvc1, BOX_ATOM},
#endif
            {BOX_udta, BOX_ATOM},
            {BOX_meta, BOX_ATOM},
            {BOX_ilst, BOX_ATOM}
        };

        uint32_t FullAtomVersionAndFlags = 0;
        boxsize_t payload_bytes;
        boxsize_t box_bytes;
        uint32_t box_name;
#if MP4D_INFO_SUPPORTED
        unsigned char **ptag = NULL;
#endif
        int read_bytes = 0;

        // Read header box type and it's length
        if (stack[depth].format == BOX_ATOM)
        {
            box_bytes = minimp4_read(mp4, 4, &eof_flag);
#if FIX_BAD_ANDROID_META_BOX
        broken_android_meta_hack:
#endif
            if (eof_flag)
                break; // normal exit

            if (box_bytes >= 2 && box_bytes < 8)
            {
                ERROR("invalid box size (broken file?)");
            }

            box_name = minimp4_read(mp4, 4, &eof_flag);
            read_bytes = 8;

            // Decode box size
            if (box_bytes == 0 ||                   // standard indication of 'till eof' size
                box_bytes == (boxsize_t)0xFFFFFFFFU // some files uses non-standard 'till eof' signaling
            )
            {
                box_bytes = ~(boxsize_t)0;
            }

            payload_bytes = box_bytes - 8;

            if (box_bytes == 1) // 64-bit sizes
            {
                TRACE(("\n64-bit chunk encountered"));

                box_bytes = minimp4_read(mp4, 4, &eof_flag);
#if MP4D_64BIT_SUPPORTED
                box_bytes <<= 32;
                box_bytes |= minimp4_read(mp4, 4, &eof_flag);
#else
                if (box_bytes)
                {
                    ERROR("UNSUPPORTED FEATURE: MP4BoxHeader(): 64-bit boxes not supported!");
                }
                box_bytes = minimp4_read(mp4, 4, &eof_flag);
#endif
                if (box_bytes < 16)
                {
                    ERROR("invalid box size (broken file?)");
                }
                payload_bytes = box_bytes - 16;
            }

            // Read and check box version for some boxes
            for (i = 0; i < NELEM(g_fullbox); i++)
            {
                if (box_name == g_fullbox[i].name)
                {
                    FullAtomVersionAndFlags = READ(4);
                    read_bytes += 4;

#if FIX_BAD_ANDROID_META_BOX
                    // Fix invalid BOX_meta, found in some Android-produced MP4
                    // This branch is optional: bad box would be skipped
                    if (box_name == BOX_meta)
                    {
                        if (FullAtomVersionAndFlags >= 8 && FullAtomVersionAndFlags < payload_bytes)
                        {
                            if (box_bytes > stack[depth].bytes)
                            {
                                ERROR("broken file structure!");
                            }
                            stack[depth].bytes -= box_bytes;
                            ;
                            depth++;
                            stack[depth].bytes = payload_bytes + 4; // +4 need for missing header
                            stack[depth].format = BOX_ATOM;
                            box_bytes = FullAtomVersionAndFlags;
                            TRACE(("Bad metadata box detected (Android bug?)!\n"));
                            goto broken_android_meta_hack;
                        }
                    }
#endif // FIX_BAD_ANDROID_META_BOX

                    if ((FullAtomVersionAndFlags >> 24) > g_fullbox[i].max_version)
                    {
                        ERROR("unsupported box version!");
                    }
                    if (g_fullbox[i].use_track_flag && !tr)
                    {
                        ERROR("broken file structure!");
                    }
                }
            }
        }
        else // stack[depth].format == BOX_OD
        {
            int val;
            box_name = OD_BASE + minimp4_read(mp4, 1, &eof_flag); // 1-byte box type
            read_bytes += 1;
            if (eof_flag)
                break;

            payload_bytes = 0;
            box_bytes = 1;
            do
            {
                val = minimp4_read(mp4, 1, &eof_flag);
                read_bytes += 1;
                if (eof_flag)
                {
                    ERROR("premature EOF!");
                }
                payload_bytes = (payload_bytes << 7) | (val & 0x7F);
                box_bytes++;
            } while (val & 0x80);
            box_bytes += payload_bytes;
        }

#if MP4D_TRACE_SUPPORTED
        box_path[depth] = (box_name >> 24) | (box_name << 24) | ((box_name >> 8) & 0x0000FF00) | ((box_name << 8) & 0x00FF0000);
        TRACE(("%2d  %8d %.*s  (%d bytes remains for sibilings) \n", depth, (int)box_bytes, depth * 4, (char *)box_path, (int)stack[depth].bytes));
#endif

        // Check that box size <= parent size
        if (depth)
        {
            // Skip box with bad size
            assert(box_bytes > 0);
            if (box_bytes > stack[depth].bytes)
            {
                TRACE(("Wrong %c%c%c%c box size: broken file?\n", (box_name >> 24) & 255, (box_name >> 16) & 255, (box_name >> 8) & 255, box_name & 255));
                box_bytes = stack[depth].bytes;
                box_name = 0;
                payload_bytes = box_bytes - read_bytes;
            }
            stack[depth].bytes -= box_bytes;
        }

        // Read box header
        switch (box_name)
        {
        case BOX_stz2: // ISO/IEC 14496-1 Page 38. Section 8.17.2 - Sample Size Box.
        case BOX_stsz:
        {
            int size = 0;
            uint32_t sample_size = READ(4);
            tr->sample_count = READ(4);
            MALLOC(unsigned int *, tr->entry_size, tr->sample_count * 4);
            for (i = 0; i < tr->sample_count; i++)
            {
                if (box_name == BOX_stsz)
                {
                    tr->entry_size[i] = (sample_size ? sample_size : READ(4));
                }
                else
                {
                    switch (sample_size & 0xFF)
                    {
                    case 16:
                        tr->entry_size[i] = READ(2);
                        break;
                    case 8:
                        tr->entry_size[i] = READ(1);
                        break;
                    case 4:
                        if (i & 1)
                        {
                            tr->entry_size[i] = size & 15;
                        }
                        else
                        {
                            size = READ(1);
                            tr->entry_size[i] = (size >> 4);
                        }
                        break;
                    }
                }
            }
        }
        break;

        case BOX_stsc: // ISO/IEC 14496-12 Page 38. Section 8.18 - Sample To Chunk Box.
            tr->sample_to_chunk_count = READ(4);
            MALLOC(MP4D_sample_to_chunk_t *, tr->sample_to_chunk, tr->sample_to_chunk_count * sizeof(tr->sample_to_chunk[0]));
            for (i = 0; i < tr->sample_to_chunk_count; i++)
            {
                tr->sample_to_chunk[i].first_chunk = READ(4);
                tr->sample_to_chunk[i].samples_per_chunk = READ(4);
                SKIP(4); // sample_description_index
            }
            break;
#if MP4D_TRACE_TIMESTAMPS || MP4D_TIMESTAMPS_SUPPORTED
        case BOX_stts:
        {
            unsigned count = READ(4);
            unsigned j, k = 0, ts = 0, ts_count = count;
#if MP4D_TIMESTAMPS_SUPPORTED
            MALLOC(unsigned int *, tr->timestamp, ts_count * 4);
            MALLOC(unsigned int *, tr->duration, ts_count * 4);
#endif

            for (i = 0; i < count; i++)
            {
                unsigned sc = READ(4);
                int d = READ(4);
                TRACE(("sample %8d count %8d duration %8d\n", i, sc, d));
#if MP4D_TIMESTAMPS_SUPPORTED
                if (k + sc > ts_count)
                {
                    ts_count = k + sc;
                    tr->timestamp = (unsigned int *)realloc(tr->timestamp, ts_count * sizeof(unsigned));
                    tr->duration = (unsigned int *)realloc(tr->duration, ts_count * sizeof(unsigned));
                }
                for (j = 0; j < sc; j++)
                {
                    tr->duration[k] = d;
                    tr->timestamp[k++] = ts;
                    ts += d;
                }
#endif
            }
        }
        break;
        case BOX_ctts:
        {
            unsigned count = READ(4);
            for (i = 0; i < count; i++)
            {
                int sc = READ(4);
                int d = READ(4);
                (void)sc;
                (void)d;
                TRACE(("sample %8d count %8d decoding to composition offset %8d\n", i, sc, d));
            }
        }
        break;
#endif
        case BOX_stco: // ISO/IEC 14496-12 Page 39. Section 8.19 - Chunk Offset Box.
        case BOX_co64:
            tr->chunk_count = READ(4);
            MALLOC(MP4D_file_offset_t *, tr->chunk_offset, tr->chunk_count * sizeof(MP4D_file_offset_t));
            for (i = 0; i < tr->chunk_count; i++)
            {
                tr->chunk_offset[i] = READ(4);
                if (box_name == BOX_co64)
                {
#if !MP4D_64BIT_SUPPORTED
                    if (tr->chunk_offset[i])
                    {
                        ERROR("UNSUPPORTED FEATURE: 64-bit chunk_offset not supported!");
                    }
#endif
                    tr->chunk_offset[i] <<= 32;
                    tr->chunk_offset[i] |= READ(4);
                }
            }
            break;

#if MP4D_INFO_SUPPORTED
        case BOX_mvhd:
            SKIP(((FullAtomVersionAndFlags >> 24) == 1) ? 8 + 8 : 4 + 4);
            mp4->timescale = READ(4);
            mp4->duration_hi = ((FullAtomVersionAndFlags >> 24) == 1) ? READ(4) : 0;
            mp4->duration_lo = READ(4);
            SKIP(4 + 2 + 2 + 4 * 2 + 4 * 9 + 4 * 6 + 4);
            break;

        case BOX_mdhd:
            SKIP(((FullAtomVersionAndFlags >> 24) == 1) ? 8 + 8 : 4 + 4);
            tr->timescale = READ(4);
            tr->duration_hi = ((FullAtomVersionAndFlags >> 24) == 1) ? READ(4) : 0;
            tr->duration_lo = READ(4);

            {
                int ISO_639_2_T = READ(2);
                tr->language[2] = (ISO_639_2_T & 31) + 0x60;
                ISO_639_2_T >>= 5;
                tr->language[1] = (ISO_639_2_T & 31) + 0x60;
                ISO_639_2_T >>= 5;
                tr->language[0] = (ISO_639_2_T & 31) + 0x60;
            }
            // the rest of this box is skipped by default ...
            break;

        case BOX_hdlr:
            if (tr) // When this box is within 'meta' box, the track may not be avaialable
            {
                SKIP(4); // pre_defined
                tr->handler_type = READ(4);
            }
            // typically hdlr box does not contain any useful info.
            // the rest of this box is skipped by default ...
            break;

        case BOX_btrt:
            if (!tr)
            {
                ERROR("broken file structure!");
            }

            SKIP(4 + 4);
            tr->avg_bitrate_bps = READ(4);
            break;

            // Set pointer to tag to be read...
        case BOX_calb:
            ptag = &mp4->tag.album;
            break;
        case BOX_cART:
            ptag = &mp4->tag.artist;
            break;
        case BOX_cnam:
            ptag = &mp4->tag.title;
            break;
        case BOX_cday:
            ptag = &mp4->tag.year;
            break;
        case BOX_ccmt:
            ptag = &mp4->tag.comment;
            break;
        case BOX_cgen:
            ptag = &mp4->tag.genre;
            break;

#endif

        case BOX_stsd:
            SKIP(4); // entry_count, BOX_mp4a & BOX_mp4v boxes follows immediately
            break;

        case BOX_mp4s: // private stream
            if (!tr)
            {
                ERROR("broken file structure!");
            }
            SKIP(6 * 1 + 2 /*Base SampleEntry*/);
            break;

        case BOX_mp4a:
            if (!tr)
            {
                ERROR("broken file structure!");
            }
#if MP4D_INFO_SUPPORTED
            SKIP(6 * 1 + 2 /*Base SampleEntry*/ + 4 * 2);
            tr->SampleDescription.audio.channelcount = READ(2);
            SKIP(2 /*samplesize*/ + 2 + 2);
            tr->SampleDescription.audio.samplerate_hz = READ(4) >> 16;
#else
            SKIP(28);
#endif
            break;

#if MP4D_AVC_SUPPORTED
        case BOX_avc1: // AVCSampleEntry extends VisualSampleEntry
                       //         case BOX_avc2:   - no test
                       //         case BOX_svc1:   - no test
        case BOX_mp4v:
            if (!tr)
            {
                ERROR("broken file structure!");
            }
#if MP4D_INFO_SUPPORTED
            SKIP(6 * 1 + 2 /*Base SampleEntry*/ + 2 + 2 + 4 * 3);
            tr->SampleDescription.video.width = READ(2);
            tr->SampleDescription.video.height = READ(2);
            // frame_count is always 1
            // compressorname is rarely set..
            SKIP(4 + 4 + 4 + 2 /*frame_count*/ + 32 /*compressorname*/ + 2 + 2);
#else
            SKIP(78);
#endif
            // ^^^ end of VisualSampleEntry
            // now follows for BOX_avc1:
            //      BOX_avcC
            //      BOX_btrt (optional)
            //      BOX_m4ds (optional)
            // for BOX_mp4v:
            //      BOX_esds
            break;

        case BOX_avcC: // AVCDecoderConfigurationRecord()
            // hack: AAC-specific DSI field reused (for it have same purpoose as sps/pps)
            // TODO: check this hack if BOX_esds co-exist with BOX_avcC
            tr->object_type_indication = MP4_OBJECT_TYPE_AVC;
            tr->dsi = (unsigned char *)malloc((size_t)box_bytes);
            tr->dsi_bytes = (unsigned)box_bytes;
            {
                int spspps;
                unsigned char *p = tr->dsi;
                unsigned int configurationVersion = READ(1);
                unsigned int AVCProfileIndication = READ(1);
                unsigned int profile_compatibility = READ(1);
                unsigned int AVCLevelIndication = READ(1);
                // bit(6) reserved =
                unsigned int lengthSizeMinusOne = READ(1) & 3;

                (void)configurationVersion;
                (void)AVCProfileIndication;
                (void)profile_compatibility;
                (void)AVCLevelIndication;
                (void)lengthSizeMinusOne;

                for (spspps = 0; spspps < 2; spspps++)
                {
                    unsigned int numOfSequenceParameterSets = READ(1);
                    if (!spspps)
                    {
                        numOfSequenceParameterSets &= 31; // clears 3 msb for SPS
                    }
                    *p++ = numOfSequenceParameterSets;
                    for (i = 0; i < numOfSequenceParameterSets; i++)
                    {
                        unsigned k, sequenceParameterSetLength = READ(2);
                        *p++ = sequenceParameterSetLength >> 8;
                        *p++ = sequenceParameterSetLength;
                        for (k = 0; k < sequenceParameterSetLength; k++)
                        {
                            *p++ = READ(1);
                        }
                    }
                }
            }
            break;
#endif // MP4D_AVC_SUPPORTED

        case OD_ESD:
        {
            unsigned flags = READ(3); // ES_ID(2) + flags(1)

            if (flags & 0x80) // steamdependflag
            {
                SKIP(2); // dependsOnESID
            }
            if (flags & 0x40) // urlflag
            {
                unsigned bytecount = READ(1);
                SKIP(bytecount); // skip URL
            }
            if (flags & 0x20) // ocrflag (was reserved in MPEG-4 v.1)
            {
                SKIP(2); // OCRESID
            }
            break;
        }

        case OD_DCD:    // ISO/IEC 14496-1 Page 28. Section 8.6.5 - DecoderConfigDescriptor.
            assert(tr); // ensured by g_fullbox[] check
            tr->object_type_indication = READ(1);
#if MP4D_INFO_SUPPORTED
            tr->stream_type = READ(1) >> 2;
            SKIP(3 /*bufferSizeDB*/ + 4 /*maxBitrate*/);
            tr->avg_bitrate_bps = READ(4);
#else
            SKIP(1 + 3 + 4 + 4);
#endif
            break;

        case OD_DSI:    // ISO/IEC 14496-1 Page 28. Section 8.6.5 - DecoderConfigDescriptor.
            assert(tr); // ensured by g_fullbox[] check
            if (!tr->dsi && payload_bytes)
            {
                MALLOC(unsigned char *, tr->dsi, (int)payload_bytes);
                for (i = 0; i < payload_bytes; i++)
                {
                    tr->dsi[i] = minimp4_read(mp4, 1, &eof_flag); // These bytes available due to check above
                }
                tr->dsi_bytes = i;
                payload_bytes -= i;
                break;
            }

        default:
            TRACE(("[%c%c%c%c]  %d\n", box_name >> 24, box_name >> 16, box_name >> 8, box_name, (int)payload_bytes));
        }

#if MP4D_INFO_SUPPORTED
        // Read tag is tag pointer is set
        if (ptag && !*ptag && payload_bytes > 16)
        {
#if 0
            uint32_t size = READ(4);
            uint32_t data = READ(4);
            uint32_t class = READ(4);
            uint32_t x1 = READ(4);
            TRACE(("%2d  %2d %2d ", size, class, x1));
#else
            SKIP(4 + 4 + 4 + 4);
#endif
            MALLOC(unsigned char *, *ptag, (unsigned)payload_bytes + 1);
            for (i = 0; payload_bytes != 0; i++)
            {
                (*ptag)[i] = READ(1);
            }
            (*ptag)[i] = 0; // zero-terminated string
        }
#endif

        if (box_name == BOX_trak)
        {
            // New track found: allocate memory using realloc()
            // Typically there are 1 audio track for AAC audio file,
            // 4 tracks for movie file,
            // 3-5 tracks for scalable audio (CELP+AAC)
            // and up to 50 tracks for BSAC scalable audio
            void *mem = realloc(mp4->track, (mp4->track_count + 1) * sizeof(MP4D_track_t));
            if (!mem)
            {
                // if realloc fails, it does not deallocate old pointer!
                ERROR("out of memory");
            }
            mp4->track = (MP4D_track_t *)mem;
            tr = mp4->track + mp4->track_count++;
            memset(tr, 0, sizeof(MP4D_track_t));
        }
        else if (box_name == BOX_meta)
        {
            tr = NULL; // Avoid update of 'hdlr' box, which may contains in the 'meta' box
        }

        // If this box is envelope, save it's size in box stack
        for (i = 0; i < NELEM(g_envelope_box); i++)
        {
            if (box_name == g_envelope_box[i].name)
            {
                if (++depth >= MAX_CHUNKS_DEPTH)
                {
                    ERROR("too deep atoms nesting!");
                }
                stack[depth].bytes = payload_bytes;
                stack[depth].format = g_envelope_box[i].type;
                break;
            }
        }

        // if box is not envelope, just skip it
        if (i == NELEM(g_envelope_box))
        {
            if (payload_bytes > file_size)
            {
                eof_flag = 1;
            }
            else
            {
                SKIP(payload_bytes);
            }
        }

        // remove empty boxes from stack
        // don't touch box with index 0 (which indicates whole file)
        while (depth > 0 && !stack[depth].bytes)
        {
            depth--;
        }

    } while (!eof_flag);

    if (!mp4->track_count)
    {
        RETURN_ERROR("no tracks found");
    }

    return 1;
    
}

/**
 *   Find chunk, containing given sample.
 *   Returns chunk number, and first sample in this chunk.
 */
static int sample_to_chunk(MP4D_track_t *tr, unsigned nsample, unsigned *nfirst_sample_in_chunk)
{
    LOG_INFO("Find chuck, containing given sample");
    unsigned chunk_group = 0, nc;
    unsigned sum = 0;
    *nfirst_sample_in_chunk = 0;
    if (tr->chunk_count <= 1)
    {
        return 0;
    }
    for (nc = 0; nc < tr->chunk_count; nc++)
    {
        if (chunk_group + 1 < tr->sample_to_chunk_count              // stuck at last entry till EOF
            && nc + 1 ==                                             // Chunks counted starting with '1'
                   tr->sample_to_chunk[chunk_group + 1].first_chunk) // next group?
        {
            chunk_group++;
        }

        sum += tr->sample_to_chunk[chunk_group].samples_per_chunk;
        if (nsample < sum)
            return nc;

        // TODO: this can be calculated once per file
        *nfirst_sample_in_chunk = sum;
    }
    return -1;
}

// Exported API function
MP4D_file_offset_t MP4D_frame_offset(const MP4D_demux_t *mp4, unsigned ntrack, unsigned nsample, unsigned *frame_bytes, unsigned *timestamp, unsigned *duration)
{
    LOG_INFO("MP4D frame offset");
    MP4D_track_t *tr = mp4->track + ntrack;
    unsigned ns;
    int nchunk = sample_to_chunk(tr, nsample, &ns);
    MP4D_file_offset_t offset;

    if (nchunk < 0)
    {
        *frame_bytes = 0;
        return 0;
    }

    offset = tr->chunk_offset[nchunk];
    for (; ns < nsample; ns++)
    {
        offset += tr->entry_size[ns];
    }

    *frame_bytes = tr->entry_size[ns];

    if (timestamp)
    {
#if MP4D_TIMESTAMPS_SUPPORTED
        *timestamp = tr->timestamp[ns];
#else
        *timestamp = 0;
#endif
    }
    if (duration)
    {
#if MP4D_TIMESTAMPS_SUPPORTED
        *duration = tr->duration[ns];
#else
        *duration = 0;
#endif
    }

    return offset;
}

#define FREE(x)   \
    if (x)        \
    {             \
        free(x);  \
        x = NULL; \
    }

// Exported API function
void MP4D_close(MP4D_demux_t *mp4)
{
    LOG_INFO("MP4D close");
    while (mp4->track_count)
    {
        MP4D_track_t *tr = mp4->track + --mp4->track_count;
        FREE(tr->entry_size);
#if MP4D_TIMESTAMPS_SUPPORTED
        FREE(tr->timestamp);
        FREE(tr->duration);
#endif
        FREE(tr->sample_to_chunk);
        FREE(tr->chunk_offset);
        FREE(tr->dsi);
    }
    FREE(mp4->track);
#if MP4D_INFO_SUPPORTED
    FREE(mp4->tag.title);
    FREE(mp4->tag.artist);
    FREE(mp4->tag.album);
    FREE(mp4->tag.year);
    FREE(mp4->tag.comment);
    FREE(mp4->tag.genre);
#endif
}

static int skip_spspps(const unsigned char *p, int nbytes, int nskip)
{
    LOG_INFO("skip sps pps");
    int i, k = 0;
    for (i = 0; i < nskip; i++)
    {
        unsigned segmbytes;
        if (k > nbytes - 2)
            return -1;
        segmbytes = p[k] * 256 + p[k + 1];
        k += 2 + segmbytes;
    }
    return k;
}

static const void *MP4D_read_spspps(const MP4D_demux_t *mp4, unsigned int ntrack, int pps_flag, int nsps, int *sps_bytes)
{
    LOG_INFO("MP4D read sps pps");
    int sps_count, skip_bytes;
    int bytepos = 0;
    unsigned char *p = mp4->track[ntrack].dsi;
    if (ntrack >= mp4->track_count)
        return NULL;
    if (mp4->track[ntrack].object_type_indication != MP4_OBJECT_TYPE_AVC)
        return NULL; // SPS/PPS are specific for AVC format only

    if (pps_flag)
    {
        // Skip all SPS
        sps_count = p[bytepos++];
        skip_bytes = skip_spspps(p + bytepos, mp4->track[ntrack].dsi_bytes - bytepos, sps_count);
        if (skip_bytes < 0)
            return NULL;
        bytepos += skip_bytes;
    }

    // Skip sps/pps before the given target
    sps_count = p[bytepos++];
    if (nsps >= sps_count)
        return NULL;
    skip_bytes = skip_spspps(p + bytepos, mp4->track[ntrack].dsi_bytes - bytepos, nsps);
    if (skip_bytes < 0)
        return NULL;
    bytepos += skip_bytes;
    *sps_bytes = p[bytepos] * 256 + p[bytepos + 1];
    return p + bytepos + 2;
}

const void *MP4D_read_sps(const MP4D_demux_t *mp4, unsigned int ntrack, int nsps, int *sps_bytes)
{
    LOG_INFO("MP4D read sps");
    return MP4D_read_spspps(mp4, ntrack, 0, nsps, sps_bytes);
}

const void *MP4D_read_pps(const MP4D_demux_t *mp4, unsigned int ntrack, int npps, int *pps_bytes)
{
    LOG_INFO("MP4D read pps");
    return MP4D_read_spspps(mp4, ntrack, 1, npps, pps_bytes);
}

// #if MP4D_PRINT_INFO_SUPPORTED
/************************************************************************/
/*  Purely informational part, may be removed for embedded applications */
/************************************************************************/

//
// Decodes ISO/IEC 14496 MP4 stream type to ASCII string
//
static const char *GetMP4StreamTypeName(int streamType)
{
    switch (streamType)
    {
    case 0x00:
        return "Forbidden";
    case 0x01:
        return "ObjectDescriptorStream";
    case 0x02:
        return "ClockReferenceStream";
    case 0x03:
        return "SceneDescriptionStream";
    case 0x04:
        return "VisualStream";
    case 0x05:
        return "AudioStream";
    case 0x06:
        return "MPEG7Stream";
    case 0x07:
        return "IPMPStream";
    case 0x08:
        return "ObjectContentInfoStream";
    case 0x09:
        return "MPEGJStream";
    default:
        if (streamType >= 0x20 && streamType <= 0x3F)
        {
            return "User private";
        }
        else
        {
            return "Reserved for ISO use";
        }
    }
}

//
// Decodes ISO/IEC 14496 MP4 object type to ASCII string
//
static const char *GetMP4ObjectTypeName(int objectTypeIndication)
{
    switch (objectTypeIndication)
    {
    case 0x00:
        return "Forbidden";
    case 0x01:
        return "Systems ISO/IEC 14496-1";
    case 0x02:
        return "Systems ISO/IEC 14496-1";
    case 0x20:
        return "Visual ISO/IEC 14496-2";
    case 0x40:
        return "Audio ISO/IEC 14496-3";
    case 0x60:
        return "Visual ISO/IEC 13818-2 Simple Profile";
    case 0x61:
        return "Visual ISO/IEC 13818-2 Main Profile";
    case 0x62:
        return "Visual ISO/IEC 13818-2 SNR Profile";
    case 0x63:
        return "Visual ISO/IEC 13818-2 Spatial Profile";
    case 0x64:
        return "Visual ISO/IEC 13818-2 High Profile";
    case 0x65:
        return "Visual ISO/IEC 13818-2 422 Profile";
    case 0x66:
        return "Audio ISO/IEC 13818-7 Main Profile";
    case 0x67:
        return "Audio ISO/IEC 13818-7 LC Profile";
    case 0x68:
        return "Audio ISO/IEC 13818-7 SSR Profile";
    case 0x69:
        return "Audio ISO/IEC 13818-3";
    case 0x6A:
        return "Visual ISO/IEC 11172-2";
    case 0x6B:
        return "Audio ISO/IEC 11172-3";
    case 0x6C:
        return "Visual ISO/IEC 10918-1";
    case 0xFF:
        return "no object type specified";
    default:
        if (objectTypeIndication >= 0xC0 && objectTypeIndication <= 0xFE)
            return "User private";
        else
            return "Reserved for ISO use";
    }
}

/**
*   Print MP4 information to stdout.
*   Subject for customization to particular application

Output Example #1: movie file

MP4 FILE: 7 tracks found. Movie time 104.12 sec

No|type|lng| duration           | bitrate| Stream type            | Object type
 0|odsm|fre|   0.00 s      1 frm|       0| Forbidden              | Forbidden
 1|sdsm|fre|   0.00 s      1 frm|       0| Forbidden              | Forbidden
 2|vide|```| 104.12 s   2603 frm| 1960559| VisualStream           | Visual ISO/IEC 14496-2   -  720x304
 3|soun|ger| 104.06 s   2439 frm|  191242| AudioStream            | Audio ISO/IEC 14496-3    -  6 ch 24000 hz
 4|soun|eng| 104.06 s   2439 frm|  194171| AudioStream            | Audio ISO/IEC 14496-3    -  6 ch 24000 hz
 5|subp|ger|  71.08 s     25 frm|       0| Forbidden              | Forbidden
 6|subp|eng|  71.08 s     25 frm|       0| Forbidden              | Forbidden

Output Example #2: audio file with tags

MP4 FILE: 1 tracks found. Movie time 92.42 sec
title = 86-Second Blowout
artist = Yo La Tengo
album = May I Sing With Me
year = 1992

No|type|lng| duration           | bitrate| Stream type            | Object type
 0|mdir|und|  92.42 s   3980 frm|  128000| AudioStream            | Audio ISO/IEC 14496-3MP4 FILE: 1 tracks found. Movie time 92.42 sec

*/
void MP4D_printf_info(const MP4D_demux_t *mp4)
{
    unsigned i;
    printf("\nMP4 FILE: %d tracks found. Movie time %.2f sec\n",
           mp4->track_count, (4294967296.0 * mp4->duration_hi + mp4->duration_lo) / mp4->timescale);

#define STR_TAG(name)  \
    if (mp4->tag.name) \
    printf("%10s = %s\n", #name, mp4->tag.name)
    STR_TAG(title);
    STR_TAG(artist);
    STR_TAG(album);
    STR_TAG(year);
    STR_TAG(comment);
    STR_TAG(genre);
    printf("\nNo|type|lng| duration           | bitrate| %-23s| Object type", "Stream type");
    for (i = 0; i < mp4->track_count; i++)
    {
        MP4D_track_t *tr = mp4->track + i;

        printf("\n%2d|%c%c%c%c|%c%c%c|%7.2f s %6d frm| %7d|", i,
               (tr->handler_type >> 24), (tr->handler_type >> 16), (tr->handler_type >> 8), (tr->handler_type >> 0),
               tr->language[0], tr->language[1], tr->language[2],
               (65536.0 * 65536.0 * tr->duration_hi + tr->duration_lo) / tr->timescale,
               tr->sample_count,
               tr->avg_bitrate_bps);

        printf(" %-23s|", GetMP4StreamTypeName(tr->stream_type));
        printf(" %-23s", GetMP4ObjectTypeName(tr->object_type_indication));

        if (tr->handler_type == MP4D_HANDLER_TYPE_SOUN)
        {
            printf("  -  %d ch %d hz", tr->SampleDescription.audio.channelcount, tr->SampleDescription.audio.samplerate_hz);
        }
        else if (tr->handler_type == MP4D_HANDLER_TYPE_VIDE)
        {
            printf("  -  %dx%d", tr->SampleDescription.video.width, tr->SampleDescription.video.height);
        }
    }
    printf("\n");
}