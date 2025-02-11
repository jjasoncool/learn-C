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

// 手動解析日期時間字符串
void parse_datetime(const char *date_time_str, struct tm *tm, char *millis) {
    sscanf(date_time_str, "%4d-%2d-%2d %2d:%2d:%2d.%3s",
           &tm->tm_year, &tm->tm_mon, &tm->tm_mday,
           &tm->tm_hour, &tm->tm_min, &tm->tm_sec, millis);
    tm->tm_year -= 1900; // 年份從1900開始
    tm->tm_mon -= 1;     // 月份從0開始
    tm->tm_isdst = -1;   // 讓mktime自動檢測夏令時
}

// 處理跨天和月末/年末的轉換
static void normalize_time(struct tm *tm) {
    time_t t = mktime(tm);
    *tm = *localtime(&t); // normalize tm
}

// 將UTC時間轉換為UTC+8
void convert_to_utc8(char *date_time) {
    struct tm tm = {0};
    char millis[4] = {0}; // 存放毫秒部分
    // 手動解析日期時間字符串
    parse_datetime(date_time, &tm, millis);
    // 增加8小時
    tm.tm_hour += 8;
    // 檢查是否需要跨天
    if (tm.tm_hour >= 24) {
        tm.tm_hour -= 24;
        tm.tm_mday += 1;
    }
    // 處理跨月和跨年
    normalize_time(&tm);
    // 格式化新的時間字符串，保留毫秒部分
    char temp[30]; // 臨時緩衝區來存放格式化後的時間
    strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M:%S", &tm);
    snprintf(date_time, 24, "%s.%s", temp, millis); // 組合日期時間和毫秒
}

void convert_date_format(const char* input, char* output, size_t output_size) {
    struct tm tm = {0};
    char millis[4];

    // 解析輸入的日期時間字符串
    sscanf(input, "%4d-%2d-%2d %2d:%2d:%2d.%3s",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec, millis);

    tm.tm_year -= 1900; // 年份從1900開始計算
    tm.tm_mon -= 1;     // 月份從0開始計算

    // 格式化輸出字符串，只包含日期和時間，忽略毫秒
    strftime(output, output_size, "%m/%d/%y %H:%M:%S", &tm);
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
    for (int i = 0; i < 13; i++) {
        char buffer[1024];
        fgets(buffer, sizeof(buffer), in);
        // printf("跳過第%d行: %s", i+1, buffer);
    }

    char line[1024];
    int line_count = 0;
    printf("\n開始處理數據...\n");
    while (fgets(line, sizeof(line), in)) {
        line_count++;
        // 初始化變數來存儲日期、時間和數據
        char date_time[24]; // doy: Day of Year
        float ncgx, ncgy, ncgz, magnitude;

        // 解析每行數據，%*s 跳過不需要的欄位
        if (sscanf(line, "%24c %*s %f %f %f %*f", date_time, &ncgx, &ncgy, &ncgz) == 4) {
            // 確保字符串以空字符結尾
            date_time[24] = '\0';
            // printf("抓到時間字串:%s \n", date_time);

            // 將UTC時間轉換為UTC+8
            // char original_time[24];
            // strcpy(original_time, date_time);
            convert_to_utc8(date_time);
            char formated_datetime[20];
            convert_date_format(date_time, formated_datetime, sizeof(formated_datetime));
            // printf("時間轉換: %s -> %s\n", original_time, date_time);

            // 計算磁場強度
            magnitude = calculate_magnitude(ncgx, ncgy, ncgz);
            // printf("磁場數據: X=%.2f, Y=%.2f, Z=%.2f\n", ncgx, ncgy, ncgz);
            // printf("計算強度: %.2f\n", magnitude);

            // 輸出到文件
            fprintf(out, "%s\t%f\n", formated_datetime, magnitude);
        }
    }

    printf("總共處理了%d行數據\n", line_count);

    fclose(in);
    fclose(out);
}

int main(int argc, char *argv[]) {
    // 設置輸出編碼為 UTF-8
    #ifdef _WIN32
        system("chcp 65001");
    #endif

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
            // 首先檢查文件是否為普通文件並有.sec後綴，節省路徑組合的開銷
            if (strstr(ent->d_name, ".sec") != NULL) {
                printf("正在處理文件: %s\n", input_path);
                // 組合完整路徑，然後檢查文件類型
                snprintf(input_path, sizeof(input_path), "%s" PATH_SEPARATOR "%s", argv[1], ent->d_name);
                if (stat(input_path, &st) == 0 && S_ISREG(st.st_mode)) {
                    // 生成輸出文件的路徑，移除.sec並添加.txt
                    char *dot = strrchr(ent->d_name, '.');
                    if (dot && strcmp(dot, ".sec") == 0) {
                        *dot = '\0'; // 移除.sec 在C語言中，字符串是以空字符 (\0) 結束的
                        snprintf(output_path, sizeof(output_path), "%s" PATH_SEPARATOR "%s.txt", argv[1], ent->d_name);
                        *dot = '.';  // 恢復原文件名
                    } else {
                        // 如果文件不是以.sec結尾，則直接添加.txt
                        snprintf(output_path, sizeof(output_path), "%s" PATH_SEPARATOR "%s.txt", argv[1], ent->d_name);
                    }

                    // 對每個符合條件的文件進行處理
                    process_file(input_path, output_path);
                } else {
                    // 如果stat失敗，報告錯誤
                    perror("無法獲取文件信息");
                }
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