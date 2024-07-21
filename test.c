#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 100
#define MAX_VIDEO_FORMATS 10

// Структура для хранения параметров видеоформата
typedef struct {
    int width;
    int height;
    int framerate;
} VideoFormat;

// Функция для парсинга строки с видеоформатом
int parse_video_format(const char* str, VideoFormat* vf) {
    return sscanf(str, "%dx%d, %d", &vf->width, &vf->height, &vf->framerate) == 3;
}

// Функция для парсинга конфигурационного файла
int parse_config_file(const char* filename, int* display_number, VideoFormat* video_formats, int* video_format_count) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Ошибка открытия файла");
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    *display_number = -1;
    *video_format_count = 0;

    while (fgets(line, sizeof(line), file)) {
        // Удаляем символ новой строки в конце строки
        line[strcspn(line, "\n")] = '\0';

        // Парсим display_number
        if (strncmp(line, "display_number=", 15) == 0) {
            *display_number = atoi(line + 15);
        } 
        // Парсим video_format
        else if (strncmp(line, "video_format=", 13) == 0) {
            if (*video_format_count < MAX_VIDEO_FORMATS && parse_video_format(line + 13, &video_formats[*video_format_count])) {
                (*video_format_count)++;
            } else {
                fprintf(stderr, "Ошибка парсинга video_format: %s\n", line + 13);
            }
        }
    }

    fclose(file);
    return 0;
}

int main() {
    const char* filename = "config.txt";
    int display_number;
    VideoFormat video_formats[MAX_VIDEO_FORMATS];
    int video_format_count;

    if (parse_config_file(filename, &display_number, video_formats, &video_format_count) != 0) {
        fprintf(stderr, "Ошибка парсинга файла конфигурации.\n");
        return EXIT_FAILURE;
    }

    // Вывод результатов парсинга
    printf("Display Number: %d\n", display_number);
    for (int i = 0; i < video_format_count; i++) {
        printf("Video Format %d: %d, %d, %d\n", i + 1, video_formats[i].width, video_formats[i].height, video_formats[i].framerate);
    }

    return 0;
}