/*
 ****************************************************************************
 *
 *        File: printf.c
 *      Author: Juergen Gross <jgross@suse.com>
 *
 *        Date: Jun 2016
 *
 * Environment: Xen Minimal OS
 * Description: Library functions for printing
 *              (FreeBSD port)
 *
 ****************************************************************************
 */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 * Portions of this software were developed by David Chisnall
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !defined HAVE_LIBC

#include <mini-os/os.h>
#include <mini-os/types.h>
#include <mini-os/hypervisor.h>
#include <mini-os/lib.h>
#include <mini-os/mm.h>
#include <mini-os/ctype.h>
#include <mini-os/posix/limits.h>

#define __DECONST(type, var)    ((type)(uintptr_t)(const void *)(var))
/* 64 bits + 0-Byte at end */
#define MAXNBUF	65

static char const hex2ascii_data[] = "0123456789abcdefghijklmnopqrstuvwxyz";
/*
 * Put a NUL-terminated ASCII number (base <= 36) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */
static char *
ksprintn(char *nbuf, uintmax_t num, int base, int *lenp, int upper)
{
	char *p, c;

	p = nbuf;
	*p = '\0';
	do {
		c = hex2ascii_data[num % base];
		*++p = upper ? toupper(c) : c;
	} while (num /= base);
	if (lenp)
		*lenp = p - nbuf;
	return (p);
}

/*
 * Convert a string to an unsigned long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
unsigned long
strtoul(const char *nptr, char **endptr, int base)
{
        const char *s = nptr;
        unsigned long acc;
        unsigned char c;
        unsigned long cutoff;
        int neg = 0, any, cutlim;

        /*
         * See strtol for comments as to the logic used.
         */
        do {
                c = *s++;
        } while (isspace(c));
        if (c == '-') {
                neg = 1;
                c = *s++;
        } else if (c == '+')
                c = *s++;
        if ((base == 0 || base == 16) &&
            c == '0' && (*s == 'x' || *s == 'X')) {
                c = s[1];
                s += 2;
                base = 16;
        }
        if (base == 0)
                base = c == '0' ? 8 : 10;
        cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
        cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
        for (acc = 0, any = 0;; c = *s++) {
                if (!isascii(c))
                        break;
                if (isdigit(c))
                        c -= '0';
                else if (isalpha(c))
                        c -= isupper(c) ? 'A' - 10 : 'a' - 10;
                else
                        break;
                if (c >= base)
                        break;
                if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
                        any = -1;
                else {
                        any = 1;
                        acc *= base;
                        acc += c;
                }
        }
        if (any < 0) {
                acc = ULONG_MAX;
        } else if (neg)
                acc = -acc;
        if (endptr != 0)
                *endptr = __DECONST(char *, any ? s - 1 : nptr);
        return (acc);
}

