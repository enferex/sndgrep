#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <getopt.h>
#include <math.h>
#include <limits.h>
#include <fftw3.h>


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


/* Sets the number of bytes used in 'size' */
static double *make_dtmf(float secs, int key, size_t *size)
{
    int i;
    double a, b, *buf;

    assert(size);
    *size = sizeof(double) * secs * RATE;
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
        buf[i] = (a + b) * (double)(SHRT_MAX / 2.0);
    }

    return buf;
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

        printf("[%d]: %fHz (value: %f)\n", i, (float)i, fft[i][1]);
    }

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


int main(int argc, char **argv)
{
    int i, tone, do_gen_tone;
    float secs;
    double *data;
    const char *fname;
    FILE *fp;
    size_t size;
    fftw_plan plan;
    fftw_complex *out;

    do_gen_tone = -1;
    secs = 0.0f;
    tone = 0;
    fname = NULL;
    data = NULL;

    while ((i=getopt(argc, argv, "-T:t:d:h")) != -1)
    {
        switch (i)
        {
            case   1: fname = optarg; break;
            case 'T': tone = atoi(optarg); do_gen_tone = true; break;
            case 't': tone = atoi(optarg); do_gen_tone = false; break;
            case 'd': secs = atoi(optarg); break;
            case 'h': usage(argv[0]); break;
            default:
                fprintf(stderr, "Unsupported argument: %s\n", optarg);
                exit(EXIT_FAILURE);
        }
    }

    if (secs <= 0)
    {
        fprintf(stderr, "Unsupported duration value: %f\n", secs);
        exit(EXIT_FAILURE);
    }

    if (do_gen_tone)
    {
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

        fclose(fp);
        free(data);
    }
    else /* Else: analyize tone... default operation */
    {
        out = fftw_malloc(sizeof(fftw_complex) * secs * RATE);
        plan = fftw_plan_dft_r2c_1d(secs * RATE, data, out, FFTW_ESTIMATE);
        fftw_execute(plan);
        report_tones(secs * RATE, out);
        fftw_destroy_plan(plan);
        fftw_free(out);
        fftw_free(data);
    }

    return 0;
}
