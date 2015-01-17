/* sndgrep: Search for specific frequencies (tones) in PCM audio.
 * Copyright 2015 enferex <mattdavis9@gmail.com>
 *
 * sndgrep is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * sndgrep is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with sndgrep.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fftw3.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <sys/stat.h>
#ifdef DEBUG_PLAYSOUND
#include <alsa/asoundlib.h>
#endif


#define RATE            8000 /* 8.0kHz */
#define PI2             (M_PI*2.0)
#define TAG             "sndgrep"
#define DTMF_LO         0 /* Low frequency index  */
#define DTMF_HI         1 /* High frequency index */
#define DTMF_BAD_DIGIT -1 /* -1 isnt a key :-)    */


/* Bit-maskable flags */
#define FLAG_NONE    0
#define FLAG_VERBOSE 1
#define FLAG_DTMF    2


#define ERR(...) \
do {                                      \
    fprintf(stderr, TAG": " __VA_ARGS__); \
    fputc('\n', stderr);                  \
    exit(EXIT_FAILURE);                   \
} while ( 0 )


#define ARRAY_LEN(_a) sizeof((_a))/sizeof((_a)[0])


static void usage(const char *execname)
{
    printf("Usage: %s <--search | --generate> [-t tone] [--dtmf] "
           "[-d duration] [file]\n"
           "  --search:    Search for '-t' tone\n"
           "  --generate:  Generate '-t' tone\n"
           "  -t tone:     Search or generate tone "
           "(in Hz, or if --dtmf the corresponding dtmf digit)\n"
           "  -d duration: Duration (seconds) of tone to generate or search\n"
           "  file:        File (search for or generate tone to this)\n",
           execname);
    exit(EXIT_SUCCESS);
}


/* https://en.wikipedia.org/wiki/Dual-tone_multi-frequency_signaling */
static const float dtmf[][2] = 
{
    {941.0f, 1336.0f}, /* 0 */
    {697.0f, 1209.0f}, /* 1 */
    {697.0f, 1336.0f}, /* 2 */
    {697.0f, 1477.0f}, /* 3 */
    {770.0f, 1209.0f}, /* 4 */
    {770.0f, 1336.0f}, /* 5 */
    {770.0f, 1477.0f}, /* 6 */
    {852.0f, 1209.0f}, /* 7 */
    {852.0f, 1336.0f}, /* 8 */
    {852.0f, 1477.0f}, /* 9 */
};


/* Each sample of audio is... */
typedef double frame_t;


/* Sets the number of bytes used in 'size' */
static frame_t *make_dtmf(float secs, int key, size_t *size)
{
    int i;
    double a, b, c;
    frame_t *buf;

    assert(size);
    *size = sizeof(frame_t) * secs * RATE;
    buf = fftw_malloc(*size);

    /* Tone generator: DTMF
     * RATE : Samples per second
     * buf  : All the samples needed to supply audio for 'secs' seconds 
     * a    : Low DTMF tone at point 'i' 
     * b    : High DTMF tone at point 'i' 
     *
     * tone at sample 'i' = sin(i * ((2*PI) * freq/RATE));
     *
     * More info at:
     * https://stackoverflow.com/questions/1399501/generate-dtmf-tones
     */
    for (i=0; i<(secs * RATE); ++i)
    {
        a = sin(i*(PI2 * (dtmf[key][0]/8000.0)));
        b = sin(i*(PI2 * (dtmf[key][1]/8000.0)));
        c = ((a + b) * (double)(SHRT_MAX / 2.0));
        buf[i] = c;
#ifdef DEBUG_OUTPUT
        printf("%f\n", buf[i]);
        fflush(NULL);
#endif
    }

    return buf;
}


static void dump_dtmf(fftw_complex *fft)
{
    int i;
    float lo, hi;

    for (i=0; i<ARRAY_LEN(dtmf); ++i)
    {
        lo = dtmf[i][0];
        hi = dtmf[i][1];
        printf("==> Tone %d (%.02fHz, %.02fHz): "
               "Found (Low: %.02fHz, High: %.02fHz) <==\n",
               i, lo, hi, 
               fft[(int)lo][1], fft[(int)hi][1]);
    }
}


/* Search the DTMF tone bins and find the closest dtmf (hi and lo) that match.
 * Returns the DTMF key that generates the two tones together.
 */
static int find_dtmf_digit(fftw_complex *fft)
{
    int i, lo_idx, hi_idx;
    double lo, hi;

    /* Find match */
    for (i=0; i<ARRAY_LEN(dtmf); ++i)
    {
        lo_idx = (int)dtmf[i][DTMF_LO];
        hi_idx = (int)dtmf[i][DTMF_HI];
        lo = fabs(fft[lo_idx][1]);
        hi = fabs(fft[hi_idx][1]);
        if (lo > 1.0 && hi > 1.0)
          return i;
    };

    return DTMF_BAD_DIGIT;
}


/* Returns true if 'tone' is found */
static _Bool find_tone(int tone, int n, fftw_complex *fft, int flags)
{
    /* Take the FFT index (bin) and convert back to a frequency
     * The freq represented by each bin is:
     * freq = index * sample_rate / num_samples
     * there are num_samples indexes or bins.
     *
     * Thanks to:
     * https://stackoverflow.com/questions/4364823/how-to-get-frequency-from-fft-result
     */
    if (flags & FLAG_DTMF)
    {
        int digit = find_dtmf_digit(fft);
        if (digit != DTMF_BAD_DIGIT)
        {
            printf("==> DTMF Key %d <==\n", digit);
            return true;
        }
    }
    else
    {
        printf("Searched tone(%f)  ==>  ", (float)tone);
        if (tone > n || tone < n)
        {
            printf("N/A (choose a frequency less than %.02fHz\n", (float)n);
            return false;
        }
        else
          printf("(Real: %fHz, Imag: %f)Hz\n", fft[tone][0], fft[tone][1]);
        return true;
    }

    /* Should not get here */
    return false;
}


