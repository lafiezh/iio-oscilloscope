/**
 * Copyright (C) 2019 Analog Devices, Inc.
 *
 * Licensed under the GPL-2.
 *
 **/
#include <stdio.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <gtkdatabox.h>
#include <gtkdatabox_grid.h>
#include <gtkdatabox_points.h>
#include <gtkdatabox_lines.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include <iio.h>

#include "../libini2.h"
#include "../osc.h"
#include "../iio_widget.h"
#include "../osc_plugin.h"
#include "../config.h"
#include "../eeprom.h"
#include "./block_diagram.h"
#include "dac_data_manager.h"

#define THIS_DRIVER "GENERIC_PLUGIN"

#define ARRAY_SIZE(x) (!sizeof(x) ?: sizeof(x) / sizeof((x)[0]))

#define DAC_DEVICE "cf-ad9361-dds-core-lpc"

static struct dac_data_manager *dac_tx_manager;

static struct iio_context *ctx;
static struct iio_device *dac;

static struct iio_widget tx_widgets[100];
static unsigned int num_tx;

static bool can_update_widgets;

static void tx_update_values(void)
{
	iio_update_widgets(tx_widgets, num_tx);
}

static int compare_gain(const char *a, const char *b) __attribute__((unused));
static int compare_gain(const char *a, const char *b)
{
	double val_a, val_b;
	sscanf(a, "%lf", &val_a);
	sscanf(b, "%lf", &val_b);

	if (val_a < val_b)
		return -1;
	else if(val_a > val_b)
		return 1;
	else
		return 0;
}

static void save_widget_value(GtkWidget *widget, struct iio_widget *iio_w)
{
	iio_w->save(iio_w);
}

static void make_widget_update_signal_based(struct iio_widget *widgets,
		unsigned int num_widgets)
{
	char signal_name[25];
	unsigned int i;

	for (i = 0; i < num_widgets; i++) {
		if (GTK_IS_CHECK_BUTTON(widgets[i].widget))
			sprintf(signal_name, "%s", "toggled");
		else if (GTK_IS_TOGGLE_BUTTON(widgets[i].widget))
			sprintf(signal_name, "%s", "toggled");
		else if (GTK_IS_SPIN_BUTTON(widgets[i].widget))
			sprintf(signal_name, "%s", "value-changed");
		else if (GTK_IS_COMBO_BOX_TEXT(widgets[i].widget))
			sprintf(signal_name, "%s", "changed");
		else
			printf("unhandled widget type, attribute: %s\n", widgets[i].attr_name);

		if (GTK_IS_SPIN_BUTTON(widgets[i].widget) &&
		    widgets[i].priv_progress != NULL) {
			iio_spin_button_progress_activate(&widgets[i]);
		} else {
			g_signal_connect(G_OBJECT(widgets[i].widget), signal_name,
					 G_CALLBACK(save_widget_value), &widgets[i]);
		}
	}
}

static bool generic_identify(const struct osc_plugin *plugin)
{
	/* Use the OSC's IIO context just to detect the devices */
	struct iio_context *osc_ctx = get_context_from_osc();

	return 1;
}

static GtkWidget * generic_init(struct osc_plugin *plugin, GtkWidget *notebook,
				const char *ini_fn)
{
	GtkBuilder *builder;
	GtkWidget *generic_panel;
	GtkWidget *dds_container;
	GtkWidget *dds_vbox;
	bool generic_en = false;
	guint i;

	ctx = osc_create_context();
	if (!ctx)
		return NULL;

	builder = gtk_builder_new();
	if (osc_load_glade_file(builder, "generic_plugin") < 0)
		return NULL;

	generic_panel = GTK_WIDGET(gtk_builder_get_object(builder, "generic_panel"));
	dds_vbox = GTK_WIDGET(gtk_builder_get_object(builder, "dds_vbox"));

	int count = iio_context_get_devices_count(ctx);
	for (i = 0; i < count; i++) {
		dac = iio_context_get_device(ctx, i);

		dac_tx_manager = dac_data_manager_new(dac, NULL, ctx);
		if (!dac_tx_manager)
			continue;

		generic_en = true;

		dds_container = gtk_frame_new("DDS");
		gtk_container_add(GTK_CONTAINER(dds_vbox), dds_container);
		gtk_container_add(GTK_CONTAINER(dds_container),
				  dac_data_manager_get_gui_container(dac_tx_manager));
		gtk_widget_show_all(dds_container);

		make_widget_update_signal_based(tx_widgets, num_tx);

		tx_update_values();
		dac_data_manager_update_iio_widgets(dac_tx_manager);

		dac_data_manager_set_buffer_size_alignment(dac_tx_manager, 16);
		dac_data_manager_set_buffer_chooser_current_folder(dac_tx_manager,
				OSC_WAVEFORM_FILE_PATH);

		can_update_widgets = true;
	}

	if (!generic_en) {
		osc_destroy_context(ctx);
		return NULL;
	}

	return generic_panel;
}

static void context_destroy(struct osc_plugin *plugin, const char *ini_fn)
{
	if (dac_tx_manager) {
		dac_data_manager_free(dac_tx_manager);
		dac_tx_manager = NULL;
	}

	osc_destroy_context(ctx);
}

struct osc_plugin plugin = {
	.name = THIS_DRIVER,
	.identify = generic_identify,
	.init = generic_init,
	.destroy = context_destroy,
};
