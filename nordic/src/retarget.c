

#define __FILE_INCOMPLETE 1
#include <stdio.h>

#include "uart_handle.h"

//warning - it blocks! incredibly costly

int stdout_putchar(int ch, FILE *f)
{

  uart2_putc_sys(ch);
  return ch;
}


int stderr_putchar(int ch, FILE *f)
{

  stdout_putchar(ch, f);
  return ch;
}

/*

//#include <rt_sys.h> // Include this for standard definitions
//#include <rt_misc.h>

// 1. Disable Semihosting 
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
    __asm(".global __use_no_semihosting\n\t");
#else
    #pragma import(__use_no_semihosting)
#endif



struct __FILE { int handle; };
FILE __stdout;
FILE __stdin;

int fputc(int ch, FILE *f)
{

  uart2_putc_sys(ch);
  return ch;
}

int __write(int handle, const unsigned char *buf, unsigned len)
{
    (void)handle;

    for (unsigned i = 0; i < len; i++)
        uart2_putc_sys(buf[i]);

    return len;
}

int ferror(FILE *f)
{

  return EOF;
}

void _ttywrch(int ch)
{
  uart2_putc_sys(ch);
}



void _sys_exit(int return_code) {
    while (1); // Infinite loop on exit
}

void test(void)
{
  printf("Hello world\n");
}*/
