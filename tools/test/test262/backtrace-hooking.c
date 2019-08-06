#include <stdio.h>

int backtrace(void **buffer, int size)
{
    int i;
    for (i = 0; i < size; i ++) {
        buffer[i] = 0;
    }
    return size;
}
