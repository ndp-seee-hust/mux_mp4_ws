/**
 * @file g711.h
 * @author phongdv
 * @brief g711 lib. G.711 is an audio coding standard used in telecommunications for audio compression and decompression of voice signals.
 * @version 0.1
 * @date 2023-05-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef G711_H_
#define G711_H_

int linear2ulaw(int	pcm_val);
int ulaw2linear(unsigned char ulawbyte);
int convert_pcm_buf_2_ulaw_buf(short *in_buf, unsigned char *out_buf, int size);
int convert_ulaw_buf_2_pcm_buf(unsigned char *in_buf, short *out_buf, int size);

#endif /* G711_H_ */
