/**
 *
 *      Chris Egerton
 *      November 2015
 *      sound.c
 *      Generates .wav files.
 *
**/

#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <limits.h>


#define PI (3.14159265358979323846264338327950288419716939937)
#define CHUNK_ID        "RIFF"
#define FORMAT          "WAVE"
#define SUBCHUNK1_ID    "fmt "
#define SUBCHUNK1_SIZE  (16)
#define AUDIO_FORMAT    (1)
#define NUM_CHANNELS    (1)
#define BITS_PER_SAMPLE (16)
#define BYTE_RATE       (sample_rate * NUM_CHANNELS * BITS_PER_SAMPLE / 8)
#define BLOCK_ALIGN     (NUM_CHANNELS * BITS_PER_SAMPLE / 8)
#define SUBCHUNK2_ID    "data"


void usage(int);
int process_flags(int, char **);
void process_wave_opt(const char *);
long parse_int_opt(const char *, const char *, long, long);
long double parse_float_opt(const char *, const char *, long double,
                            long double);
uint32_t get_num_samples(uint32_t);
int16_t *create_samples(long double *, uint8_t, long double, uint32_t,
                        long double(long double, uint32_t));
long double sine_wave_function(long double, uint32_t);
long double square_wave_function(long double, uint32_t);
long double triangle_wave_function(long double, uint32_t);
long double sawtooth_wave_function(long double, uint32_t);
long double point_wave_function(long double, uint32_t);
long double circle_wave_function(long double, uint32_t);
void write_sound_file(const int16_t *, uint32_t);
void write_int_data(size_t, uint8_t);
void checked_fputc(unsigned char, FILE *);
void checked_fprintf(FILE *, const char *, ...);


/* Used for calls to perror(). */
static const char *program_name;
static const char *const default_program_name = "sound";

/* The duration (in milliseconds) of the sound. */
static uint32_t duration;
static const uint32_t default_duration = 1000;

/**
 *  The amplitude of the sound.
 *  Specifically, a constant that each sample is multiplied by.
**/
static long double amplitude;
static const long double default_amplitude = 33.333333;

/* The number of overtones to create above each frequency. */
static int8_t num_overtones;
static const int8_t default_num_overtones = 0;

/* The type of wave to produce */
static long double (*wave_function)(long double, uint32_t);
static long double (*const default_wave_function)(long double, uint32_t) =
    sine_wave_function;

/* The name of the default wave. Used in usage message. */
static const char *const default_wave_function_name = "sine";

/* The number of samples per second to capture */
static uint32_t sample_rate;
static const uint32_t default_sample_rate = 44100;

/* The file to write the produced sound to. */
static FILE *out;

/**
 *  The name of the FILE * specified by <out>.
 *  Stored for potential use in error messages.
**/
static const char *out_name;
static const char *const default_out_name = "stdout";


int main(int argc, char **argv) {
    int argindex = process_flags(argc, argv);
    uint32_t num_samples = get_num_samples(duration);
    int num_pitches = argc - argindex;
    int num_frequencies = (num_overtones + 1) * num_pitches;
    long double frequencies[num_frequencies];
    for(int p = 0; p < num_pitches; p++) {
        long double fundamental = parse_float_opt(argv[p + argindex],
                                                  "Frequency", 1, 30000);
        for(int o = 0; o <= num_overtones; o++) {
            frequencies[(p * (num_overtones + 1)) + o] = (o + 1) * fundamental;
        }
    }
    int16_t *samples = create_samples(frequencies, num_frequencies, amplitude,
                                      num_samples, wave_function);
    write_sound_file(samples, num_samples);
    free(samples);
    return 0;
}

/**
 *  Prints the program usage message to stderr, then exits with the specified
 *  value.
**/
void usage(int exit_value) {
    fprintf(stderr, "usage: %s "
                    "[-f|--file <file=%s>] "
                    "[-d|--duration <duration=%u>] "
                    "[-a|--amplitude <amplitude=%Lf>] "
                    "[-s|--sample-rate <sample-rate=%u] "
                    "[-w|--wave-function <wave=%s>] "
                    "[-o|--overtones <overtones=%hhd>] "
                    "frequency [frequency ...]\n",
                    program_name, default_out_name, default_duration,
                    default_amplitude, default_sample_rate,
                    default_wave_function_name, default_num_overtones);
    exit(exit_value);
}

