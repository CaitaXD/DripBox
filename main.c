#include <stdio.h>
#include <common.h>
#include <dynamic_array.h>

int main(void)
{
    dynamic_array(int) array = dynamic_array_new(int, &default_allocator);
    dynamic_array_push(array, 42);
    dynamic_array_push(array, 1337);
    dynamic_array_push(array, 12);
    dynamic_array_push(array, 3);
    dynamic_array_push(array, 6);

    while (dynamic_array_length(array) > 0) {
        printf("%d\n", dynamic_array_pop(array));
    }

    return 0;
}
