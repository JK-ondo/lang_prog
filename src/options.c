#include "options.h"
#include "diag.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void request_phase(bool *requested, phase_t *deepest, phase_t p) {
    if (!*requested || p > *deepest) {
        *deepest = p;
        *requested = true;
    }
}

options_status_t options_parse(options_t *opt, int argc, char **argv, diag_t *d) {
    bool phase_requested = false;
    phase_t deepest = PHASE_LEX;

    /* init opt */
    opt->input = NULL;
    opt->dump_tokens = false;
    opt->no_warnings = false;
    opt->warnings_are_errors = false;
    opt->max_errors = 0;

    /* scan arguments */
    char *arg = NULL;
    char msg[DIAG_MSG_MAX];

    for (int i = 1; i < argc; i++) {
        arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            /* help option */
            (void)fprintf(stdout,
                "usage: mpplc [options] <file.mpl>\n\nOptions:\n  --dump-tokens       dump the token stream\n  -w                  suppress all warnings\n  -Werror             treat warnings as errors\n  -fmax-errors=<n>    stop after n errors (0 means unlimited)\n  -h, --help          display this help and exit\n  --version           display version information and exit\n\nWith no options, mpplc prints the token count table to standard output.\n");
            return OPTIONS_FINISHED;
        } else if (strcmp(arg, "--version") == 0) {
            /* version option */
            (void)fprintf(stdout, "mpplc 0.1.0\n");
            return OPTIONS_FINISHED;
        } else if (arg[0] == '-') {
            /* option */
            if (strcmp(arg, "--dump-tokens") == 0) {
                opt->dump_tokens = true;
                request_phase(&phase_requested, &deepest, PHASE_LEX);
            } else if (strcmp(arg, "-w") == 0) {
                opt->no_warnings = true;
            } else if (strcmp(arg, "-Werror") == 0) {
                opt->warnings_are_errors = true;
            } else if (strncmp(arg, "-fmax-errors=", sizeof "-fmax-errors=" - 1) == 0) {
                const char * value_str = arg + (sizeof "-fmax-errors=" - 1);
                char *endptr = NULL;
                errno = 0;
                unsigned value = strtoul(value_str, &endptr, 10);
                if (*value_str == '\0' || *endptr != '\0' || *value_str == '-' || value > UINT_MAX || errno == ERANGE) {
                    /* 制限数が空の場合 */
                    /* 制限数に数字以外が含まれる場合 */
                    /* 制限数がマイナスの場合 */
                    /* 制限数が桁溢れ */
                    /* strtoulが−１を受理 */
                    (void)snprintf(msg, sizeof msg, "invalid argument to -fmax-errors: '%s'", value_str);
                    diag_error_noloc(d, msg);
                    return OPTIONS_ERROR;
                } else {
                    opt->max_errors = value;
                }
            } else if (strcmp(arg, "--dump-ast") == 0) {
                request_phase(&phase_requested, &deepest, PHASE_PARSE);
            } else if (strcmp(arg, "--dump-symtab") == 0 || strcmp(arg, "-fsyntax-only") == 0) {
                request_phase(&phase_requested, &deepest, PHASE_SEMANTIC);
            } else if (strcmp(arg, "-S") == 0) {
                request_phase(&phase_requested, &deepest, PHASE_CODEGEN);
            } else {
                /* undefine option */
                (void)snprintf(msg, sizeof msg, "unrecognized command-line option '%s'", arg);
                diag_error_noloc(d, msg);
                return OPTIONS_ERROR;
            }
        } else {
            if (opt->input != NULL) {
                /* 入力ファイル名の個数が２個以上 */
                (void)snprintf(msg, sizeof msg, "multiple input files are not supported (%s, %s)", opt->input, arg);
                diag_error_noloc(d, msg);
                return OPTIONS_ERROR;
            }
            opt->input = arg;
        }
    }

    opt->requested = phase_requested ? deepest : MPPLC_LAST_IMPLEMENTED_PHASE;

    if (opt->input == NULL) {
        /* 入力ファイル名の個数が０個 */
        diag_error_noloc(d, "no input file");
        return OPTIONS_ERROR;
    }

    return OPTIONS_OK;
}
