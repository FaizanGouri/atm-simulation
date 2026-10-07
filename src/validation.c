#include "validation.h"
#include <string.h>
#include <ctype.h>

bool validation_is_valid_card_number(const char *card_number)
{
    if (!card_number) return false;
    size_t len = strlen(card_number);
    if (len < 12 || len > 19) return false;

    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)card_number[i])) {
            return false;
        }
    }
    return true;
}

bool validation_is_valid_pin(const char *pin)
{
    if (!pin) return false;
    size_t len = strlen(pin);
    if (len < 4 || len > 6) return false;

    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)pin[i])) {
            return false;
        }
    }
    return true;
}

void validation_trim(char *str)
{
    if (!str) return;
    char *start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\r' || *(end - 1) == '\n')) {
        end--;
    }
    *end = '\0';
    if (start != str) {
        memmove(str, start, (size_t)(end - start + 1));
    }
}
