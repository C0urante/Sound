/**
 *
 *      Chris Egerton
 *      November 2015
 *      sound.c
 *      Generates .wav files.
 *      Many, many thanks go to Craig Sapp (craig@ccrma.stanford.edu) for his
 *  wonderful web page http://soundfile.sapp.org/doc/WaveFormat/, which
 *  gives an idiot-proof explanation of how how .wav files are formatted.
 *      TODO: Think of what else to write here.
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

#define CHUNK_ID_SIZE           (4)
#define CHUNK_SIZE_SIZE         (4)
#define FORMAT_SIZE             (4)
#define SUBCHUNK1_ID_SIZE       (4)
#define SUBCHUNK1_SIZE_SIZE     (4)
#define AUDIO_FORMAT_SIZE       (2)
#define NUM_CHANNELS_SIZE       (2)
#define SAMPLE_RATE_SIZE        (4)
#define BYTE_RATE_SIZE          (4)
#define BLOCK_ALIGN_SIZE        (2)
#define BITS_PER_SAMPLE_SIZE    (2)
#define SUBCHUNK2_ID_SIZE       (4)
#define SUBCHUNK2_SIZE_SIZE     (4)

#define CHUNK_ID_OFFSET         (0)
#define CHUNK_SIZE_OFFSET       (4)
#define FORMAT_OFFSET           (8)
#define SUBCHUNK1_ID_OFFSET     (12)
#define SUBCHUNK1_SIZE_OFFSET   (16)
#define AUDIO_FORMAT_OFFSET     (20)
#define NUM_CHANNELS_OFFSET     (22)
#define SAMPLE_RATE_OFFSET      (24)
#define BYTE_RATE_OFFSET        (28)
#define BLOCK_ALIGN_OFFSET      (32)
#define BITS_PER_SAMPLE_OFFSET  (34)
#define SUBCHUNK2_ID_OFFSET     (36)
#define SUBCHUNK2_SIZE_OFFSET   (40)
#define DATA_OFFSET             (44)


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
void create_sound_file(const int16_t *, uint32_t);
void append_sound_file(const int16_t *, uint32_t);
void verify_int_header(const char *, size_t, size_t, uint8_t);
void verify_string_header(const char *, const char *, size_t, size_t);
void write_int_data(size_t, uint8_t);
size_t read_int_data(FILE *, uint8_t);
void checked_fputc(uint8_t, FILE *);
void checked_fprintf(FILE *, const char *, ...);
uint8_t checked_fgetc(FILE *);
void checked_fseek(FILE *, long, int);
void close_out(void);


/* Used for calls to perror(). */
static const char *program_name;
static const char *const default_program_name = "sound";

/* The duration (in milliseconds) of the sound. */
static uint32_t duration;
static const uint32_t default_duration = 1000;

/**
 *  The volume of the sound.
 *  Specifically, a constant that each sample is multiplied by.
**/
static long double volume;
static const long double default_volume = 33.333333;

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