/*
 * Convert a string to a quad integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
int64_t
strtoq(const char *nptr, char **endptr, int base)
{
        const char *s;
        uint64_t acc;
        unsigned char c;
        uint64_t qbase, cutoff;
        int neg, any, cutlim;

        /*
         * Skip white space and pick up leading +/- sign if any.
         * If base is 0, allow 0x for hex and 0 for octal, else
         * assume decimal; if base is already 16, allow 0x.
         */
        s = nptr;
        do {
                c = *s++;
        } while (isspace(c));
        if (c == '-') {
                neg = 1;
                c = *s++;
        } else {
                neg = 0;
                if (c == '+')
                        c = *s++;
        }
        if ((base == 0 || base == 16) &&
            c == '0' && (*s == 'x' || *s == 'X')) {
                c = s[1];
                s += 2;
                base = 16;
        }
        if (base == 0)
                base = c == '0' ? 8 : 10;

        /*
         * Compute the cutoff value between legal numbers and illegal
         * numbers.  That is the largest legal value, divided by the
         * base.  An input number that is greater than this value, if
         * followed by a legal input character, is too big.  One that
         * is equal to this value may be valid or not; the limit
         * between valid and invalid numbers is then based on the last
         * digit.  For instance, if the range for quads is
         * [-9223372036854775808..9223372036854775807] and the input base
         * is 10, cutoff will be set to 922337203685477580 and cutlim to
         * either 7 (neg==0) or 8 (neg==1), meaning that if we have
         * accumulated a value > 922337203685477580, or equal but the
         * next digit is > 7 (or 8), the number is too big, and we will
         * return a range error.
         *
         * Set any if any `digits' consumed; make it negative to indicate
         * overflow.
         */
        qbase = (unsigned)base;
        cutoff = neg ? (uint64_t)-(LLONG_MIN + LLONG_MAX) + LLONG_MAX : LLONG_MAX;
        cutlim = cutoff % qbase;
        cutoff /= qbase;
        for (acc = 0, any = 0;; c = *s++) {
                if (!isascii(c))
                        break;
                if (isdigit(c))
                        c -= '0';
                else if (isalpha(c))
                        c -= isupper(c) ? 'A' - 10 : 'a' - 10;
                else
                        break;
                if (c >= base)
                        break;
                if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
                        any = -1;
                else {
                        any = 1;
                        acc *= qbase;
                        acc += c;
                }
        }
        if (any < 0) {
                acc = neg ? LLONG_MIN : LLONG_MAX;
        } else if (neg)
                acc = -acc;
        if (endptr != 0)
                *endptr = __DECONST(char *, any ? s - 1 : nptr);
        return (acc);
}

/*
 * Convert a string to an unsigned quad integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
uint64_t
strtouq(const char *nptr, char **endptr, int base)
{
        const char *s = nptr;
        uint64_t acc;
        unsigned char c;
        uint64_t qbase, cutoff;
        int neg, any, cutlim;

        /*
         * See strtoq for comments as to the logic used.
         */
        do {
                c = *s++;
        } while (isspace(c));
        if (c == '-') {
                neg = 1;
                c = *s++;
        } else {
                neg = 0;
                if (c == '+')
                        c = *s++;
        }
        if ((base == 0 || base == 16) &&
            c == '0' && (*s == 'x' || *s == 'X')) {
                c = s[1];
                s += 2;
                base = 16;
        }
        if (base == 0)
                base = c == '0' ? 8 : 10;
        qbase = (unsigned)base;
        cutoff = (uint64_t)ULLONG_MAX / qbase;
        cutlim = (uint64_t)ULLONG_MAX % qbase;
        for (acc = 0, any = 0;; c = *s++) {
                if (!isascii(c))
                        break;
                if (isdigit(c))
                        c -= '0';
                else if (isalpha(c))
                        c -= isupper(c) ? 'A' - 10 : 'a' - 10;
                else
                        break;
                if (c >= base)
                        break;
                if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
                        any = -1;
                else {
                        any = 1;
                        acc *= qbase;
                        acc += c;
                }
        }
        if (any < 0) {
                acc = ULLONG_MAX;
        } else if (neg)
                acc = -acc;
        if (endptr != 0)
                *endptr = __DECONST(char *, any ? s - 1 : nptr);
        return (acc);
}

/*
 * Scaled down version of printf(3).
 */
