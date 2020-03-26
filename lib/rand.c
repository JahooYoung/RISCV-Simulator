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
