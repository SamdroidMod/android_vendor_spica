/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2007  Nokia Corporation
 *  Copyright (C) 2004-2009  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <signal.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <gdbus.h>

#include "glib-helper.h"
#include "btio.h"
#include "../src/manager.h"
#include "../src/adapter.h"
#include "../src/device.h"

#include "logging.h"
#include "textfile.h"
#include "dtun_device.h"

/* some local debug macros */
#ifdef DTUN_STANDALONE
#define info(format, ...) fprintf (stdout, format, ## __VA_ARGS__)
#define debug(format, ...) fprintf (stdout, format, ## __VA_ARGS__)
#define error(format, ...) fprintf (stderr, format, ## __VA_ARGS__)
#else
#define LOG_TAG "DTUN_HCID"
#include "utils/Log.h"
#define info(format, ...) LOGI (format, ## __VA_ARGS__)
#define debug(format, ...) LOGD (format, ## __VA_ARGS__)
#define error(format, ...) LOGE (format, ## __VA_ARGS__)
#endif

#define PRINTFUNC() debug("\t\t%s()\n", __FUNCTION__);

typedef enum {
	HEADSET	= 1 << 0,
	GATEWAY	= 1 << 1,
	SINK	= 1 << 2,
	SOURCE	= 1 << 3,
	CONTROL	= 1 << 4,
	TARGET	= 1 << 5,
	INVALID	= 1 << 6
} audio_service_type;

typedef enum {
		GENERIC_AUDIO = 0,
		ADVANCED_AUDIO,
		AV_REMOTE,
		GET_RECORDS
} audio_sdp_state_t;

struct audio_adapter {
	struct btd_adapter *btd_adapter;
	uint32_t hsp_ag_record_id;
	uint32_t hfp_ag_record_id;
	uint32_t hfp_hs_record_id;
	GIOChannel *hsp_ag_server;
	GIOChannel *hfp_ag_server;
	GIOChannel *hfp_hs_server;
	gint ref;
};

static gboolean auto_connect = TRUE;
static int max_connected_headsets = 1;
static DBusConnection *connection = NULL;
static GSList *adapters = NULL;
static GSList *devices = NULL;

#define AUDIO_INTERFACE "org.bluez.Audio"


struct audio_device *manager_find_device(const char *path,
					const bdaddr_t *src,
					const bdaddr_t *dst,
					const char *interface,
					gboolean connected)
{
	GSList *l;

	for (l = devices; l != NULL; l = l->next) {
		struct audio_device *dev = l->data;

		if ((path && (strcmp(path, "")) && strcmp(dev->path, path)))
			continue;

		if ((src && bacmp(src, BDADDR_ANY)) && bacmp(&dev->src, src))
			continue;

		if ((dst && bacmp(dst, BDADDR_ANY)) && bacmp(&dev->dst, dst))
			continue;

		return dev;
	}

	return NULL;
}

struct audio_device *audio_device_register(DBusConnection *conn,
					struct btd_device *device,
					const char *path, const bdaddr_t *src,
					const bdaddr_t *dst)
{
	struct audio_device *dev;

	if (!conn || !path)
		return NULL;

	dev = g_new0(struct audio_device, 1);

	dev->btd_dev = btd_device_ref(device);
	
	dev->path = g_strdup(path);
	bacpy(&dev->dst, dst);
	bacpy(&dev->src, src);
	dev->conn = dbus_connection_ref(conn);

	return dev;
}

static void audio_device_free(struct audio_device *dev)
{

	if (dev->conn)
		dbus_connection_unref(dev->conn);

	btd_device_unref(dev->btd_dev);

	g_free(dev->path);
	g_free(dev);
}

void audio_device_unregister(struct audio_device *device)
{

	if (device->sink)
		sink_unregister(device);

	g_dbus_unregister_interface(device->conn, device->path,
						AUDIO_INTERFACE);

	audio_device_free(device);
}


void	dtun_device_check_remove_audio( struct btd_device *device )
{
	GSList *l;

	for (l = devices; l != NULL; l = l->next) {
		struct audio_device *dev = l->data;

		if( dev->btd_dev == device )
			audio_device_unregister( dev );
	}
}

struct audio_device *manager_get_device(DBusConnection *conn,
                                        const bdaddr_t *src,
					const bdaddr_t *dst,
					gboolean create)
{
	struct audio_device *dev;
	struct btd_adapter *adapter;
	struct btd_device *device;
	char addr[18];
	const char *path;

	dev = manager_find_device(NULL, src, dst, NULL, FALSE);
	if (dev)
		return dev;

	if (!create)
		return NULL;

	ba2str(src, addr);

	adapter = manager_find_adapter(src);
	if (!adapter) {
		error("Unable to get a btd_adapter object for %s",
				addr);
		return NULL;
	}

	ba2str(dst, addr);

	device = adapter_get_device(conn, adapter, addr);
	if (!device) {
		error("Unable to get btd_device object for %s", addr);
		return NULL;
	}

	path = device_get_path(device);

	dev = audio_device_register(conn, device, path, src, dst);
	if (!dev)
		return NULL;

	devices = g_slist_append(devices, dev);

	return dev;
}

