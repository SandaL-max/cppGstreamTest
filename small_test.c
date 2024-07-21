#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Функция для конкатенации строки и числа
char* concat_string_and_number(const char* str, int num) {
    // Определяем длину строки и числа
    size_t str_len = strlen(str);
    char num_str[12]; // Максимальная длина int в десятичном представлении + знак
    snprintf(num_str, sizeof(num_str), "%d", num);
    size_t num_len = strlen(num_str);

    // Выделяем память для результирующей строки
    char* result = (char*)malloc(str_len + num_len + 1);
    if (result == NULL) {
        fprintf(stderr, "Ошибка выделения памяти\n");
        exit(EXIT_FAILURE);
    }

    // Конкатенируем строку и число
    strcpy(result, str);
    strcat(result, num_str);

    return result;
}

int main() {
    const char* my_string = "videoscale";
    int my_number = 3;

    char* result = concat_string_and_number(my_string, my_number);
    printf("%s\n", result);

    // Освобождаем выделенную память
    free(result);

    return 0;
}