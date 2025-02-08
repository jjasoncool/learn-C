#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h> // 新增此行

#ifdef _WIN32
    #define PATH_SEPARATOR "\\"
#else
    #define PATH_SEPARATOR "/"
#endif

// 計算磁場強度
float calculate_magnitude(float x, float y, float z) {
    return sqrt(x*x + y*y + z*z);
}

// 將UTC時間轉換為UTC+8
void convert_to_utc8(char *date_time) {
    struct tm tm = {0};
    // 將字符串解析為tm結構體
    strptime(date_time, "%Y-%m-%d %H:%M:%S.%f", &tm);
    // 轉換為time_t，這個值是以秒為單位的UTC時間
    time_t time = mktime(&tm);
    // 增加8小時來轉換為UTC+8
    time += 8 * 3600;
    // 將時間轉換回tm結構體，但這次是本地時間（UTC+8）
    struct tm *local = localtime(&time);
    // 格式化新的時間字符串，忽略毫秒部分，因為原始數據也忽略了
    strftime(date_time, 20, "%Y-%m-%d %H:%M:%S.000", local);
}

void process_file(const char *input_file, const char *output_file) {
    FILE *in = fopen(input_file, "r");
    if (in == NULL) {
        perror("無法打開輸入文件");
        return;
    }

    FILE *out = fopen(output_file, "w");
    if (out == NULL) {
        perror("無法創建輸出文件");
        fclose(in);
        return;
    }

    // 跳過文件的前11行，這些是標題或不相關的數據
    for (int i = 0; i < 11; i++) {
        char buffer[1024];
        fgets(buffer, sizeof(buffer), in);
    }

    char line[1024];
    while (fgets(line, sizeof(line), in)) {
        // 初始化變數來存儲日期、時間和數據
        char date_time[20], doy[10]; // doy: Day of Year
        float ncgx, ncgy, ncgz, magnitude;

        // 解析每行數據，%*s 跳過不需要的欄位
        if (sscanf(line, "%s %s %*s %f %f %f %*f", date_time, doy, &ncgx, &ncgy, &ncgz) == 6) {
            // 將UTC時間轉換為UTC+8
            convert_to_utc8(date_time);
            // 計算磁場強度
            magnitude = calculate_magnitude(ncgx, ncgy, ncgz);
            // 輸出到文件，包含日期、時間和計算結果
            fprintf(out, "%s %s\t%f\t%f\t%f\t%f\n", date_time, doy, ncgx, ncgy, ncgz, magnitude);
        }
    }

    fclose(in);
    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("使用方法: %s <目錄>\n", argv[0]);
        return 1;
    }

    DIR *dir;
    struct dirent *ent;
    // 打開指定的目錄
    if ((dir = opendir(argv[1])) != NULL) {
        char input_path[1024], output_path[1024];
        struct stat st;
        // 遍歷目錄中的每個文件
        while ((ent = readdir(dir)) != NULL) {
            // 只處理普通文件（非目錄）和.txt結尾的文件
            snprintf(input_path, sizeof(input_path), "%s" PATH_SEPARATOR "%s", argv[1], ent->d_name);
            if (stat(input_path, &st) == 0 && S_ISREG(st.st_mode) && strstr(ent->d_name, ".txt") != NULL) { // 只處理.txt文件
                snprintf(output_path, sizeof(output_path), "%s" PATH_SEPARATOR "%s_processed.txt", argv[1], ent->d_name);

                // 對每個符合條件的文件進行處理
                process_file(input_path, output_path);
            }
        }
        closedir(dir);
        printf("資料夾中的文本文件已處理完畢。\n");
    } else {
        perror("無法開啟目錄");
        return 1;
    }

    return 0;
}