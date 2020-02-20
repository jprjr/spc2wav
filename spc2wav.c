#include <snes_spc/spc.h>
#include <id666/id666.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SAMPLE_RATE 32000
#define CHANNELS 2

#define str_equals(s1,s2) (strcmp(s1,s2) == 0)
#define str_istarts(s1,s2) (strncasecmp(s1,s2,strlen(s2)) == 0)

static char time_buf[256];

static const char *frame_to_time(uint64_t frames);
static unsigned int scan_uint(const char *s);
static void pack_int16le(uint8_t *d, int16_t n);
static void pack_uint16le(uint8_t *d, uint16_t n);
static void pack_uint32le(uint8_t *d, uint32_t n);
static void fade_frames(int16_t *d, unsigned int framesRem, unsigned int fadeFrames, unsigned int frameCount);
static void pack_frames(uint8_t *d, int16_t *s, unsigned int frameCount);
static int write_wav_header(FILE *f, uint64_t totalFrames);
static int write_frames(FILE *f, uint8_t *d, unsigned int frameCount);
static uint8_t *slurp(char *filename, uint32_t *size);

int usage(const char *self, int e) {
    fprintf(stderr,"Usage: %s --amp (amplitude) /path/to/file\n",self);
    fprintf(stderr,"  \"Accurate\" SNES amplitude = 256\n");
    fprintf(stderr,"  Default = 384\n");
    return e;
}

int main(int argc, char *argv[]) {
    snes_spc_t *spc;
    spc_filter_t *filter;
    id666 id6;

    uint8_t *rom;
    uint32_t romSize;
    int16_t *buf; /* audio samples, native machine format */
    uint8_t *pac; /* audio samples, little-endian format */
    int fc; /* current # of frames to decode */
    FILE *out;
    uint64_t totalFrames;
    uint64_t fadeFrames;
    unsigned int outFileLen;
    char *outFile;
    const char *s;
    const char *self;
    char *c;

    unsigned int amp;

    rom = NULL;
    buf = NULL;
    pac = NULL;
    romSize = 0;
    amp = 0x180;
    self = *argv++;
    argc--;

    while(argc > 0) {
        if(str_equals(*argv,"--")) {
            argv++;
            argc--;
            break;
        }
        else if(str_istarts(*argv,"--amp")) {
            c = strchr(*argv,'=');
            if(c != NULL) {
                s = &c[1];
            } else {
                argv++;
                argc--;
                s = *argv;
            }
            amp = scan_uint(s);
            if(amp == 0) {
                return usage(self,1);
            }
            argv++;
            argc--;
        }
        else {
            break;
        }
    }

    if(argc < 1) {
        return usage(self,1);
    }

    if(argc > 1) {
        outFile = strdup(argv[1]);
    } else {
        outFile = strdup(argv[0]);
        c = strrchr(outFile,'.');
        *c = 0;
        outFileLen = snprintf(NULL,0,"%s%s",outFile,".wav");
        free(outFile);
        outFile = (char *)malloc(outFileLen + 1);
        strcpy(outFile,argv[0]);
        c = strrchr(outFile,'.');
        *c = 0;
        strcat(outFile,".wav");
    }

    rom = slurp(argv[0],&romSize);
    if(rom == NULL) return 1;

    if(id666_parse(&id6,rom,romSize)) {
        return 1;
    }

    spc = spc_new();
    filter = spc_filter_new();

    if(spc_load_spc(spc,rom,romSize) != NULL) {
        return 1;
    }
    free(rom);

    buf = (int16_t *)malloc(sizeof(int16_t) * 2 * 4096);
    pac = (uint8_t *)malloc(sizeof(int16_t) * 2 * 4096);

    totalFrames = ((uint64_t)id6.total_len) / 2;
    fadeFrames = ((uint64_t)id6.fade) / 2;

    out = fopen(outFile,"wb");
    if(out == NULL) {
        fprintf(stderr,"Error opening %s\n",outFile);
        return 1;
    }
    spc_clear_echo(spc);
    spc_filter_clear(filter);
    spc_filter_set_gain(filter,amp);

    fprintf(stderr,"Decoding %s to %s\n",argv[0],outFile);
    fprintf(stderr,"Applying gain: 0x%04x (%s)\n",amp, amp == 0x180 ? "default" : "custom");
    fprintf(stderr,"Length: %s\n",frame_to_time(totalFrames));
    fprintf(stderr,"  Play length: %s\n",frame_to_time(((uint64_t)id6.play_len) / 2));
    fprintf(stderr,"  Fade length: %s\n",frame_to_time(fadeFrames));
    fprintf(stderr,"Title: %s\n",id6.song);
    fprintf(stderr,"Game: %s\n",id6.game);
    fprintf(stderr,"Artist: %s\n",id6.artist);
    fprintf(stderr,"Dumper: %s\n",id6.dumper);
    fprintf(stderr,"Comment: %s\n",id6.comment);
    fprintf(stderr,"Publisher: %s\n",id6.publisher);
    fprintf(stderr,"Year: %d\n",id6.year);

    write_wav_header(out,totalFrames);
    while(totalFrames) {
        fc = totalFrames < 4096 ? totalFrames : 4096;
        spc_play(spc,fc * 2,buf);
        spc_filter_run(filter,buf,fc*2);
        fade_frames(buf,totalFrames,fadeFrames,fc);
        pack_frames(pac,buf,fc);
        write_frames(out,pac,fc);
        totalFrames -= fc;
    }
    fclose(out);

    spc_delete(spc);
    spc_filter_delete(filter);

    free(buf);
    free(pac);
    free(outFile);
    return 0;
}