int
vsnprintf(char *str, size_t size, char const *fmt, va_list ap)
{
#define PCHAR(c) { if (size >= 2) { *str++ = c; size--; } retval++; }
        char nbuf[MAXNBUF];
        const char *p, *percent;
        int ch, n;
        uintmax_t num;
        int base, lflag, qflag, tmp, width, ladjust, sharpflag, neg, sign, dot;
        int cflag, hflag, jflag, tflag, zflag;
        int dwidth, upper;
        char padc;
        int stop = 0, retval = 0;

        num = 0;

        if (fmt == NULL)
                fmt = "(fmt null)\n";

        for (;;) {
                padc = ' ';
                width = 0;
                while ((ch = (u_char)*fmt++) != '%' || stop) {
                        if (ch == '\0') {
                                if (size >= 1)
                                        *str++ = '\0';
                                return (retval);
                        }
                        PCHAR(ch);
                }
                percent = fmt - 1;
                qflag = 0; lflag = 0; ladjust = 0; sharpflag = 0; neg = 0;
                sign = 0; dot = 0; dwidth = 0; upper = 0;
                cflag = 0; hflag = 0; jflag = 0; tflag = 0; zflag = 0;
reswitch:       switch (ch = (u_char)*fmt++) {
                case '.':
                        dot = 1;
                        goto reswitch;
                case '#':
                        sharpflag = 1;
                        goto reswitch;
                case '+':
                        sign = 1;
                        goto reswitch;
                case '-':
                        ladjust = 1;
                        goto reswitch;
                case '%':
                        PCHAR(ch);
                        break;
                case '*':
                        if (!dot) {
                                width = va_arg(ap, int);
                                if (width < 0) {
                                        ladjust = !ladjust;
                                        width = -width;
                                }
                        } else {
                                dwidth = va_arg(ap, int);
                        }
                        goto reswitch;
                case '0':
                        if (!dot) {
                                padc = '0';
                                goto reswitch;
                        }
                        /* fallthrough */
                case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                                for (n = 0;; ++fmt) {
                                        n = n * 10 + ch - '0';
                                        ch = *fmt;
                                        if (ch < '0' || ch > '9')
                                                break;
                                }
                        if (dot)
                                dwidth = n;
                        else
                                width = n;
                        goto reswitch;
                case 'c':
                        PCHAR(va_arg(ap, int));
                        break;
                case 'd':
                case 'i':
                        base = 10;
                        sign = 1;
                        goto handle_sign;
                case 'h':
                        if (hflag) {
                                hflag = 0;
                                cflag = 1;
                        } else
                                hflag = 1;
                        goto reswitch;
                case 'j':
                        jflag = 1;
                        goto reswitch;
                case 'l':
                        if (lflag) {
                                lflag = 0;
                                qflag = 1;
                        } else
                                lflag = 1;
                        goto reswitch;
                case 'n':
                        if (jflag)
                                *(va_arg(ap, intmax_t *)) = retval;
                        else if (qflag)
                                *(va_arg(ap, int64_t *)) = retval;
                        else if (lflag)
                                *(va_arg(ap, long *)) = retval;
                        else if (zflag)
                                *(va_arg(ap, size_t *)) = retval;
                        else if (hflag)
                                *(va_arg(ap, short *)) = retval;
                        else if (cflag)
                                *(va_arg(ap, char *)) = retval;
                        else
                                *(va_arg(ap, int *)) = retval;
                        break;
                case 'o':
                        base = 8;
                        goto handle_nosign;
                case 'p':
                        base = 16;
                        sharpflag = (width == 0);
                        sign = 0;
                        num = (uintptr_t)va_arg(ap, void *);
                        goto number;
                case 'q':
                        qflag = 1;
                        goto reswitch;
                case 'r':
                        base = 10;
                        if (sign)
                                goto handle_sign;
                        goto handle_nosign;
                case 's':
                        p = va_arg(ap, char *);
                        if (p == NULL)
                                p = "(null)";
                        if (!dot)
                                n = strlen (p);
                        else
                                for (n = 0; n < dwidth && p[n]; n++)
                                        continue;

                        width -= n;

                        if (!ladjust && width > 0)
                                while (width--)
                                        PCHAR(padc);
                        while (n--)
                                PCHAR(*p++);
                        if (ladjust && width > 0)
                                while (width--)
                                        PCHAR(padc);
                        break;
                case 't':
                        tflag = 1;
                        goto reswitch;
                case 'u':
                        base = 10;
                        goto handle_nosign;
                case 'X':
                        upper = 1;
                case 'x':
                        base = 16;
                        goto handle_nosign;
                case 'y':
                        base = 16;
                        sign = 1;
                        goto handle_sign;
                case 'z':
                        zflag = 1;
                        goto reswitch;
handle_nosign:
                        sign = 0;
                        if (jflag)
                                num = va_arg(ap, uintmax_t);
                        else if (qflag)
                                num = va_arg(ap, uint64_t);
                        else if (tflag)
                                num = va_arg(ap, ptrdiff_t);
                        else if (lflag)
                                num = va_arg(ap, u_long);
                        else if (zflag)
                                num = va_arg(ap, size_t);
                        else if (hflag)
                                num = (unsigned short)va_arg(ap, int);
                        else if (cflag)
                                num = (u_char)va_arg(ap, int);
                        else
                                num = va_arg(ap, u_int);
                        goto number;
handle_sign:
                        if (jflag)
                                num = va_arg(ap, intmax_t);
                        else if (qflag)
                                num = va_arg(ap, int64_t);
                        else if (tflag)
                                num = va_arg(ap, ptrdiff_t);
                        else if (lflag)
                                num = va_arg(ap, long);
                        else if (zflag)
                                num = va_arg(ap, ssize_t);
                        else if (hflag)
                                num = (short)va_arg(ap, int);
                        else if (cflag)
                                num = (char)va_arg(ap, int);
                        else
                                num = va_arg(ap, int);
number:
                        if (sign && (intmax_t)num < 0) {
                                neg = 1;
                                num = -(intmax_t)num;
                        }
                        p = ksprintn(nbuf, num, base, &n, upper);
                        tmp = 0;
                        if (sharpflag && num != 0) {
                                if (base == 8)
                                        tmp++;
                                else if (base == 16)
                                        tmp += 2;
                        }
                        if (neg)
                                tmp++;

                        if (!ladjust && padc == '0')
                                dwidth = width - tmp;
                        width -= tmp + (dwidth > n ? dwidth : n);
                        dwidth -= n;
                        if (!ladjust)
                                while (width-- > 0)
                                        PCHAR(' ');
                        if (neg)
                                PCHAR('-');
                        if (sharpflag && num != 0) {
                                if (base == 8) {
                                        PCHAR('0');
                                } else if (base == 16) {
                                        PCHAR('0');
                                        PCHAR('x');
                                }
                        }
                        while (dwidth-- > 0)
                                PCHAR('0');

                        while (*p)
                                PCHAR(*p--);

                        if (ladjust)
                                while (width-- > 0)
                                        PCHAR(' ');

                        break;
                default:
                        while (percent < fmt)
                                PCHAR(*percent++);
                        /*
                         * Since we ignore a formatting argument it is no
                         * longer safe to obey the remaining formatting
                         * arguments as the arguments will no longer match
                         * the format specs.
                         */
                        stop = 1;
                        break;
                }
        }
#undef PCHAR
}

