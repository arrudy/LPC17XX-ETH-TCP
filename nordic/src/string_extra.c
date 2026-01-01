#include "string_extra.h"

#define false 0
#define true 1
#define bool int
  




char* itoa_dec(int num, char* str) {
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


char* itoa_hex(int num, char* str) {
    const char hex_chars[] = "0123456789ABCDEF";
    unsigned int n = (unsigned int)num; // treat as unsigned
    char buf[9]; // enough for 8 hex digits + null
    int i = 0;

    if (str == 0) return 0;

    // Handle zero explicitly
    if (n == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    // Convert to hex in reverse order
    while (n != 0 && i < 8) {
        buf[i++] = hex_chars[n & 0xF];
        n >>= 4;
    }

    // Reverse the string into output buffer
    int j;
    for (j = 0; j < i; j++) {
        str[j] = buf[i - j - 1];
    }
    str[i] = '\0';
    return str;
}


char * itoa(int num, char* str, int base)
{
switch(base)
  {
  case 10:
    return itoa_dec(num, str);
  case 16:
    return itoa_hex(num, str);
  default:
    return 0;
  }
}



