#include <gst/gst.h>
#include <glib.h>
#include <signal.h>

static GstElement *pipeline;
static gboolean eos_received = FALSE;

// Signal handler for SIGINT to stop recording
void sigint_handler(int sig) {
    g_print("Received SIGINT, stopping...\n");
    gst_element_send_event(GST_ELEMENT(pipeline), gst_event_new_eos());
}

int main(int argc, char *argv[]) {
    GstElement *source, *videoscale1, *videoscale2, *convert1, *convert2, *sink1, *sink2, *tee, *queue1, *queue2;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    GstCaps *caps1, *caps2;
    GstPad *sinkpad1, *sinkpad2;

    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Create elements
    source = gst_element_factory_make("ximagesrc", "source");
    videoscale1 = gst_element_factory_make("videoscale", "videoscale1");
    convert1 = gst_element_factory_make("videoconvert", "convert1");
    sink1 = gst_element_factory_make("xvimagesink", "sink1");

    videoscale2 = gst_element_factory_make("videoscale", "videoscale2");
    convert2 = gst_element_factory_make("videoconvert", "convert2");
    sink2 = gst_element_factory_make("xvimagesink", "sink2");

    tee = gst_element_factory_make("tee", "tee");
    queue1 = gst_element_factory_make("queue", "queue1");
    queue2 = gst_element_factory_make("queue", "queue2");

    // Check that all elements are created successfully
    if (!source || !videoscale1 || !convert1 || !sink1 || !videoscale2 || !convert2 || !sink2 || !tee || !queue1 || !queue2) {
        g_printerr("Failed to create one of the elements.\n");
        return -1;
    }

    // Set properties for elements
    g_object_set(source, "startx", 0, "use-damage", 0, NULL);

    // Create pipeline and add elements to it
    pipeline = gst_pipeline_new("multi-screen-recorder");
    gst_bin_add_many(GST_BIN(pipeline), source, tee, queue1, videoscale1, convert1, sink1, queue2, videoscale2, convert2, sink2, NULL);

    // Set caps for videoscale
    int resolutions1[2] = {1280, 720};
    int resolutions2[2] = {640, 480};
    int framerate = 30;

    caps1 = gst_caps_new_simple("video/x-raw",
                               "framerate", GST_TYPE_FRACTION, framerate, 1,
                               "width", G_TYPE_INT, resolutions1[0],
                               "height", G_TYPE_INT, resolutions1[1],
                               NULL);

    caps2 = gst_caps_new_simple("video/x-raw",
                               "framerate", GST_TYPE_FRACTION, framerate, 1,
                               "width", G_TYPE_INT, resolutions2[0],
                               "height", G_TYPE_INT, resolutions2[1],
                               NULL);

    // Link elements
    if (!gst_element_link_many(source, tee, NULL)) {
        g_printerr("Failed to link source to tee.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    if (!gst_element_link_many(tee, queue1, videoscale1, NULL) ||
        !gst_element_link_filtered(videoscale1, convert1, caps1) ||
        !gst_element_link_many(convert1, sink1, NULL)) {
        g_printerr("Failed to link elements for first sink.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    if (!gst_element_link_many(tee, queue2, videoscale2, NULL) ||
        !gst_element_link_filtered(videoscale2, convert2, caps2) ||
        !gst_element_link_many(convert2, sink2, NULL)) {
        g_printerr("Failed to link elements for second sink.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    gst_caps_unref(caps1);
    gst_caps_unref(caps2);

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