/**
 * snprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @size: The size of the buffer, including the trailing null space
 * @fmt: The format string to use
 * @...: Arguments for the format string
 */
int snprintf(char * buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i=vsnprintf(buf,size,fmt,args);
    va_end(args);
    return i;
}

/**
 * vsprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @args: Arguments for the format string
 *
 * Call this function if you are already dealing with a va_list.
 * You probably want sprintf instead.
 */
int vsprintf(char *buf, const char *fmt, va_list args)
{
    return vsnprintf(buf, 0xFFFFFFFFUL, fmt, args);
}


/**
 * sprintf - Format a string and place it in a buffer
 * @buf: The buffer to place the result into
 * @fmt: The format string to use
 * @...: Arguments for the format string
 */
int sprintf(char * buf, const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i=vsprintf(buf,fmt,args);
    va_end(args);
    return i;
}

/*
 * Fill in the given table from the scanset at the given format
 * (just after `[').  Return a pointer to the character past the
 * closing `]'.  The table has a 1 wherever characters should be
 * considered part of the scanset.
 */
static const u_char *
__sccl(char *tab, const u_char *fmt)
{
        int c, n, v;

        /* first `clear' the whole table */
        c = *fmt++;             /* first char hat => negated scanset */
        if (c == '^') {
                v = 1;          /* default => accept */
                c = *fmt++;     /* get new first char */
        } else
                v = 0;          /* default => reject */

        /* XXX: Will not work if sizeof(tab*) > sizeof(char) */
        for (n = 0; n < 256; n++)
                     tab[n] = v;        /* memset(tab, v, 256) */

        if (c == 0)
                return (fmt - 1);/* format ended before closing ] */

        /*
         * Now set the entries corresponding to the actual scanset
         * to the opposite of the above.
         *
         * The first character may be ']' (or '-') without being special;
         * the last character may be '-'.
         */
        v = 1 - v;
        for (;;) {
                tab[c] = v;             /* take character c */
doswitch:
                n = *fmt++;             /* and examine the next */
                switch (n) {

                case 0:                 /* format ended too soon */
                        return (fmt - 1);

                case '-':
                        /*
                         * A scanset of the form
                         *      [01+-]
                         * is defined as `the digit 0, the digit 1,
                         * the character +, the character -', but
                         * the effect of a scanset such as
                         *      [a-zA-Z0-9]
                         * is implementation defined.  The V7 Unix
                         * scanf treats `a-z' as `the letters a through
                         * z', but treats `a-a' as `the letter a, the
                         * character -, and the letter a'.
                         *
                         * For compatibility, the `-' is not considerd
                         * to define a range if the character following
                         * it is either a close bracket (required by ANSI)
                         * or is not numerically greater than the character
                         * we just stored in the table (c).
                         */
                        n = *fmt;
                        if (n == ']' || n < c) {
                                c = '-';
                                break;  /* resume the for(;;) */
                        }
                        fmt++;
                        /* fill in the range */
                        do {
                            tab[++c] = v;
                        } while (c < n);
                        c = n;
                        /*
                         * Alas, the V7 Unix scanf also treats formats
                         * such as [a-c-e] as `the letters a through e'.
                         * This too is permitted by the standard....
                         */
                        goto doswitch;
                        break;

                case ']':               /* end of scanset */
                        return (fmt);

                default:                /* just another character */
                        c = n;
                        break;
                }
        }
        /* NOTREACHED */
}