static uint8_t *slurp(char *filename, uint32_t *size) {
    uint8_t *buf;
    FILE *f = fopen(filename,"rb");
    if(f == NULL) {
        fprintf(stderr,"Error opening %s: %s\n",
          filename,
          strerror(errno));
        return NULL;
    }
    fseek(f,0,SEEK_END);
    *size = ftell(f);
    fseek(f,0,SEEK_SET);

    buf = (uint8_t *)malloc(*size);
    if(buf == NULL) {
        fprintf(stderr,"out of memory\n");
        return NULL;
    }
    if(fread(buf,1,*size,f) != *size) {
        fprintf(stderr,"error reading file\n");
        free(buf);
        return NULL;
    }
    return buf;
}

static void pack_int16le(uint8_t *d, int16_t n) {
    d[0] = (uint8_t)(n      );
    d[1] = (uint8_t)(n >> 8 );
}

static void pack_uint16le(uint8_t *d, uint16_t n) {
    d[0] = (uint8_t)(n      );
    d[1] = (uint8_t)(n >> 8 );
}

static void pack_uint32le(uint8_t *d, uint32_t n) {
    d[0] = (uint8_t)(n      );
    d[1] = (uint8_t)(n >> 8 );
    d[2] = (uint8_t)(n >> 16);
    d[3] = (uint8_t)(n >> 24);
}

static int write_wav_header(FILE *f, uint64_t totalFrames) {
    unsigned int dataSize = totalFrames * sizeof(int16_t) * CHANNELS;
    uint8_t tmp[4];
    if(fwrite("RIFF",1,4,f) != 4) return 0;
    pack_uint32le(tmp,dataSize + 44 - 8);
    if(fwrite(tmp,1,4,f) != 4) return 0;

    if(fwrite("WAVE",1,4,f) != 4) return 0;
    if(fwrite("fmt ",1,4,f) != 4) return 0;

    pack_uint32le(tmp,16); /*fmtSize */
    if(fwrite(tmp,1,4,f) != 4) return 0;

    pack_uint16le(tmp,1); /* audioFormat */
    if(fwrite(tmp,1,2,f) != 2) return 0;

    pack_uint16le(tmp,CHANNELS); /* numChannels */
    if(fwrite(tmp,1,2,f) != 2) return 0;

    pack_uint32le(tmp,SAMPLE_RATE);
    if(fwrite(tmp,1,4,f) != 4) return 0;

    pack_uint32le(tmp,SAMPLE_RATE * CHANNELS * sizeof(int16_t));
    if(fwrite(tmp,1,4,f) != 4) return 0;

    pack_uint16le(tmp,CHANNELS * sizeof(int16_t));
    if(fwrite(tmp,1,2,f) != 2) return 0;

    pack_uint16le(tmp,sizeof(int16_t) * 8);
    if(fwrite(tmp,1,2,f) != 2) return 0;

    if(fwrite("data",1,4,f) != 4) return 0;

    pack_uint32le(tmp,dataSize);
    if(fwrite(tmp,1,4,f) != 4) return 0;

    return 1;
}

static void
fade_frames(int16_t *data, unsigned int framesRem, unsigned int framesFade, unsigned int frameCount) {
    unsigned int i = 0;
    unsigned int f = framesFade;
    double fade;

    if(framesRem - frameCount > framesFade) return;
    if(framesRem > framesFade) {
        i = framesRem - framesFade;
        f += i;
    } else {
        f = framesRem;
    }

    while(i<frameCount) {
        fade = (double)(f-i) / (double)framesFade;
        data[(i*2)+0] *= fade;
        data[(i*2)+1] *= fade;
        i++;
    }

    return;
}

static void pack_frames(uint8_t *d, int16_t *s, unsigned int frameCount) {
    unsigned int i = 0;
    while(i<frameCount) {
        pack_int16le(&d[0],s[(i*2)+0]);
#if CHANNELS == 2
        pack_int16le(&d[sizeof(int16_t)],s[(i*2)+1]);
#endif
        i++;
        d += (sizeof(int16_t) * CHANNELS);
    }
}

static int write_frames(FILE *f, uint8_t *d, unsigned int frameCount) {
    return fwrite(d,sizeof(int16_t) * CHANNELS,frameCount,f) == frameCount;
}

unsigned int scan_uint(const char *str) {
    const char *s = str;
    unsigned int num = 0;
    while(*s) {
        if(*s < 48 || *s > 57) break;
        num *= 10;
        num += (*s - 48);
        s++;
    }

    return num;
}

static const char *frame_to_time(uint64_t frames) {
    unsigned int mill;
    unsigned int sec;
    unsigned int min;
    time_buf[0] = 0;
    frames /= 32;
    mill = frames % 1000;
    frames /= 1000;
    sec = frames % 60;
    frames /= 60;
    min = frames;

    sprintf(time_buf,"%02d:%02d.%03d",min,sec,mill);
    return time_buf;
}
