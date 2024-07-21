#include <gst/gst.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GstElement *pipeline;
static gboolean eos_received = FALSE;

// Signal handler for SIGINT to stop recording
void sigint_handler(int sig) {
    g_print("Received SIGINT, stopping...\n");
    gst_element_send_event(GST_ELEMENT(pipeline), gst_event_new_eos());
}

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

int main(int argc, char *argv[]) {
    GstElement *source, *tee;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;

    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Create elements
    source = gst_element_factory_make("ximagesrc", "source");

    int num = 3;
    int video_formats[3][3] = {
        {1280, 720, 30},
        {640, 480, 15},
        {426, 240, 5}
    };
    GstElement *videoscales[num];
    GstElement *converts[num];
    GstElement *videorates[num];
    GstElement *sinks[num];
    GstElement *queues[num];
    GstCaps *caps[num];

    tee = gst_element_factory_make("tee", "tee");

    for(int i = 0; i < num; i++) {
        videoscales[i] = gst_element_factory_make("videoscale", concat_string_and_number("videoscale", i));
        converts[i] = gst_element_factory_make("videoconvert", concat_string_and_number("convert", i));
        videorates[i] = gst_element_factory_make("videorate", concat_string_and_number("videorate", i));
        sinks[i] = gst_element_factory_make("xvimagesink", concat_string_and_number("sink", i));
        queues[i] = gst_element_factory_make("queue", concat_string_and_number("queue", i));
    }

    // Check that all elements are created successfully
    if (!source || !tee) {
        g_printerr("Failed to create one of the elements.\n");
        return -1;
    }
    for (int i = 0; i < num; i++) {
        if (!videoscales[i] || !converts[i] || !videorates[i] || !sinks[i] || !queues[i]) {
            g_printerr("Failed to create one of the elements.\n");
            return -1;
        }
    }

    // Set properties for elements
    g_object_set(source, "startx", 0, "use-damage", 0, NULL);

    // Create pipeline and add elements to it
    pipeline = gst_pipeline_new("multi-screen-recorder");
    gst_bin_add_many(GST_BIN(pipeline), source, tee, NULL);
    for (int i = 0; i < num; i++) {
        gst_bin_add_many(GST_BIN(pipeline), queues[i], videoscales[i], converts[i], videorates[i], sinks[i], NULL);
    }

    // Set caps for videoscale

    for (int i = 0; i < num; i++) {
        caps[i] = gst_caps_new_simple("video/x-raw",
                               "framerate", GST_TYPE_FRACTION, video_formats[i][2], 1,
                               "width", G_TYPE_INT, video_formats[i][0],
                               "height", G_TYPE_INT, video_formats[i][1],
                               NULL);
    }

    // Link elements
    if (!gst_element_link_many(source, tee, NULL)) {
        g_printerr("Failed to link source to tee.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    for (int i = 0; i < num; i++) {
        if (!gst_element_link_many(tee, queues[i], videorates[i], videoscales[i], NULL) ||
            !gst_element_link_filtered(videoscales[i], converts[i], caps[i]) ||
            !gst_element_link_many(converts[i], sinks[i], NULL)) {
            g_printerr("Failed to link elements for first sink.\n");
            gst_object_unref(pipeline);
            return -1;
        }
    }

    for (int i = 0; i < num; i++) {
        gst_caps_unref(caps[i]);
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