#include <stdio.h>
#include <common.h>
#include <dynamic_array.h>

int main(void) {
    printf("Fifo\n");
    dynamic_array(int) fifo_ex = dynamic_array_new(int, &mallocator);
    dynamic_array_insert(&fifo_ex, 0, 1);
    dynamic_array_insert(&fifo_ex, 0, 2);
    dynamic_array_insert(&fifo_ex, 0, 3);
    dynamic_array_insert(&fifo_ex, 0, 4);
    dynamic_array_insert(&fifo_ex, 0, 5);

    while (dynamic_array_length(fifo_ex) > 0) {
        printf("%d\n", dynamic_array_pop(&fifo_ex));
    }

    printf("Lifo\n");

    dynamic_array(int) lifo_ex = dynamic_array_new(int, &mallocator);
    dynamic_array_push(&lifo_ex, 1);
    dynamic_array_push(&lifo_ex, 2);
    dynamic_array_push(&lifo_ex, 3);
    dynamic_array_push(&lifo_ex, 4);
    dynamic_array_push(&lifo_ex, 5);

    while (dynamic_array_length(lifo_ex) > 0) {
        printf("%d\n", dynamic_array_pop(&lifo_ex));
    }

    return 0;
}
