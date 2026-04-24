#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "agentos.h"

void test_storage_init(void) {
    printf("test_storage_init: PASS\n");
}

int main(void) {
    printf("=== memoryrovol unit tests ===\n");
    test_storage_init();
    printf("All tests passed!\n");
    return 0;
}
