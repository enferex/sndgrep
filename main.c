#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <fftw3.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <sys/stat.h>
#ifdef DEBUG_PLAYSOUND
#include <alsa/asoundlib.h>
#endif


#define RATE 8000      /* 8.0kHz */
#define PI2 (M_PI*2.0)
#define TAG "sndgrep"


#define ERR(...) \
do {                                      \
    fprintf(stderr, TAG": " __VA_ARGS__); \
    fputc('\n', stderr);                  \
    exit(EXIT_FAILURE);                   \
} while ( 0 )


static void usage(const char *execname)
{
    printf("Usage: %s [-t tone | -T tone] [-d duration] [file]\n"
           "  -t tone:     Search for tone (in Hz)\n"
           "  -T tone:     Generate tone (in Hz)\n"
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

    for (i=0; i<sizeof(dtmf)/sizeof(dtmf[0]); ++i)
    {
        lo = dtmf[i][0];
        hi = dtmf[i][1];
        printf("Tone %d (%.02fHz, %.02fHz): "
               "Found (%.02fHz, %.02fHz :: %.02fHz, %.02fHz\n",
               i, lo, hi, 
               fft[(int)lo][0], fft[(int)lo][1],
               fft[(int)hi][0], fft[(int)hi][1]);
    }
}


static void report_tones(int n, fftw_complex *fft)
{
    int i, idx;
    double min;

    min = fft[0][1];
    idx = 0;

    for (i=0; i<n; ++i)
    {
        if (fft[i][1] < min)
        {
            min = fft[i][1];
            idx = i;
        }
        /* printf("[%d]: %fHz (value: %f)\n", i, (float)i, fft[i][1]); */
    }

    dump_dtmf(fft);

    /* Take the FFT index (bin) and convert back to a frequency
     * The freq represented by each bin is:
     * freq = index * sample_rate / num_samples
     * there are num_samples indexes or bins.
     *
     * Thanks to:
     * https://stackoverflow.com/questions/4364823/how-to-get-frequency-from-fft-result
     */
    printf("Frequency: %fHz", (double)idx);
}


static void gen_tone(const char *fname, float secs, int tone)
{
    FILE *fp;
    size_t size;
    frame_t *data;

    if (secs <= 0)
      ERR("Unsupported duration value: %f", secs);

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


static void analyize_tone(const char *fname, float secs, int tone)
{
    int i, n_samples;
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
            if (ret != sizeof(frame))
              ERR("Error extracting data from input stream: "
                  "read %zd of %zd bytes", ret, sizeof(frame));
            byte += sizeof(frame);
            data[i] = (double)frame;
#ifdef DEBUG_OUTPUT
            printf("%f\n", (frame_t)data[i]);
            fflush(NULL);
#endif
        }

        //byte = fread(data, 1, size, fp);
        //if ((fp != stdin) && (byte != size))
        //  ERR("Error extracting %d samples from output file", n_samples);

        out = fftw_malloc(sizeof(fftw_complex) * n_samples);
        plan = fftw_plan_dft_r2c_1d(n_samples, data, out, FFTW_ESTIMATE);
        fftw_execute(plan);

        report_tones(secs * RATE, out);
        fftw_destroy_plan(plan);
        fftw_free(out);
        fftw_free(data);
    } while (fp == stdin && !feof(stdin) && !ferror(stdin));

    fclose(fp);
}


int main(int argc, char **argv)
{
    int i, tone;
    _Bool do_gen_tone;
    float secs;
    const char *fname;

    do_gen_tone = -1;
    secs = 0.0f;
    tone = 0;
    fname = NULL;

    while ((i=getopt(argc, argv, "-T:t:d:h:x")) != -1)
    {
        switch (i)
        {
            case   1: fname = optarg; break;
            case 'T': tone = atoi(optarg); do_gen_tone = true; break;
            case 't': tone = atoi(optarg); do_gen_tone = false; break;
            case 'd': secs = atoi(optarg); break;
            case 'h': usage(argv[0]); break;
            default:
                ERR("Unsupported argument: %s", optarg);
        }
    }

    if (do_gen_tone)
      gen_tone(fname, secs, tone);
    else
      analyize_tone(fname, secs, tone);

    return 0;
}
