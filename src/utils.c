#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>

static uint32_t g_seq_counter = 100;

void utils_generate_txn_reference(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 32) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    g_seq_counter++;
    if (g_seq_counter > 999) g_seq_counter = 100;

    char dt[24];
    if (t) {
        strftime(dt, sizeof(dt), "%Y%m%d%H%M%S", t);
    } else {
        snprintf(dt, sizeof(dt), "20261007000000");
    }

    snprintf(buffer, buffer_size, "TXN%s%03u", dt, g_seq_counter);
}

bool utils_parse_amount_to_paise(const char *amount_str, int64_t *out_paise)
{
    if (!amount_str || !out_paise) return false;

    /* Trim leading spaces */
    while (*amount_str == ' ' || *amount_str == '\t') amount_str++;
    if (*amount_str == '\0' || *amount_str == '-') return false;

    int64_t rupees = 0;
    int64_t paise = 0;
    const char *p = amount_str;

    while (*p && *p != '.') {
        if (!isdigit((unsigned char)*p)) return false;
        rupees = rupees * 10 + (*p - '0');
        if (rupees > 1000000000LL) return false; /* Reasonable limit */
        p++;
    }

    if (*p == '.') {
        p++;
        if (!isdigit((unsigned char)*p)) return false;
        paise = (*p - '0') * 10;
        p++;
        if (*p) {
            if (!isdigit((unsigned char)*p)) return false;
            paise += (*p - '0');
            p++;
            if (*p) return false; /* More than 2 decimal places */
        }
    }

    int64_t total = (rupees * 100LL) + paise;
    if (total <= 0) return false;

    *out_paise = total;
    return true;
}

void utils_paise_to_decimal_str(int64_t paise, char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 24) return;
    int64_t r = paise / 100LL;
    int64_t p = paise % 100LL;
    if (p < 0) p = -p;
    snprintf(buffer, buffer_size, "%lld.%02lld", (long long)r, (long long)p);
}
