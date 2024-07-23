#include <gst/gst.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 100
#define MAX_VIDEO_FORMATS 10

static GstElement *pipeline;
static gboolean eos_received = FALSE;

// Signal handler for SIGINT to stop recording
void sigint_handler(int sig) {
    g_print("Received SIGINT, stopping...\n");
    gst_element_send_event(GST_ELEMENT(pipeline), gst_event_new_eos());
}

// Функция для конкатенации строки и числа
char* concat_string_and_number(const char* str, int video_format_count) {
    // Определяем длину строки и числа
    size_t str_len = strlen(str);
    char num_str[12]; // Максимальная длина int в десятичном представлении + знак
    snprintf(num_str, sizeof(num_str), "%d", video_format_count);
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

// Структура для хранения параметров видеоформата
typedef struct {
    int width;
    int height;
    int framerate;
} VideoFormat;

void swap(VideoFormat* xp, VideoFormat* yp) 
{ 
    VideoFormat temp = *xp; 
    *xp = *yp; 
    *yp = temp; 
} 
  
// Function to perform Selection Sort 
void selectionSort(VideoFormat arr[], int n) 
{ 
    int i, j, min_idx; 
  
    // One by one move boundary of 
    // unsorted subarray 
    for (i = 0; i < n - 1; i++) { 
        // Find the minimum element in 
        // unsorted array 
        min_idx = i; 
        for (j = i + 1; j < n; j++) 
            if (arr[j].width > arr[min_idx].width) 
                min_idx = j; 
  
        // Swap the found minimum element 
        // with the first element 
        swap(&arr[min_idx], &arr[i]); 
    } 
}

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

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("Not enough arguments! Please enter configuration file name!");
        return -1;
    }
    const char* filename = argv[1];
    int display_number;
    VideoFormat video_formats[MAX_VIDEO_FORMATS];
    int video_format_count;

    if (parse_config_file(filename, &display_number, video_formats, &video_format_count) != 0) {
        fprintf(stderr, "Error of parsing configuration file.\n");
        return EXIT_FAILURE;
    }

    selectionSort(video_formats, video_format_count);

    GstElement *source, *tee;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;

    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Create elements
    source = gst_element_factory_make("ximagesrc", "source");

    GstElement *videoscales[video_format_count];
    GstElement *converts[video_format_count];
    GstElement *videorates[video_format_count];
    GstElement *sink, *compositor;
    GstElement *queues[video_format_count];
    GstCaps *caps[video_format_count];

    tee = gst_element_factory_make("tee", "tee");

    for(int i = 0; i < video_format_count; i++) {
        videoscales[i] = gst_element_factory_make("videoscale", concat_string_and_number("videoscale", i));
        converts[i] = gst_element_factory_make("videoconvert", concat_string_and_number("convert", i));
        videorates[i] = gst_element_factory_make("videorate", concat_string_and_number("videorate", i));
        queues[i] = gst_element_factory_make("queue", concat_string_and_number("queue", i));
    }

    compositor = gst_element_factory_make("compositor", "compositor");
    sink = gst_element_factory_make("xvimagesink", "sink");

    // Check that all elements are created successfully
    if (!source || !tee || !sink || !compositor) {
        g_printerr("Failed to create one of the elements.\n");
        return -1;
    }
    for (int i = 0; i < video_format_count; i++) {
        if (!videoscales[i] || !converts[i] || !videorates[i] || !queues[i]) {
            g_printerr("Failed to create one of the elements.\n");
            return -1;
        }
    }

    // Set properties for elements
    g_object_set(source, "startx", 0, "use-damage", 0, "display-name", concat_string_and_number(":", display_number), NULL);

    g_object_set(compositor, "background", 1, NULL);

    // Create pipeline and add elements to it
    pipeline = gst_pipeline_new("multi-screen-recorder");
    gst_bin_add_many(GST_BIN(pipeline), source, tee, NULL);
    for (int i = 0; i < video_format_count; i++) {
        gst_bin_add_many(GST_BIN(pipeline), queues[i], videoscales[i], converts[i], videorates[i], NULL);
    }
    gst_bin_add_many(GST_BIN(pipeline), compositor, sink, NULL);

    // Set caps for videoscale

    for (int i = 0; i < video_format_count; i++) {
        caps[i] = gst_caps_new_simple("video/x-raw",
                               "framerate", GST_TYPE_FRACTION, video_formats[i].framerate, 1,
                               "width", G_TYPE_INT, video_formats[i].width,
                               "height", G_TYPE_INT, video_formats[i].height,
                               NULL);
    }

    // Link elements
    if (!gst_element_link_many(source, tee, NULL)) {
        g_printerr("Failed to link source to tee.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    for (int i = 0; i < video_format_count; i++) {
        if (!gst_element_link_many(tee, queues[i], videorates[i], videoscales[i], NULL) ||
            !gst_element_link_filtered(videoscales[i], converts[i], caps[i])) {
            g_printerr("Failed to link elements for first sink.\n");
            gst_object_unref(pipeline);
            return -1;
        }
    }

    for (int i = 0; i < video_format_count; i++) {
        gst_caps_unref(caps[i]);
    }

    GstPad *sinkpads[video_format_count];
    for (int i = 0; i < video_format_count; i++) {
        sinkpads[i] = gst_element_request_pad_simple(compositor, "sink_%u");
    }
    int xpos_sum = 0;
    for (int i = 1; i < video_format_count; i++) {
        g_object_set(sinkpads[i], "xpos", xpos_sum, "ypos", video_formats[0].height, "width", video_formats[i].width, "height", video_formats[i].height, "operator", 0, NULL);
        xpos_sum += video_formats[i].width;
    }
    g_object_set(sinkpads[0], "xpos", xpos_sum <= video_formats[0].width ? 0 : (xpos_sum - video_formats[0].width) / 2, "ypos", 0, NULL);
    for (int i = 0; i < video_format_count; i++) {
        gst_object_unref(sinkpads[i]);
    }

    if (!gst_element_link(compositor, sink)) {
        g_printerr("Failed to link compositor to sink.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    for(int i = 0; i < video_format_count; i++) {
        if (!gst_element_link(converts[i], compositor)) {
            g_printerr("Failed to link queues to compositor.\n");
            gst_object_unref(pipeline);
            return -1;
        }
    }

    // Set the pipeline to the PLAYING state
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to set pipeline to PLAYING state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Register SIGINT handler
    signal(SIGINT, sigint_handler);

    // Wait for error or EOS messages
    bus = gst_element_get_bus(pipeline);
    while (!eos_received) {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                         GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    if (msg != NULL) {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR:
                    gst_message_parse_error(msg, &err, &debug_info);
                    g_printerr("Error: %s\n", err->message);
                    g_error_free(err);
                    g_free(debug_info);
                    break;
                case GST_MESSAGE_EOS:
                    g_print("End of stream reached.\n");
                    eos_received = TRUE;
                    break;
                default:
                    g_printerr("Unexpected message received.\n");
                    break;
            }
            gst_message_unref(msg);
        }
    }

    // Stop pipeline and release resources
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);

    return 0;
}