/**
 * vsscanf - Unformat a buffer into a list of arguments
 * @buf:	input buffer
 * @fmt:	format of buffer
 * @args:	arguments
 */
#define BUF             32      /* Maximum length of numeric string. */

/*
 * Flags used during conversion.
 */
#define LONG            0x01    /* l: long or double */
#define SHORT           0x04    /* h: short */
#define SUPPRESS        0x08    /* suppress assignment */
#define POINTER         0x10    /* weird %p pointer (`fake hex') */
#define NOSKIP          0x20    /* do not skip blanks */
#define QUAD            0x400
#define SHORTSHORT      0x4000  /** hh: char */

/*
 * The following are used in numeric conversions only:
 * SIGNOK, NDIGITS, DPTOK, and EXPOK are for floating point;
 * SIGNOK, NDIGITS, PFXOK, and NZDIGITS are for integral.
 */
#define SIGNOK          0x40    /* +/- is (still) legal */
#define NDIGITS         0x80    /* no digits detected */

#define DPTOK           0x100   /* (float) decimal point is still legal */
#define EXPOK           0x200   /* (float) exponent (e+3, etc) still legal */

#define PFXOK           0x100   /* 0x prefix is (still) legal */
#define NZDIGITS        0x200   /* no zero digits detected */

/*
 * Conversion types.
 */
#define CT_CHAR         0       /* %c conversion */
#define CT_CCL          1       /* %[...] conversion */
#define CT_STRING       2       /* %s conversion */
#define CT_INT          3       /* integer, i.e., strtoq or strtouq */
typedef uint64_t (*ccfntype)(const char *, char **, int);

