#include <tinylib.h>

static unsigned long long seed = 12345;

void srand(unsigned int _seed)
{
    seed = _seed;
}

int rand(void)
{
    return seed = seed * 48271 % 2147483647;
}

int tisdigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

int atoi(const char *str)
{
    int num = 0;
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    for (; isdigit(*str); str++)
        num = num * 10 + *str - '0';
    return sign * num;
}

void _assert(char const* expr, int value)
{
    if (!value) {
        printf("assertion failed: %s\n", expr);
        exit(1);
    }
}
