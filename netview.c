

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>


int main(int argc, char **argv){
    long size = 0;
    unsigned long c;
    unsigned long * f1_hash = NULL;

    struct stat info1;

    int f1 = open(argv[1], O_RDONLY);

    if ( fstat(f1, &info1)){
        printf("stat error\n");
        exit(0);
    }


    f1_hash = mmap(0, info1.st_size, PROT_READ, MAP_PRIVATE, f1, 0);

    size = info1.st_size / sizeof(unsigned long);

    for (c=0 ; c < size ; c++){
        printf("%lu %lu\n", f1_hash[c], c);
    }

    munmap(f1_hash , info1.st_size);
    close(f1);

    return 0;
}


