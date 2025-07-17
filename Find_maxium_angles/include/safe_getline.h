#ifndef SAFE_GETLINE_H
#define SAFE_GETLINE_H

#include <stdio.h>

/**
 * 安全的動態行讀取函數
 * 從檔案中讀取一行，自動分配記憶體
 *
 * @param file 要讀取的檔案指標
 * @return 分配的字串指標（需要調用者 free），失敗時返回 NULL
 */
char *safe_getline(FILE *file);

#endif // SAFE_GETLINE_H
