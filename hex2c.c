//
// hex2c
//
// Convert between Intel HEX, Binary and C Include format
//
// https://github.com/matveyt/hex2c
//

#include "stdz.h"
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif // _WIN32
#include "ihx.h"

static void c_dump(IHX* ihx, FILE* f);

// user options
static struct {
    char* input;
    char* output;
    int fmt_out;
    unsigned filler;
    unsigned padding;
    unsigned wrap;
} opt = {0};

/*noreturn*/
static void usage(int status)
{
    if (status != 0)
        fprintf(stderr, "Try '%s --help' for more information.\n", z_getprogname());
    else
        printf(
"Usage: %s [OPTION]... FILE\n"
"Convert between Intel HEX, Binary and C Include format.\n"
"\n"
"With no FILE, or when FILE is -, write standard output.\n"
"\n"
"-b, --binary       Binary dump output\n"
"-c, --c            C Include output\n"
"-x, --hex          Intel HEX format output\n"
"-i, --info         Only show file info\n"
"-o, --output=FILE  Set output file name\n"
"-z, --filler=X     Default data byte value\n"
"-p, --padding=NUM  Extra space on line\n"
"-w, --wrap=NUM     Maximum output bytes per line\n"
"-h, --help         Show this message and exit\n",
        z_getprogname());
    exit(status);
}

static void parse_args(int argc, char* argv[])
{
    z_setprogname(argv[0]);

    static struct z_option lopts[] = {
        { "binary", z_no_argument, NULL, 'b' },
        { "c", z_no_argument, NULL, 'c' },
        { "hex", z_no_argument, NULL, 'x' },
        { "info", z_no_argument, NULL, 'i' },
        { "output", z_required_argument, NULL, 'o' },
        { "filler", z_optional_argument, NULL, 'z' },
        { "padding", z_required_argument, NULL, 'p' },
        { "wrap", z_required_argument, NULL, 'w' },
        { "help", z_no_argument, NULL, 'h'},
        {0}
    };

    int c;
    while ((c = z_getopt_long(argc, argv, "bcxio:z::p:w:h", lopts, NULL)) != -1) {
        switch (c) {
        case 'b':
        case 'c':
        case 'x':
        case 'i':
            opt.fmt_out = c;
        break;
        case 'o':
            free(opt.output);
            opt.output = z_strdup(z_optarg);
        break;
        case 'z':
            opt.filler = z_optarg ? strtoul(z_optarg, NULL, 16) : UINT8_MAX;
        break;
        case 'p':
            opt.padding = strtoul(z_optarg, NULL, 10);
            if (opt.padding > UINT8_MAX)
                opt.padding = 0;
        break;
        case 'w':
            opt.wrap = strtoul(z_optarg, NULL, 10);
            if (opt.wrap > UINT8_MAX)
                opt.wrap = 0;
        break;
        case 'h':
            usage(EXIT_SUCCESS);
        break;
        case '?':
            usage(EXIT_FAILURE);
        break;
        }
    }

    if (z_optind == argc - 1)
        opt.input = z_strdup(argv[z_optind]);
    else {
        z_warnx("missing file name");
        usage(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[])
{
    opt.filler = UINT8_MAX + 1; // not used
    parse_args(argc, argv);

    // open files
    FILE* fin = z_fopen(opt.input, "rb");
    FILE* fout = z_fopen(opt.output, "w");

    // read in
    IHX ihx;
    int fmt_in = ihx_load(&ihx, opt.filler, fin);
    if (fmt_in < 0)
        z_error(EXIT_FAILURE, errno, "ihx_load");

    // write out
    switch (opt.fmt_out) {
    case 'b':
#if defined(_WIN32)
        _setmode(_fileno(fout), _O_BINARY);
#endif
        if (opt.filler <= UINT8_MAX)
            for (size_t i = ihx.base; i > 0; --i)
                fputc(opt.filler, fout);
        if (fwrite(ihx.image, 1, ihx.sz, fout) != ihx.sz)
            z_error(EXIT_FAILURE, errno, "fwrite(%zu)", ihx.sz);
    break;
    case 'c':
    case 0:
        c_dump(&ihx, fout);
    break;
    case 'x':
        ihx_dump(&ihx, opt.filler, opt.wrap, fout);
    break;
    case 'i':
        printf("Format: %s\n", (fmt_in == 'x') ? "Intel HEX" : "Binary");
        printf("Size: %zu bytes\n", ihx.sz);
        if (fmt_in == 'x' && ihx.sz > 0) {
            printf("Address Range: %04zX-%04zX\n", ihx.base, ihx.base + ihx.sz - 1);
            printf("Entry Point: %04zX\n", ihx.entry);
        }
    break;
    }

    free(ihx.image);
    fclose(fout);
    fclose(fin);
    free(opt.output);
    free(opt.input);
    exit(EXIT_SUCCESS);
}

// write C Include output file
void c_dump(IHX* ihx, FILE* f)
{
    // user options
    unsigned wrap = opt.wrap ? opt.wrap : 8;
    unsigned padding = opt.padding ? opt.padding : 4;

    // header
    fprintf(f, "// made with %s\n", z_getprogname());
    if (ihx->base > 0)
        fprintf(f, "// image base %#04zx\n", ihx->base);
    if (ihx->entry > 0)
        fprintf(f, "// entry point %#04zx\n", ihx->entry);
    fprintf(f, "const unsigned char image[%zu] = {\n", ihx->sz);

    for (size_t i = 0; i < ihx->sz; i += wrap) {
        // leading space
        fprintf(f, "%*c", padding, ' ');

        // data
        unsigned cb = min(wrap, ihx->sz - i);
        for (unsigned j = 0; j < cb; ++j)
            fprintf(f, "%#02x, ", ihx->image[i + j]);

        // trailing space
        fprintf(f, "%*c// %03zx\n", (wrap - cb) * 6 - 1 + padding, ' ', ihx->base + i);
    }

    // footer
    fputs("};\n", f);
}
