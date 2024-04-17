

/* FILE   : h264reader.h
 * AUTHOR : leon
 * DATE   : Dec, 9 2015
 * DESC   :
 */
#ifndef H264_READER_H_
#define H264_READER_H_

/*
 * PURPOSE : Remove H264 file reader
 * INPUT   : [reader] - Reader pointer
 * OUTPUT  : None
 * RETURN  : None
 * DESCRIPT: None
 */
void
H264FileReaderRemove(void* reader);

/*
 * PURPOSE : Create new H264 file reader
 * INPUT   : [fileName] - File name
 * OUTPUT  : None
 * RETURN  : Reader pointer
 * DESCRIPT: None
 */
void*
H264FileReaderCreate(char* fileName);

/*
 * PURPOSE : Read 1 H264 frame from file
 * INPUT   : [reader] - Reader pointer
 * OUTPUT  : [data]   - Frame pointer
 *           [size]   - Frame size
 * RETURN  : Frame size. 0 if EOF
 * DESCRIPT: None
 */
int
H264FileReaderGetFrame(void* reader, void* data, int* size);

#endif /* H264_READER_H_ */