/**
 *  Uses getopt_long() to process command line flags.
 *  Should only be called with <argc> and <argv> from main().
 *  Returns the first index in <argv> that contains a legitimate, non-flag
 *  argument.
**/
int process_flags(int argc, char **argv) {
    program_name = (argc && argv[0] && argv[0][0]) ?
                   argv[0] : default_program_name;
    out = stdout;
    out_name = default_out_name;
    duration = default_duration;
    amplitude = default_amplitude;
    sample_rate = default_sample_rate;
    wave_function = default_wave_function;
    num_overtones = default_num_overtones;
    struct option options[] = {
        {"file",            required_argument,  NULL,   'f'},
        {"duration",        required_argument,  NULL,   'd'},
        {"amplitude",       required_argument,  NULL,   'a'},
        {"sample-rate",     required_argument,  NULL,   's'},
        {"wave-function",   required_argument,  NULL,   'w'},
        {"overtones",       required_argument,  NULL,   'o'},
        {"help",            no_argument,        NULL,   'h'},
        {0, 0, 0, 0}
    };
    for(;;) {
        int c = getopt_long(argc, argv, "f:d:a:s:w:o:h", options, &optind);
        switch(c) {
            case -1:
                if(optind >= argc) {
                    fprintf(stderr, "%s: At least one frequency required.\n",
                            program_name);
                    usage(1);
                }
                return optind;
            case 'f':
                if(!(out = fopen(optarg, "w"))) {
                    fprintf(stderr, "%s: %s: %s.\n", program_name, optarg,
                            strerror(errno));
                    exit(1);
                }
                out_name = optarg;
                break;
            case 'd':
                duration = parse_int_opt(optarg, "Duration", 1, UINT32_MAX);
                break;
            case 'a':
                amplitude = parse_float_opt(optarg, "Amplitude",
                                           (long double)100 / INT16_MAX, 100);
                break;
            case 's':
                sample_rate = parse_int_opt(optarg, "Sample rate", 1,
                                            UINT32_MAX);
                break;
            case 'w':
                process_wave_opt(optarg);
                break;
            case 'o':
                num_overtones = parse_int_opt(optarg, "Overtones", 0,
                    INT8_MAX);
                break;
            case '?':
                usage(1);
            case 'h':
                usage(0);
            default:
                fprintf(stderr, "%s: Unrecognized getopt() return value: %c.\n",
                        program_name, c);
                usage(1);
        }
    }
}

/**
 *  Processes a command-line wave function specification.
**/
void process_wave_opt(const char *opt) {
    if(!strcmp(opt, "sine")) {
        wave_function = sine_wave_function;
    }else if(!strcmp(opt, "square")) {
        wave_function = square_wave_function;
    } else if(!strcmp(opt, "triangle")) {
        wave_function = triangle_wave_function;
    } else if(!strcmp(opt, "sawtooth")) {
        wave_function = sawtooth_wave_function;
    } else if(!strcmp(opt, "point")) {
        wave_function = point_wave_function;
    } else if(!strcmp(opt, "circle")) {
        wave_function = circle_wave_function;
    }  else {
        fprintf(stderr, "%s: Wave function must be one of 'sine', 'square', "
                "'triangle', 'sawtooth', 'point', or 'circle'.\n",
                program_name);
        usage(1);
    }
}

/**
 *  Parses <opt> as a long, then returns its value.
 *  <optname> is the name of the option, should an error occur, and <optmin> and
 *  <optmax> are the lower and upper bounds for allowed result values.
**/
long parse_int_opt(const char *opt, const char *optname, long optmin,
                   long optmax) {
    errno = 0;
    char *c;
    long result = strtol(opt, &c, 10);
    if(errno && errno != ERANGE) {
        perror(program_name);
        usage(1);
    } else if(*c) {
        fprintf(stderr, "%s: %s must be an integer.\n", program_name, optname);
        usage(1);
    } else if(errno == ERANGE || result < optmin || result > optmax) {
        fprintf(stderr, "%s: %s must be in the range [%ld, %ld].\n",
                program_name, optname, optmin, optmax);
        usage(1);
    } else {
        return result;
    }
    // Should never happen, but put here to silence compiler warnings.
    fprintf(stderr, "%s: Fatal error encountered. Exiting...\n", program_name);
    exit(1);
}

/**
 *  Parses <opt> as a long double, then returns its value.
 *  <optname> is the name of the option, should an error occur, and <optmin> and
 *  <optmax> are the inclusive lower and upper bounds for allowed result values.
**/
long double parse_float_opt(const char *opt, const char *optname,
                            long double optmin, long double optmax) {
    errno = 0;
    char *c;
    long double result = strtold(opt, &c);
    if(errno && errno != ERANGE) {
        perror(program_name);
        usage(1);
    } else if(*c) {
        fprintf(stderr, "%s: %s must be a number.\n", program_name, optname);
        usage(1);
    } else if(errno == ERANGE || result < optmin || result > optmax) {
        fprintf(stderr, "%s: %s must be in the range [%Lf, %Lf].\n",
                program_name, optname, optmin, optmax);
        usage(1);
    } else {
        return result;
    }
    // Should never happen, but put here to silence compiler warnings.
    fprintf(stderr, "%s: Fatal error encountered. Exiting...\n", program_name);
    exit(1);
}

/**
 *  Returns the number of samples required to cover <duration> milliseconds,
 *  accounting for possible truncation.
 *  If an overflow would occur, print an error message and exit.
**/
uint32_t get_num_samples(uint32_t duration) {
    size_t result = (duration / 1000) * sample_rate;
    if(result > UINT32_MAX) {
        fprintf(stderr, "%s: Duration of %u and sample rate of %u combine to "
                        "create a file that is too large to store in WAVE "
                        "format.\n", program_name, duration, sample_rate);
    }
    if((duration * sample_rate) % 1000) {
        return result + 1;
    } else {
        return result;
    }
}

