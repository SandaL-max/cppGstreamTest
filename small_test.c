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
    GstElement *source1, *source2, *videoscale1, *videoscale2, *convert1, *convert2, *queue1, *queue2, *compositor, *sink;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    GstCaps *caps1, *caps2;

    // Initialize GStreamer
    gst_init(&argc, &argv);

    // Create elements
    source1 = gst_element_factory_make("ximagesrc", "source1");
    videoscale1 = gst_element_factory_make("videoscale", "videoscale1");
    convert1 = gst_element_factory_make("videoconvert", "convert1");
    queue1 = gst_element_factory_make("queue", "queue1");

    source2 = gst_element_factory_make("ximagesrc", "source2");
    videoscale2 = gst_element_factory_make("videoscale", "videoscale2");
    convert2 = gst_element_factory_make("videoconvert", "convert2");
    queue2 = gst_element_factory_make("queue", "queue2");

    compositor = gst_element_factory_make("compositor", "compositor");
    sink = gst_element_factory_make("xvimagesink", "sink");

    // Check that all elements are created successfully
    if (!source1 || !videoscale1 || !convert1 || !queue1 ||
        !source2 || !videoscale2 || !convert2 || !queue2 ||
        !compositor || !sink) {
        g_printerr("Failed to create one of the elements.\n");
        return -1;
    }

    // Set properties for elements
    g_object_set(source1, "startx", 0, "starty", 0, "use-damage", 0, NULL);
    g_object_set(source2, "startx", 0, "starty", 0, "use-damage", 0, NULL);

    // Create pipeline and add elements to it
    pipeline = gst_pipeline_new("multi-screen-recorder");
    gst_bin_add_many(GST_BIN(pipeline), source1, videoscale1, convert1, queue1,
                     source2, videoscale2, convert2, queue2,
                     compositor, sink, NULL);

    // Set caps for videoscale
    caps1 = gst_caps_new_simple("video/x-raw",
                               "framerate", GST_TYPE_FRACTION, 5, 1,
                               "width", G_TYPE_INT, 640,
                               "height", G_TYPE_INT, 360,
                               NULL);

    caps2 = gst_caps_new_simple("video/x-raw",
                               "framerate", GST_TYPE_FRACTION, 30, 1,
                               "width", G_TYPE_INT, 1280,
                               "height", G_TYPE_INT, 720,
                               NULL);

    // Link elements for source 1
    if (!gst_element_link_many(source1, videoscale1, convert1, NULL) ||
        !gst_element_link_filtered(convert1, queue1, caps1)) {
        g_printerr("Failed to link elements for source 1.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Link elements for source 2
    if (!gst_element_link_many(source2, videoscale2, convert2, NULL) ||
        !gst_element_link_filtered(convert2, queue2, caps2)) {
        g_printerr("Failed to link elements for source 2.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Set properties for compositor
    GstPad *sinkpad1 = gst_element_request_pad_simple(compositor, "sink_%u");
    GstPad *sinkpad2 = gst_element_request_pad_simple(compositor, "sink_%u");
    g_object_set(sinkpad1, "xpos", 0, "ypos", 0, NULL);
    g_object_set(sinkpad2, "xpos", 640, "ypos", 0, NULL);
    gst_object_unref(sinkpad1);
    gst_object_unref(sinkpad2);
    //g_object_set(sink, "render-rectangle")

    // Link compositor to sink
    if (!gst_element_link(compositor, sink)) {
        g_printerr("Failed to link compositor to sink.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    // Link queues to compositor
    if (!gst_element_link(queue1, compositor) ||
        !gst_element_link(queue2, compositor)) {
        g_printerr("Failed to link queues to compositor.\n");
        gst_object_unref(pipeline);
        return -1;
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
