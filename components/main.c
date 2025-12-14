#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("Hello from main!\n");
    printf("Build successful!\n");
    
    // Example command line arguments
    if (argc > 1) {
        printf("Arguments received:\n");
        for (int i = 1; i < argc; i++) {
            printf("  %d: %s\n", i, argv[i]);
        }
    } else {
        printf("No arguments provided.\n");
    }
    
    return 0;
}
