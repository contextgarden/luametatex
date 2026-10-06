/*
    See license.txt in the root of this project. The credits for the basics of this code (the
    inline functions go to wherever gemini got it from. In principle it is published code
    based on some research. The two public helpers are tuned for our purpose, especially
    striping the redundant zeros and precision.
*/

# include <stdint.h>
# include <stdbool.h>
# include <string.h>

typedef struct {
    uint64_t significand;
    int32_t  exponent;
} decimal_rep_t;

static const char DIGIT_TABLE[200] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

static const double POW10[] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19
};

# define pow_10_max 19

static inline decimal_rep_t fast_double_to_decimal(double v, bool *is_neg)
{
    union { double d; uint64_t u; } bits = { v };
    *is_neg = (bits.u >> 63) != 0;
    if (*is_neg) v = -v;
    decimal_rep_t dec = {0, 0};
    if (v == 0.0) {
        return dec;
    }
    // fast integer scaling to extract exact 15-17 significant decimal digits
    int exp10 = 0;
    // scale into normalized double range [1e14, 1e15)
    if (v >= 1e15) {
        while (v >= 1e23) { v /= 1e8;  exp10 += 8; }
        while (v >= 1e15) { v /= 10.0; exp10++; }
    } else if (v <  1e14) {
        while (v <  1e6 ) { v *= 1e8;  exp10 -= 8; }
        while (v <  1e14) { v *= 10.0; exp10--; }
    }
    // fast rounding to nearest integer
    uint64_t sig = (uint64_t) (v + 0.5);
    // strip trailing zeros to keep representation minimal
    while (sig > 0 && (sig % 10 == 0)) {
        sig /= 10;
        exp10++;
    }
    dec.significand = sig;
    dec.exponent = exp10;
    return dec;
}

static inline int render_digits(uint64_t val, char *buf)
{
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    char temp[32];
    int pos = 0;
    while (val >= 100) {
        uint32_t ind = (uint32_t) ((val % 100) * 2);
        val /= 100;
        temp[pos++] = DIGIT_TABLE[ind + 1];
        temp[pos++] = DIGIT_TABLE[ind];
    }
    if (val < 10) {
        temp[pos++] = (char) ('0' + val);
    } else {
        uint32_t ind = (uint32_t) (val * 2);
        temp[pos++] = DIGIT_TABLE[ind + 1];
        temp[pos++] = DIGIT_TABLE[ind];
    }

    for (int i = 0; i < pos; i++) {
        buf[i] = temp[pos - 1 - i];
    }
    buf[pos] = '\0';
    return pos;
}

int double_to_string_f(double val, char *buf, int max_precision)
{
    bool is_neg;
    union { double d; uint64_t u; } bits = { val };
    uint32_t exp = (bits.u >> 52) & 0x7FF;
    if (exp == 0x7FF) {
        if (bits.u & 0x000FFFFFFFFFFFFFULL) {
            buf[0]='N'; buf[1]='a'; buf[2]='N'; buf[3]='\0';
            return 3;
        } else if (bits.u >> 63) {
            buf[0]='-'; buf[1]='I'; buf[2]='n'; buf[3]='f'; buf[4]='\0';
            return 4;
        } else {
            buf[0]='I'; buf[1]='n'; buf[2]='f'; buf[3]='\0';
            return 3;
        }
    }
    decimal_rep_t dec = fast_double_to_decimal(val, &is_neg);
    char digits[32];
    int len = render_digits(dec.significand, digits);
    int dot_pos = len + dec.exponent;
    if (dot_pos < len) {
        int avail_frac = len - (dot_pos > 0 ? dot_pos : 0);
        if (avail_frac > max_precision) {
            int drop = avail_frac - max_precision;
            if (drop > 0 && drop <= pow_10_max) {
                uint64_t p10 = (uint64_t) POW10[drop];
                dec.significand = (dec.significand + (p10 / 2)) / p10;
                dec.exponent += drop;
                len = render_digits(dec.significand, digits);
                dot_pos = len + dec.exponent;
            }
        }
    }
    int p = 0;
    if (is_neg && val != 0.0) buf[p++] = '-';
    if (dot_pos <= 0) {
        buf[p++] = '0';
        buf[p++] = '.';
        for (int i = 0; i < -dot_pos; i++) {
            buf[p++] = '0';
        }
        for (int i = 0; i < len; i++) {
            buf[p++] = digits[i];
        }
    } else if (dot_pos >= len) {
        for (int i = 0; i < len; i++) {
            buf[p++] = digits[i];
        }
        for (int i = 0; i < (dot_pos - len); i++) {
            buf[p++] = '0';
        }
    } else {
        for (int i = 0; i < len; i++) {
            if (i == dot_pos) {
                buf[p++] = '.';
            }
            buf[p++] = digits[i];
        }
    }
    buf[p] = '\0';
    // Trim trailing zeros after the decimal point
    if (memchr(buf, '.', p)) {
        while (p > 0 && buf[p - 1] == '0') {
            p--;
        }
        if (p > 0 && buf[p - 1] == '.') {
            p--;
        }
        buf[p] = '\0';
    }
    return p;
}

int double_to_string_g(double val, char *buf, int max_precision)
{
    bool is_neg;
    union { double d; uint64_t u; } bits = { val };
    uint32_t exp = (bits.u >> 52) & 0x7FF;
    if (exp == 0x7FF) {
        if (bits.u & 0x000FFFFFFFFFFFFFULL) {
            buf[0]='N'; buf[1]='a'; buf[2]='N'; buf[3]='\0';
            return 3;
        }
        if (bits.u >> 63) {
            buf[0]='-'; buf[1]='I'; buf[2]='n'; buf[3]='f'; buf[4]='\0';
            return 4;
        }
        buf[0]='I'; buf[1]='n'; buf[2]='f'; buf[3]='\0';
        return 3;
    }
    decimal_rep_t dec = fast_double_to_decimal(val, &is_neg);
    char digits[32];
    int len = render_digits(dec.significand, digits);
    int exp_val = (len - 1) + dec.exponent;
    // use scientific notation if out of clean printable range
    if (exp_val < -4 || exp_val >= 6) {
        int p = 0;
        if (is_neg && val != 0.0) {
            buf[p++] = '-';
        }
        buf[p++] = digits[0];
        if (len > 1) {
            buf[p++] = '.';
            for (int i = 1; i < len; i++) {
                buf[p++] = digits[i];
            }
        }
        // custom integer exponent renderer (no sprintf)
        buf[p++] = 'e';
        if (exp_val >= 0) {
            buf[p++] = '+';
        } else {
            buf[p++] = '-';
            exp_val = -exp_val;
        }
        if (exp_val < 10) {
            buf[p++] = '0';
            buf[p++] = (char) ('0' + exp_val);
        } else {
            p += render_digits((uint64_t) exp_val, buf + p);
        }
        buf[p] = '\0';
        return p;
    }
    return double_to_string_f(val, buf, max_precision);
}
