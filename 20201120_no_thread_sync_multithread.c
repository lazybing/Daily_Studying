#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define LEN 100000
int num = 0;

char *thread_func(void *arg)
{
    for (int i = 0; i < LEN; i++)
        num += 1;

    return NULL;
}

int main(int argc, char **argv)
{
    pthread_t tid1, tid2;
    if (pthread_create(&tid1, NULL, (void *)thread_func, NULL) != 0) {
        printf("pthread_create error.\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&tid2, NULL, (void *)thread_func, NULL) != 0) {
        printf("pthread_create error.\n");
        exit(EXIT_FAILURE);
    }

    char *rev = NULL;
    pthread_join(tid1, (void *)&rev);
    pthread_join(tid2, (void *)&rev);

    printf("correct result = %d. wrong result = %d.\n", 2 * LEN, num);
    return 0;
}
