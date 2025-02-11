#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
    #define PATH_SEPARATOR "\\"
#else
    #define PATH_SEPARATOR "/"
#endif

// 結構體，用於保存檔案名和對應的LINE數字
struct FileInfo {
    char filename[256];
    int line_number;
};

// 比較函數，用於qsort
int compare_files(const void *a, const void *b) {
    return ((struct FileInfo *)a)->line_number - ((struct FileInfo *)b)->line_number;
}

// 從檔案的第二行解析出LINE數字
int getLineFromFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("無法打開檔案");
        return -1;
    }

    char line[1024];
    // 跳過第一行
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return -1;
    }

    // 讀取第二行並解析LINE數字
    if (fgets(line, sizeof(line), file) != NULL) {
        char *token = strtok(line, " ");
        // 假設LINE是第15個欄位，跳過前14個欄位
        for (int i = 0; i < 14; i++) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                fclose(file);
                return -1;
            }
        }
        fclose(file);
        return atoi(token); // 將字符串轉換為整數
    }

    fclose(file);
    return -1;
}

int main(int argc, char *argv[]) {
    DIR *dir;
    struct dirent *ent;
    struct FileInfo *files = NULL;
    struct stat st;
    int file_count = 0;
    const char *directory_path;
    FILE *merged_file;

    // 檢查是否有提供路徑參數
    if (argc > 1) {
        directory_path = argv[1];
    } else {
        directory_path = "."; // 如果沒有參數，則使用當前目錄
    }

    // 打開資料夾
    if ((dir = opendir(directory_path)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            // 構造文件路徑，注意路徑分隔符
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s%s%s", directory_path, PATH_SEPARATOR, ent->d_name);

            // 確認是否為常規檔案
            if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
                // 動態分配記憶體來存儲新檔案信息
                files = realloc(files, (file_count + 1) * sizeof(struct FileInfo));
                strncpy(files[file_count].filename, full_path, sizeof(files[file_count].filename) - 1);
                files[file_count].filename[sizeof(files[file_count].filename) - 1] = '\0';
                files[file_count].line_number = getLineFromFile(full_path);
                if (files[file_count].line_number != -1) {
                    file_count++;
                }
            }
        }
        closedir(dir);
    } else {
        perror("無法打開資料夾");
        free(files);
        return 1;
    }

    // 排序檔案
    qsort(files, file_count, sizeof(struct FileInfo), compare_files);

    // 根據提供的目錄路徑來決定輸出文件的位置
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s%smerged_file.txt", directory_path, PATH_SEPARATOR);


    // 創建合併檔案
    merged_file = fopen(output_path, "w");
    if (merged_file == NULL) {
        perror("無法創建合併檔案");
        free(files);
        return 1;
    }

    // 遍歷排序後的檔案列表並進行合併
    int header_written = 0; // 用來檢查是否已經寫入過標頭

    for (int i = 0; i < file_count; i++) {
        FILE *file = fopen(files[i].filename, "r");
        if (file != NULL) {
            char line[1024];

            // 讀取檔案的第一行進行標頭檢查
            if (fgets(line, sizeof(line), file) != NULL) {
                int alphabets = 0, others = 0;
                for (int j = 0; line[j] != '\0'; j++) {
                    if (isalpha(line[j])) {
                        alphabets++;
                    } else if (!isspace(line[j])) { // 忽略空格
                        others++;
                    }
                }
                // 如果這行主要由字母組成，則視為標頭
                if (alphabets > others) {
                    // 只有在header還未寫入時才寫入
                    if (!header_written) {
                        fputs(line, merged_file);
                        header_written = 1; // 標頭已寫入
                    }
                } else {
                    // 如果不是主要由字母組成的行，直接寫入這行
                    fputs(line, merged_file);
                }

                // 繼續處理檔案的剩餘行
                while (fgets(line, sizeof(line), file) != NULL) {
                    fputs(line, merged_file);
                }
            }
            fclose(file);
        }
    }

    fclose(merged_file);
    free(files);
    printf("檔案已成功合併到 'merged_file.txt'。\n");
    return 0;
}