#include <stdio.h>
#include <stdlib.h>
#include "safe_getline.h"

// 安全的動態行讀取函數
char *safe_getline(FILE *file) {
    if (!file) return NULL;

    size_t capacity = 128;
    char *line = malloc(capacity);
    if (!line) return NULL;

    size_t len = 0;
    int c;

    while ((c = fgetc(file)) != EOF) {
        if (len >= capacity - 1) {
            capacity *= 2;
            char *new_line = realloc(line, capacity);
            if (!new_line) {
                free(line);
                return NULL;
            }
            line = new_line;
        }

        line[len++] = (char)c;
        if (c == '\n') break;
    }

    if (len == 0 && c == EOF) {
        free(line);
        return NULL;
    }

    line[len] = '\0';
    return line;
}