int
vsscanf(const char *inp, char const *fmt0, va_list ap)
{
        int inr;
        const u_char *fmt = (const u_char *)fmt0;
        int c;                  /* character from format, or conversion */
        size_t width;           /* field width, or 0 */
        char *p;                /* points into all kinds of strings */
        int n;                  /* handy integer */
        int flags;              /* flags as defined above */
        char *p0;               /* saves original value of p when necessary */
        int nassigned;          /* number of fields assigned */
        int nconversions;       /* number of conversions */
        int nread;              /* number of characters consumed from fp */
        int base;               /* base argument to strtoq/strtouq */
        ccfntype ccfn;          /* conversion function (strtoq/strtouq) */
        char ccltab[256];       /* character class table for %[...] */
        char buf[BUF];          /* buffer for numeric conversions */

        /* `basefix' is used to avoid `if' tests in the integer scanner */
        static short basefix[17] =
                { 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

        inr = strlen(inp);

        nassigned = 0;
        nconversions = 0;
        nread = 0;
        base = 0;               /* XXX just to keep gcc happy */
        ccfn = NULL;            /* XXX just to keep gcc happy */
        for (;;) {
                c = *fmt++;
                if (c == 0)
                        return (nassigned);
                if (isspace(c)) {
                        while (inr > 0 && isspace(*inp))
                                nread++, inr--, inp++;
                        continue;
                }
                if (c != '%')
                        goto literal;
                width = 0;
                flags = 0;
                /*
                 * switch on the format.  continue if done;
                 * break once format type is derived.
                 */
again:          c = *fmt++;
                switch (c) {
                case '%':
literal:
                        if (inr <= 0)
                                goto input_failure;
                        if (*inp != c)
                                goto match_failure;
                        inr--, inp++;
                        nread++;
                        continue;

                case '*':
                        flags |= SUPPRESS;
                        goto again;
                case 'l':
                        if (flags & LONG){
                                flags &= ~LONG;
                                flags |= QUAD;
                        } else {
                                flags |= LONG;
                        }
                        goto again;
                case 'q':
                        flags |= QUAD;
                        goto again;
                case 'h':
                        if (flags & SHORT){
                                flags &= ~SHORT;
                                flags |= SHORTSHORT;
                        } else {
                                flags |= SHORT;
                        }
                        goto again;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                        width = width * 10 + c - '0';
                        goto again;

                /*
                 * Conversions.
                 *
                 */
                case 'd':
                        c = CT_INT;
                        ccfn = (ccfntype)strtoq;
                        base = 10;
                        break;

                case 'i':
                        c = CT_INT;
                        ccfn = (ccfntype)strtoq;
                        base = 0;
                        break;

                case 'o':
                        c = CT_INT;
                        ccfn = strtouq;
                        base = 8;
                        break;

                case 'u':
                        c = CT_INT;
                        ccfn = strtouq;
                        base = 10;
                        break;

                case 'x':
                        flags |= PFXOK; /* enable 0x prefixing */
                        c = CT_INT;
                        ccfn = strtouq;
                        base = 16;
                        break;

                case 's':
                        c = CT_STRING;
                        break;

                case '[':
                        fmt = __sccl(ccltab, fmt);
                        flags |= NOSKIP;
                        c = CT_CCL;
                        break;

                case 'c':
                        flags |= NOSKIP;
                        c = CT_CHAR;
                        break;

                case 'p':       /* pointer format is like hex */
                        flags |= POINTER | PFXOK;
                        c = CT_INT;
                        ccfn = strtouq;
                        base = 16;
                        break;

                case 'n':
                        nconversions++;
                        if (flags & SUPPRESS)   /* ??? */
                                continue;
                        if (flags & SHORTSHORT)
                                *va_arg(ap, char *) = nread;
                        else if (flags & SHORT)
                                *va_arg(ap, short *) = nread;
                        else if (flags & LONG)
                                *va_arg(ap, long *) = nread;
                        else if (flags & QUAD)
                                *va_arg(ap, int64_t *) = nread;
                        else
                                *va_arg(ap, int *) = nread;
                        continue;
                }

                /*
                 * We have a conversion that requires input.
                 */
                if (inr <= 0)
                        goto input_failure;

                /*
                 * Consume leading white space, except for formats
                 * that suppress this.
                 */
                if ((flags & NOSKIP) == 0) {
                        while (isspace(*inp)) {
                                nread++;
                                if (--inr > 0)
                                        inp++;
                                else
                                        goto input_failure;
                        }
                        /*
                         * Note that there is at least one character in
                         * the buffer, so conversions that do not set NOSKIP
                         * can no longer result in an input failure.
                         */
                }

                /*
                 * Do the conversion.
                 */
                switch (c) {

                case CT_CHAR:
                        /* scan arbitrary characters (sets NOSKIP) */
                        if (width == 0)
                                width = 1;
                        if (flags & SUPPRESS) {
                                size_t sum = 0;
                                if ((n = inr) < width) {
                                        sum += n;
                                        width -= n;
                                        inp += n;
                                        if (sum == 0)
                                                goto input_failure;
                                } else {
                                        sum += width;
                                        inr -= width;
                                        inp += width;
                                }
                                nread += sum;
                        } else {
                                memcpy(va_arg(ap, char *), inp, width);
                                inr -= width;
                                inp += width;
                                nread += width;
                                nassigned++;
                        }
                        nconversions++;
                        break;

                case CT_CCL:
                        /* scan a (nonempty) character class (sets NOSKIP) */
                        if (width == 0)
                                width = (size_t)~0;     /* `infinity' */
                        /* take only those things in the class */
                        if (flags & SUPPRESS) {
                                n = 0;
                                while (ccltab[(unsigned char)*inp]) {
                                        n++, inr--, inp++;
                                        if (--width == 0)
                                                break;
                                        if (inr <= 0) {
                                                if (n == 0)
                                                        goto input_failure;
                                                break;
                                        }
                                }
                                if (n == 0)
                                        goto match_failure;
                        } else {
                                p0 = p = va_arg(ap, char *);
                                while (ccltab[(unsigned char)*inp]) {
                                        inr--;
                                        *p++ = *inp++;
                                        if (--width == 0)
                                                break;
                                        if (inr <= 0) {
                                                if (p == p0)
                                                        goto input_failure;
                                                break;
                                        }
                                }
                                n = p - p0;
                                if (n == 0)
                                        goto match_failure;
                                *p = 0;
                                nassigned++;
                        }
                        nread += n;
                        nconversions++;
                        break;

                case CT_STRING:
                        /* like CCL, but zero-length string OK, & no NOSKIP */
                        if (width == 0)
                                width = (size_t)~0;
                        if (flags & SUPPRESS) {
                                n = 0;
                                while (!isspace(*inp)) {
                                        n++, inr--, inp++;
                                        if (--width == 0)
                                                break;
                                        if (inr <= 0)
                                                break;
                                }
                                nread += n;
                        } else {
                                p0 = p = va_arg(ap, char *);
                                while (!isspace(*inp)) {
                                        inr--;
                                        *p++ = *inp++;
                                        if (--width == 0)
                                                break;
                                        if (inr <= 0)
                                                break;
                                }
                                *p = 0;
                                nread += p - p0;
                                nassigned++;
                        }
                        nconversions++;
                        continue;

                case CT_INT:
                        /* scan an integer as if by strtoq/strtouq */
#ifdef hardway
                        if (width == 0 || width > sizeof(buf) - 1)
                                width = sizeof(buf) - 1;
#else
                        /* size_t is unsigned, hence this optimisation */
                        if (--width > sizeof(buf) - 2)
                                width = sizeof(buf) - 2;
                        width++;
#endif
                        flags |= SIGNOK | NDIGITS | NZDIGITS;
                        for (p = buf; width; width--) {
                                c = *inp;
                                /*
                                 * Switch on the character; `goto ok'
                                 * if we accept it as a part of number.
                                 */
                                switch (c) {

                                /*
                                 * The digit 0 is always legal, but is
                                 * special.  For %i conversions, if no
                                 * digits (zero or nonzero) have been
                                 * scanned (only signs), we will have
                                 * base==0.  In that case, we should set
                                 * it to 8 and enable 0x prefixing.
                                 * Also, if we have not scanned zero digits
                                 * before this, do not turn off prefixing
                                 * (someone else will turn it off if we
                                 * have scanned any nonzero digits).
                                 */
                                case '0':
                                        if (base == 0) {
                                                base = 8;
                                                flags |= PFXOK;
                                        }
                                        if (flags & NZDIGITS)
                                            flags &= ~(SIGNOK|NZDIGITS|NDIGITS);
                                        else
                                            flags &= ~(SIGNOK|PFXOK|NDIGITS);
                                        goto ok;

                                /* 1 through 7 always legal */
                                case '1': case '2': case '3':
                                case '4': case '5': case '6': case '7':
                                        base = basefix[base];
                                        flags &= ~(SIGNOK | PFXOK | NDIGITS);
                                        goto ok;

                                /* digits 8 and 9 ok iff decimal or hex */
                                case '8': case '9':
                                        base = basefix[base];
                                        if (base <= 8)
                                                break;  /* not legal here */
                                        flags &= ~(SIGNOK | PFXOK | NDIGITS);
                                        goto ok;

                                /* letters ok iff hex */
                                case 'A': case 'B': case 'C':
                                case 'D': case 'E': case 'F':
                                case 'a': case 'b': case 'c':
                                case 'd': case 'e': case 'f':
                                        /* no need to fix base here */
                                        if (base <= 10)
                                                break;  /* not legal here */
                                        flags &= ~(SIGNOK | PFXOK | NDIGITS);
                                        goto ok;

                                /* sign ok only as first character */
                                case '+': case '-':
                                        if (flags & SIGNOK) {
                                                flags &= ~SIGNOK;
                                                goto ok;
                                        }
                                        break;

                                /* x ok iff flag still set & 2nd char */
                                case 'x': case 'X':
                                        if (flags & PFXOK && p == buf + 1) {
                                                base = 16;      /* if %i */
                                                flags &= ~PFXOK;
                                                goto ok;
                                        }
                                        break;
                                }

                                /*
                                 * If we got here, c is not a legal character
                                 * for a number.  Stop accumulating digits.
                                 */
                                break;
                ok:
                                /*
                                 * c is legal: store it and look at the next.
                                 */
                                *p++ = c;
                                if (--inr > 0)
                                        inp++;
                                else 
                                        break;          /* end of input */
                        }
                        /*
                         * If we had only a sign, it is no good; push
                         * back the sign.  If the number ends in `x',
                         * it was [sign] '' 'x', so push back the x
                         * and treat it as [sign] ''.
                         */
                        if (flags & NDIGITS) {
                                if (p > buf) {
                                        inp--;
                                        inr++;
                                }
                                goto match_failure;
                        }
                        c = ((u_char *)p)[-1];
                        if (c == 'x' || c == 'X') {
                                --p;
                                inp--;
                                inr++;
                        }
                        if ((flags & SUPPRESS) == 0) {
                                uint64_t res;

                                *p = 0;
                                res = (*ccfn)(buf, (char **)NULL, base);
                                if (flags & POINTER)
                                        *va_arg(ap, void **) =
                                                (void *)(uintptr_t)res;
                                else if (flags & SHORTSHORT)
                                        *va_arg(ap, char *) = res;
                                else if (flags & SHORT)
                                        *va_arg(ap, short *) = res;
                                else if (flags & LONG)
                                        *va_arg(ap, long *) = res;
                                else if (flags & QUAD)
                                        *va_arg(ap, int64_t *) = res;
                                else
                                        *va_arg(ap, int *) = res;
                                nassigned++;
                        }
                        nread += p - buf;
                        nconversions++;
                        break;

                }
        }
input_failure:
        return (nconversions != 0 ? nassigned : -1);
match_failure:
        return (nassigned);
}

/**
 * sscanf - Unformat a buffer into a list of arguments
 * @buf:	input buffer
 * @fmt:	formatting of buffer
 * @...:	resulting arguments
 */
int sscanf(const char * buf, const char * fmt, ...)
{
	va_list args;
	int i;

	va_start(args,fmt);
	i = vsscanf(buf,fmt,args);
	va_end(args);
	return i;
}

#endif