/* Is a new file being created, or an existing one being appended to? */
static uint8_t append_mode;
static const uint8_t default_append_mode = 0;


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
    int16_t *samples = create_samples(frequencies, num_frequencies, volume,
                                      num_samples, wave_function);
    if(append_mode) {
        append_sound_file(samples, num_samples);
    } else {
        create_sound_file(samples, num_samples);
    }
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
                    "[-a|--append <file>] "
                    "[-d|--duration <duration=%u>] "
                    "[-v|--volume <volume=%Lf>] "
                    "[-s|--sample-rate <sample-rate=%u] "
                    "[-w|--wave-function <wave=%s>] "
                    "[-o|--overtones <overtones=%hhd>] "
                    "frequency [frequency ...]\n",
                    program_name, default_out_name, default_duration,
                    default_volume, default_sample_rate,
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
    append_mode = default_append_mode;
    out = stdout;
    out_name = default_out_name;
    duration = default_duration;
    volume = default_volume;
    sample_rate = default_sample_rate;
    wave_function = default_wave_function;
    num_overtones = default_num_overtones;
    struct option options[] = {
        {"file",            required_argument,  NULL,   'f'},
        {"append",          required_argument,  NULL,   'a'},
        {"duration",        required_argument,  NULL,   'd'},
        {"volume",          required_argument,  NULL,   'v'},
        {"sample-rate",     required_argument,  NULL,   's'},
        {"wave-function",   required_argument,  NULL,   'w'},
        {"overtones",       required_argument,  NULL,   'o'},
        {"help",            no_argument,        NULL,   'h'},
        {0, 0, 0, 0}
    };
    for(;;) {
        int c = getopt_long(argc, argv, "f:a:d:v:s:w:o:h", options, &optind);
        switch(c) {
            case -1:
                if(optind >= argc) {
                    fprintf(stderr, "%s: At least one frequency required.\n",
                            program_name);
                    usage(1);
                }
                return optind;
            case 'a':
                if(out != stdout) {
                    fprintf(stderr, "%s: Cannot output to multiple files.\n",
                            program_name);
                    exit(1);
                }
                append_mode = 1;
                errno = 0;
                if(!(out = fopen(optarg, "r+"))) {
                    fprintf(stderr, "%s: %s: %s.\n", program_name, optarg,
                            strerror(errno));
                    exit(1);
                }
                atexit(close_out);
                out_name = optarg;
                break;
            case 'f':
                if(out != stdout) {
                    fprintf(stderr, "%s: Cannot output to multiple files.\n",
                            program_name);
                    exit(1);
                }
                append_mode = 0;
                errno = 0;
                if(!(out = fopen(optarg, "w"))) {
                    fprintf(stderr, "%s: %s: %s.\n", program_name, optarg,
                            strerror(errno));
                    exit(1);
                }
                atexit(close_out);
                out_name = optarg;
                break;
            case 'd':
                duration = parse_int_opt(optarg, "Duration", 1, UINT32_MAX);
                break;
            case 'v':
                volume = parse_float_opt(optarg, "Amplitude",
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
    char *c;
    errno = 0;
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
    char *c;
    errno = 0;
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
    uint32_t low, high;
    if(duration > sample_rate) {
        low = sample_rate;
        high = duration;
    } else {
        low = duration;
        high = sample_rate;
    }
    if(low / 1000 > UINT32_MAX / high) {
        fprintf(stderr, "%s: Duration of %u and sample rate of %u would  "
                        "combine to create a file that is too large to store "
                        "in WAVE format.\n", program_name, duration,
                        sample_rate);
        exit(1);
    } else if(low > UINT32_MAX / high) {
        return ((high / 1000) + 1) * low;
    } else {
        return ((high * low) / 1000) + 1;
    }
}

/**
 *  Create a sample array representing each pitch in the array <frequencies>
 *  of length <num_frequencies> being played for <duration> samples with a
 *  maximum value of <volume> for each sample and a given <wave_function>.
 *
 *  Return the resulting array.
**/
int16_t *create_samples(long double *frequencies, uint8_t num_frequencies,
                        long double volume, uint32_t num_samples,
                        long double (*wave_function)(long double, uint32_t)) {
    int16_t *result = malloc(sizeof(uint16_t) * num_samples);
    for(uint32_t t = 0; t < num_samples; t++) {
        long double sample = 0;
        for(uint8_t f = 0; f < num_frequencies; f++) {
            sample += ((volume / 100) * INT16_MAX *
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
void create_sound_file(const int16_t *data, uint32_t data_length) {
    uint32_t subchunk2_size = data_length * NUM_CHANNELS * BITS_PER_SAMPLE / 8;
    checked_fprintf(out, "%s", CHUNK_ID);
    write_int_data(36 + subchunk2_size, CHUNK_SIZE_SIZE);
    checked_fprintf(out, "%s", FORMAT);
    checked_fprintf(out, "%s", SUBCHUNK1_ID);
    write_int_data(SUBCHUNK1_SIZE, SUBCHUNK1_SIZE_SIZE);
    write_int_data(AUDIO_FORMAT, AUDIO_FORMAT_SIZE);
    write_int_data(NUM_CHANNELS, NUM_CHANNELS_SIZE);
    write_int_data(sample_rate, SAMPLE_RATE_SIZE);
    write_int_data(BYTE_RATE, BYTE_RATE_SIZE);
    write_int_data(BLOCK_ALIGN, BLOCK_ALIGN_SIZE);
    write_int_data(BITS_PER_SAMPLE, BITS_PER_SAMPLE_SIZE);
    checked_fprintf(out, "%s", SUBCHUNK2_ID);
    write_int_data(subchunk2_size, SUBCHUNK2_SIZE_SIZE);
    for(uint32_t i = 0; i < data_length; i++) {
        write_int_data(data[i], 2);
    }
}


/**
 *  Write <length> samples specified in the array <data> to the end of the
 *  output file, after verifying and then adjusting the header as needed.
**/
void append_sound_file(const int16_t *new_data, uint32_t new_data_length) {
    // How much larger is the data chunk going to get?
    uint32_t subchunk2_size_addition = new_data_length * NUM_CHANNELS *
                                       BITS_PER_SAMPLE / 8;

    // How large was the previous data chunk?
    checked_fseek(out, CHUNK_SIZE_OFFSET, SEEK_SET);
    uint32_t prev_subchunk2_size = read_int_data(out, CHUNK_SIZE_SIZE) - 36;

    // Make sure that all header fields are the expected values before rewriting
    // any of them.
    verify_string_header("Chunk ID", CHUNK_ID, CHUNK_ID_OFFSET, CHUNK_ID_SIZE);
    verify_string_header("Format", FORMAT, FORMAT_OFFSET, FORMAT_SIZE);
    verify_string_header("Subchunk 1 ID", SUBCHUNK1_ID, SUBCHUNK1_ID_OFFSET,
                         SUBCHUNK1_ID_SIZE);
    verify_int_header("Subchunk 1 size", SUBCHUNK1_SIZE, SUBCHUNK1_SIZE_OFFSET,
                      SUBCHUNK1_SIZE_SIZE);
    verify_int_header("Audio format", AUDIO_FORMAT, AUDIO_FORMAT_OFFSET,
                      AUDIO_FORMAT_SIZE);
    verify_int_header("Number of channels", NUM_CHANNELS, NUM_CHANNELS_OFFSET,
                      NUM_CHANNELS_SIZE);
    verify_int_header("Sample rate", sample_rate, SAMPLE_RATE_OFFSET,
                      SAMPLE_RATE_SIZE);
    verify_int_header("Byte rate", BYTE_RATE, BYTE_RATE_OFFSET, BYTE_RATE_SIZE);
    verify_int_header("Block align", BLOCK_ALIGN, BLOCK_ALIGN_OFFSET,
                      BLOCK_ALIGN_SIZE);
    verify_int_header("Bits per sample", BITS_PER_SAMPLE,
                      BITS_PER_SAMPLE_OFFSET, BITS_PER_SAMPLE_SIZE);
    verify_string_header("Subchunk 2 ID", SUBCHUNK2_ID, SUBCHUNK2_ID_OFFSET,
                         SUBCHUNK2_ID_SIZE);
    verify_int_header("Subchunk 2 size", prev_subchunk2_size,
                      SUBCHUNK2_SIZE_OFFSET, SUBCHUNK2_SIZE_SIZE);

    // Update fields dependent on the size of the data chunk--namely, the Chunk
    // Size and Subchunk2 Size fields.
    checked_fseek(out, CHUNK_SIZE_OFFSET, SEEK_SET);
    write_int_data(prev_subchunk2_size + subchunk2_size_addition + 36,
                 CHUNK_SIZE_SIZE);
    checked_fseek(out, SUBCHUNK2_SIZE_OFFSET, SEEK_SET);
    write_int_data((new_data_length * NUM_CHANNELS * BITS_PER_SAMPLE / 8) +
                   prev_subchunk2_size, SUBCHUNK2_SIZE_SIZE);

    // Write the new data, beginning at the end of the existing data chunk.
    checked_fseek(out, DATA_OFFSET + prev_subchunk2_size, SEEK_SET);
    for(uint32_t i = 0; i < new_data_length; i++) {
        write_int_data(new_data[i], 2);
    }
}

/**
 *  Checks to make sure that the header of <file> matches the given number
 *  <field> of <size> bytes at the given position <offset>.
**/
void verify_int_header(const char *field_name, size_t field, size_t offset,
                       uint8_t size) {
    checked_fseek(out, offset, SEEK_SET);
    size_t value = read_int_data(out, size);
    if(field != value) {
        fprintf(stderr, "%s: %s: Header field '%s' appears to be corrupted.\n",
                program_name, out_name, field_name);
        fprintf(stderr, "Expected value: %zu; encountered value: %zu.\n",
                field, value);
        exit(1);
    }
}

/**
 *  Checks to make sure that the header of <file> matches the given string
 *  <field> of <size> bytes at the given position <offset>.
**/
void verify_string_header(const char *field_name, const char *field,
                          size_t offset, size_t size) {
    checked_fseek(out, offset, SEEK_SET);
    char buf[size];
    if(fread(buf, sizeof(char), size, out) <= 0) {
        fprintf(stderr, "%s: %s: Read failed.\n", program_name, out_name);
        exit(1);
    }
    if(strncmp(buf, field, size)) {
        fprintf(stderr, "%s: %s: Header field '%s' appears to be corrupted.\n",
                program_name, out_name, field_name);
        fprintf(stderr, "Expected value: \"%s\"; encountered value: \"%s\".\n",
                field, buf);
        exit(1);
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
 *  Attempts to read <num_bytes> bytes from <file>, and convert their values to
 *  an unsigned integer assuming they are in ascending order from least to most
 *  significant byte.
**/
size_t read_int_data(FILE *file, uint8_t num_bytes) {
    size_t result = 0;
    for(uint8_t i = 0; i < num_bytes; i++) {
        size_t byte = checked_fgetc(file);
        result += byte << (CHAR_BIT * i);
    }
    return result;
}

/**
 *  Writes <byte> to <file>. If failure is detected, prints an error message and
 *  exits the program.
**/
void checked_fputc(uint8_t byte, FILE *file) {
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

/**
 *  Attempts to read a byte from <file>, and return it. If failure is detected,
 *  prints an error message and exits the program.
**/
uint8_t checked_fgetc(FILE *file) {
    int result = fgetc(file);
    if(result == EOF) {
        fprintf(stderr, "%s: %s: Read failed.\n", program_name, out_name);
    }
    return (uint8_t)result;
}

/**
 *  Attempts to perform a call to fseek with the provided arguments. If failure
 *  is detected, prings an error message and exists the program.
**/
void checked_fseek(FILE *file, long offset, int whence) {
    if(fseek(file, offset, whence)) {
        fprintf(stderr, "%s: %s: Seek failed.\n", program_name, out_name);
    }
}

/**
 *  Close the file stream <out>. Intended for use as an exit
 *  handler.
**/
void close_out(void) {
    fclose(out);
}
