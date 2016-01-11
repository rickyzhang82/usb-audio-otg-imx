//File   : lplay.c
//Author : Loon <sepnic@gmail.com>

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <signal.h>
#include "wav_parser.h"
#include "wav_parser.c"
#include "sndwav_common.h"
#include "sndwav_common.c"

SNDPCMContainer_t *playback;

ssize_t SNDWAV_P_SaveRead(int fd, void *buf, size_t count)
{
	ssize_t res;

	res = read(fd, buf, count);

	return res;
}


void SNDWAV_Play(SNDPCMContainer_t *sndpcm, WAVContainer_t *wav, int fd)
{
	int load;
	off64_t written = 0;
	off64_t c;
	off64_t count = LE_INT(wav->chunk.length);
	int    readRet, writeRet;

	load = 0;
	//while (written < count) 
	while (1)  /* read and write forever */
	{
	    /* Must read [chunk_bytes] bytes data enough. */
	    //c = count - written;
	    //if (c > sndpcm->chunk_bytes)
	    c = sndpcm->chunk_bytes;


	    readRet = SNDWAV_P_SaveRead(fd, sndpcm->data_buf, c);

	    if(readRet < 0)
	    {
		perror("Read PCM file failed! \n");
	        fprintf(stderr, "Try read %lld bytes ret is %d \n", c, readRet);
		break;
	    }

	    if(readRet != c)
	    {
		fprintf(stderr, "Read PCM return %d bytes, should return %lld bytes!, maybe pipe broken \n", readRet, c);
	    }

	    /* Transfer to size frame */ /* load and writeRet both as frame number */
	    load = readRet * 8 / sndpcm->bits_per_frame;  /* bits per frame = bits per sample x channels */

	    writeRet = SNDWAV_WritePcm(sndpcm, load);  
	    if (writeRet != load)
	    {
		
		fprintf(stderr, "Write alsa return %d frames,  should return %d frames \n", writeRet, load);
		//snd_pcm_drain(playback->handle);
		//break;
	    }

	    //writeRet = writeRet * sndpcm->bits_per_frame / 8;
	    //written += writeRet;

	    //if(readRet != c)
	    //{
		/* drain pcm */
		//snd_pcm_drain(playback->handle);
            //}
	}
}

void INTHandler(int arg)
{
    snd_pcm_drain(playback->handle);
    close(playback->fd);
    free(playback->data_buf);
    snd_output_close(playback->log);
    snd_pcm_close(playback->handle);
    free(playback);
    exit(0);
}

int main(int argc, char *argv[])
{
	char *filename;
	char *devicename = "default";
	int fd;
	WAVContainer_t wav;
	
	if (argc != 2) {
		fprintf(stderr, "Usage: ./lplay <FILENAME>\n");
		return -1;
	}
	
	signal(SIGINT, INTHandler);
		
	playback = malloc(sizeof *playback);

	memset(playback, 0x0, sizeof(*playback));

	filename = argv[1];
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error open [%s]\n", filename);
		return -1;
	}
	
	playback->fd = fd;

	if (WAV_ReadHeader(fd, &wav) < 0) {
		fprintf(stderr, "Error WAV_Parse [%s]\n", filename);
		goto Err;
	}

	if (snd_output_stdio_attach(&playback->log, stderr, 0) < 0) {
		fprintf(stderr, "Error snd_output_stdio_attach\n");
		goto Err;
	}

	if (snd_pcm_open(&playback->handle, devicename, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
		fprintf(stderr, "Error snd_pcm_open [ %s]\n", devicename);
		goto Err;
	}

	if (SNDWAV_SetParams(playback, &wav) < 0) {
		fprintf(stderr, "Error set_snd_pcm_params\n");
		goto Err;
	}
	snd_pcm_dump(playback->handle, playback->log);

	SNDWAV_Play(playback, &wav, fd);

	snd_pcm_drain(playback->handle);

	close(fd);
	free(playback->data_buf);
	snd_output_close(playback->log);
	snd_pcm_close(playback->handle);
	free(playback);
	return 0;

Err:
	close(fd);
	if (playback->data_buf) free(playback->data_buf);
	if (playback->log) snd_output_close(playback->log);
	if (playback->handle) snd_pcm_close(playback->handle);
	free(playback);
	return -1;
}
