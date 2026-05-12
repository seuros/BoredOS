// Copyright (c) 2023-2026 Chris (boreddevnl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.
// BOREDOS_APP_DESC: Search for text inside a file.

#include "../libc/syscall.h"
#include "../libc/stdlib.h"
#include "../libc/string.h"
#include "../libc/stdio.h"

#define READ_BUF_SIZE 4096
#define LINE_BUF_SIZE 1024

static int sc_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void print_usage(void) {
    printf("Usage: grep [options] <text> <file>\n");
    printf("\n");
    printf("Search for text inside a file.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -n    Show line numbers\n");
    printf("  -i    Case-insensitive search\n");
    printf("  -c    Print match count only\n");
    printf("  -h    Show this help\n");
}

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static int str_contains(const char *haystack, const char *needle, int ignore_case) {
    int h_len = (int)strlen(haystack);
    int n_len = (int)strlen(needle);

    if (n_len == 0) return 1;
    if (n_len > h_len) return 0;

    for (int i = 0; i <= h_len - n_len; i++) {
        int match = 1;
        for (int j = 0; j < n_len; j++) {
            char h = ignore_case ? to_lower(haystack[i + j]) : haystack[i + j];
            char n = ignore_case ? to_lower(needle[j]) : needle[j];
            if (h != n) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    int show_numbers = 0;
    int ignore_case  = 0;
    int count_only   = 0;
    int arg_offset   = 1;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') break;
        if (sc_strcmp(argv[i], "-n") == 0) {
            show_numbers = 1; arg_offset++;
        } else if (sc_strcmp(argv[i], "-i") == 0) {
            ignore_case = 1; arg_offset++;
        } else if (sc_strcmp(argv[i], "-c") == 0) {
            count_only = 1; arg_offset++;
        } else if (sc_strcmp(argv[i], "-h") == 0 ||
                   sc_strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else {
            printf("grep: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (argc - arg_offset < 2) {
        print_usage();
        return 1;
    }

    const char *pattern = argv[arg_offset];
    const char *path    = argv[arg_offset + 1];

    int fd = sys_open(path, "r");
    if (fd < 0) {
        printf("grep: cannot open '%s'\n", path);
        return 1;
    }

    static char read_buf[READ_BUF_SIZE];
    static char line[LINE_BUF_SIZE];
    int line_pos  = 0;
    int line_num  = 0;
    int match_cnt = 0;

    while (1) {
        int bytes = sys_read(fd, read_buf, READ_BUF_SIZE);
        if (bytes <= 0) break;

        for (int i = 0; i < bytes; i++) {
            char c = read_buf[i];

            if (c == '\n' || line_pos >= LINE_BUF_SIZE - 1) {
                line[line_pos] = '\0';
                line_num++;

                if (str_contains(line, pattern, ignore_case)) {
                    match_cnt++;
                    if (!count_only) {
                        if (show_numbers)
                            printf("%d: %s\n", line_num, line);
                        else
                            printf("%s\n", line);
                    }
                }

                line_pos = 0;
            } else if (c != '\r') {
                line[line_pos++] = c;
            }
        }
    }

    // Handle last line if file doesn't end with newline
    if (line_pos > 0) {
        line[line_pos] = '\0';
        line_num++;
        if (str_contains(line, pattern, ignore_case)) {
            match_cnt++;
            if (!count_only) {
                if (show_numbers)
                    printf("%d: %s\n", line_num, line);
                else
                    printf("%s\n", line);
            }
        }
    }

    sys_close(fd);

    if (count_only)
        printf("%d\n", match_cnt);

    return match_cnt > 0 ? 0 : 1;
}
