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
    GstElement *source, *videoscale, *convert, *sink;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    GstCaps *caps;
    GstPad *sinkpad;

    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Create elements
    source = gst_element_factory_make("ximagesrc", "source");
    videoscale = gst_element_factory_make("videoscale", "videoscale");
    convert = gst_element_factory_make("videoconvert", "convert");
    sink = gst_element_factory_make("xvimagesink", "sink");

    // Check that all elements are created successfully
    if (!source || !videoscale || !convert || !sink) {
        g_printerr("Failed to create one of the elements.\n");
        return -1;
    }

    // Set properties for elements
    g_object_set(source, "startx", 0, "use-damage", 0, NULL);

    // Create pipeline and add elements to it
    pipeline = gst_pipeline_new("screen-recorder");
    gst_bin_add_many(GST_BIN(pipeline), source, videoscale, convert, sink, NULL);

    // Set caps for videoscale
    int resolutions[6][2] = {
        {256, 144},
        {426, 240},
        {480, 360},
        {640, 480},
        {1280, 720},
        {1920, 1080}
    };
    caps = gst_caps_new_simple("video/x-raw",
                               "framerate", GST_TYPE_FRACTION, 30, 1,
                               "width", G_TYPE_INT, resolutions[4][0],
                               "height", G_TYPE_INT, resolutions[4][1],
                               NULL);

    // Try linking elements with caps
    if (!gst_element_link_filtered(source, videoscale, NULL)) {
        g_printerr("Failed to link source to videoscale.\n");
        gst_object_unref(pipeline);
        return -1;
    }
    
    if (!gst_element_link_filtered(videoscale, convert, caps)) {
        g_printerr("Failed to link videoscale to videoconvert with caps.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    if (!gst_element_link_many(convert, sink, NULL)) {
        g_printerr("Failed to link elements after videoconvert.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    gst_caps_unref(caps);

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