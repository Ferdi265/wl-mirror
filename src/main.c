#include <stdio.h>
#include <stdlib.h>
#include "context.h"

int main(int argc, char ** argv) {
    if (argc != 2) {
        printf("usage: wl-mirror <output>\n");
        exit(1);
    }

    char * output = argv[1];
}