static void gen_tone(const char *fname, float secs, int tone, int flags)
{
    FILE *fp;
    size_t size;
    frame_t *data;

    if (secs <= 0)
      ERR("Unsupported duration value: %f", secs);

    if (flags & FLAG_DTMF)
      printf("==> Writing DTMF key tone %d for %.02f second%s to %s...\n",
             tone, secs, (secs == 1.0f) ? "" : "s", fname);

    data = make_dtmf(secs, tone, &size);

    /* If no fname, use stdout */
    if (fname && !(fp = fopen(fname, "wb")))
      ERR("Could not open %s for writing", fname);
    else if (!fname)
      fp = stdout;

    if (!fp)
      ERR("Could not locate an output file or stdout to write to");

    if (fwrite(data, 1, size, fp) != size)
      ERR("Error writing data to %s", fname);

    /* Based on the simple ALSA example:
     * http://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm_min_8c-example.html
     */
#ifdef DEBUG_PLAYSOUND
    {
        snd_pcm_t *dev;
        snd_pcm_sframes_t frames;
        if (snd_pcm_open(&dev, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
          ERR("Error opening the default sound device");
        if (snd_pcm_set_params(dev, SND_PCM_FORMAT_FLOAT64,
                               SND_PCM_ACCESS_RW_INTERLEAVED,
                               1, RATE, 1, 0) < 0)
          ERR("Error setting audio device options");
        if ((frames = snd_pcm_writei(dev, data, size) < 0))
          frames = snd_pcm_recover(dev, frames, 0);
        if (frames < 0)
          ERR("Error writing to the sound device");
        snd_pcm_close(dev);
    }
#endif /* DEBUG_PLAYSOUND */
    fclose(fp);
    free(data);
}


static void analyize_tone(const char *fname, float secs, int tone, int flags)
{
    int i, n_samples, found;
    double *data;
    frame_t frame;
    FILE *fp;
    struct stat stat;
    size_t size, byte, ret;
    fftw_plan plan;
    fftw_complex *out;
    const size_t frame_sz  = sizeof(double);

    if (!fname)
      fp = stdin;
    else if (!(fp = fopen(fname, "rb")))
      ERR("Could not open %s for reading", fname);

    /* Assume the input is a series of frame_t */
    found = 0;
    do 
    {
        if (fp != stdin) 
        {
            fstat(fileno(fp), &stat);
            size = stat.st_size;
        }
        else
          size = (RATE * frame_sz); /* One second of 8kHz */
            
        n_samples = ceil((double)size / (double)frame_sz);

        /* Suck in the data */
        size = frame_sz * n_samples;
        data = malloc(sizeof(double) * (size/sizeof(frame_t)));
        byte = 0;

        for (i=0; i<size/sizeof(frame_t); ++i)
        {
            ret = fread(&frame, 1, sizeof(frame), fp);
            if (ret == -1 || ((fp!=stdin && ret!=sizeof(frame))))
              ERR("Error extracting data from input stream: "
                  "read %zd of %zd bytes", ret, sizeof(frame));
            byte += sizeof(frame);
            data[i] = (double)frame;
#ifdef DEBUG_OUTPUT
            printf("%f\n", (frame_t)data[i]);
            fflush(NULL);
#endif
        }

        out = fftw_malloc(sizeof(fftw_complex) * n_samples);
        plan = fftw_plan_dft_r2c_1d(n_samples, data, out, FFTW_ESTIMATE);
        fftw_execute(plan);

        found += find_tone(tone, secs * RATE, out, flags);

        fftw_destroy_plan(plan);
        fftw_free(out);
        fftw_free(data);
    } while (fp == stdin && !feof(stdin) && !ferror(stdin));

    if (!found)
      printf("==> Tone could not be located\n");

    if (flags & FLAG_VERBOSE)
      dump_dtmf(out);

    fclose(fp);
}


int main(int argc, char **argv)
{
    int i, tone, do_gen_tone, flags;
    float secs;
    const char *fname;
    static const struct option opts[] = 
    {
        {"generate", no_argument,       NULL, 'g'},
        {"search",   no_argument,       NULL, 's'},
        {"tone",     required_argument, NULL, 't'},
        {"duration", required_argument, NULL, 'd'},
        {"help",     no_argument,       NULL, 'h'},
        {"dtmf",     no_argument,       NULL, 'D'},
        {"verbose",  no_argument,       NULL, 'v'},
        {NULL, 0, NULL, 0},
    };

    do_gen_tone = -1;
    secs = 0.0f;
    tone = 0;
    fname = NULL;
    flags = FLAG_NONE;

    while ((i=getopt_long(argc, argv, "-t:d:gshxv", opts, NULL)) != -1)
    {
        switch (i)
        {
            case   1: fname = optarg; break;
            case 'g': do_gen_tone = true; break;
            case 's': do_gen_tone = false; break;
            case 't': tone = atoi(optarg); break; 
            case 'd': secs = atoi(optarg); break;
            case 'D': flags |= FLAG_DTMF; break;
            case 'h': usage(argv[0]); break;
            case 'v': flags |= FLAG_VERBOSE; break;
            default:  exit(EXIT_FAILURE); break;
        }
    }

    if (do_gen_tone == -1)
      usage(argv[0]);
    else if (do_gen_tone)
      gen_tone(fname, secs, tone, flags);
    else
    {
        analyize_tone(fname, secs, tone, flags);
    }

    return 0;
}