/**
 *  Create a sample array representing each pitch in the array <frequencies>
 *  of length <num_frequencies> being played for <duration> samples with a
 *  maximum value of <amplitude> for each sample and a given <wave_function>.
 *
 *  Return the resulting array.
**/
int16_t *create_samples(long double *frequencies, uint8_t num_frequencies,
                        long double amplitude, uint32_t num_samples,
                        long double (*wave_function)(long double, uint32_t)) {
    int16_t *result = malloc(sizeof(uint16_t) * num_samples);
    for(uint32_t t = 0; t < num_samples; t++) {
        long double sample = 0;
        for(uint8_t f = 0; f < num_frequencies; f++) {
            sample += ((amplitude / 100) * INT16_MAX *
                      wave_function(frequencies[f], t)) / num_frequencies;
        }
        result[t] = (int16_t)sample;
    }
    return result;
}

/**
 *  Returns a sample of the sine wave of a given frequency at a given time.
**/
long double sine_wave_function(long double frequency, uint32_t time) {
    long double x = (2 * PI * time * frequency) / sample_rate;
    return sinl(x);
}

/**
 *  Returns a sample of the square wave of a given frequency at a given time.
**/
long double square_wave_function(long double frequency, uint32_t time) {
    long double x = (2 * time * frequency) / sample_rate;
    return ((size_t)x % 2) ? 1 : -1;
}

/**
 *  Returns a sample of the triangle wave of a given frequency at a given time.
**/
long double triangle_wave_function(long double frequency, uint32_t time) {
    long double x = (4 * time * frequency) / sample_rate;
    return (x - (2 * floor((x + 1) / 2))) *
           (((size_t)((x + 1) / 2) % 2) ? -1 : 1);
}

/**
 *  Returns a sample of the sawtooth wave of a given frequency at a given time.
**/
long double sawtooth_wave_function(long double frequency, uint32_t time) {
    long double x = (time * frequency) / sample_rate;
    return 2 * (x - floor(x)) - 1;
}

/**
 *  Returns a sample of a point wave of a given frequency at a given time.
**/
long double point_wave_function(long double frequency, uint32_t time) {
    long double x = (4 * time * frequency) / sample_rate;
    long double root = x - (1 + (floor(x / 2) * 2));
    return (1 - sqrt(1 - (root * root))) *
           (((size_t)((x + 1) / 2) % 2) ? -1 : 1);
}

/**
 *  Returns a sample of a circle wave of a given frequency at a given time.
**/
long double circle_wave_function(long double frequency, uint32_t time) {
    long double x = (4 * time * frequency) / sample_rate;
    long double root = x - (floor(x / 2) * 2) - 1;
    return sqrt(1 - (root * root)) * (((size_t)((x + 1) / 2) % 2) ? -1 : 1);
}


/**
 *  Write <length> samples specified in the array <data> to the output file.
**/
void write_sound_file(const int16_t *data, uint32_t length) {
    checked_fprintf(out, "%s", CHUNK_ID);
    write_int_data(36 + length, 4);
    checked_fprintf(out, "%s", FORMAT);
    checked_fprintf(out, "%s", SUBCHUNK1_ID);
    write_int_data(SUBCHUNK1_SIZE, 4);
    write_int_data(AUDIO_FORMAT, 2);
    write_int_data(NUM_CHANNELS, 2);
    write_int_data(sample_rate, 4);
    write_int_data(BYTE_RATE, 4);
    write_int_data(BLOCK_ALIGN, 2);
    write_int_data(BITS_PER_SAMPLE, 2);
    checked_fprintf(out, "%s", SUBCHUNK2_ID);
    write_int_data(length * NUM_CHANNELS * BITS_PER_SAMPLE / 8, 4);
    for(uint32_t i = 0; i < length; i++) {
        write_int_data(data[i], 2);
    }
}

/**
 *  Write <num_bytes> of the integer given in <data> to the output file byte by
 *  byte in ascending order from least to most significant bytes.
**/
void write_int_data(size_t data, uint8_t num_bytes) {
    for(uint8_t i = 0; i < num_bytes; i++, data >>= CHAR_BIT) {
        checked_fputc(data & UCHAR_MAX, out);
    }
}

/**
 *  Writes <byte> to <file>. If failure is detected, prints an error message and
 *  exits the program.
**/
void checked_fputc(unsigned char byte, FILE *file) {
    if(fputc(byte, file) == EOF) {
        fprintf(stderr, "%s: %s: Write failed.\n", program_name, out_name);
        exit(1);
    }
}

/**
 *  Calls fprintf on <file> with all supplied arguments. If failure is detected,
 *  prints an error message and exits the program.
**/
void checked_fprintf(FILE *file, const char *format, ...) {
    va_list arg_list;
    va_start(arg_list, format);
    if(vfprintf(file, format, arg_list) < 0) {
        va_end(arg_list);
        fprintf(stderr, "%s: %s: Write failed.\n", program_name, out_name);
        exit(1);
    }
    va_end(arg_list);
}
