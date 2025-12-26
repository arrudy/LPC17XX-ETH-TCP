#include "string_extra.h"

#define false 0
#define true 1
#define bool int

char* itoa(int num, char* str) {
    int i = 0;
    bool is_negative = false;

    // Handle 0 explicitly
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // Handle negative numbers
    if (num < 0) {
        is_negative = true;
        num = -num;
    }

    // Extract digits (in reverse order)
    while (num != 0) {
        int digit = num % 10;
        str[i++] = digit + '0';
        num /= 10;
    }

    // Add minus sign if needed
    if (is_negative)
        str[i++] = '-';

    str[i] = '\0';

    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char tmp = str[start];
        str[start] = str[end];
        str[end] = tmp;
        start++;
        end--;
    }

    return str;
}



int strnlen(const char *str, int maxlen) {
    int len = 0;
    while (len < maxlen && *str++) {
        len++;
    }
    return len;
}


