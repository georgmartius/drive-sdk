/*
 *  simplified version of the anki drive sdk
 *
 *  Copyright (C) 2014 Anki, Inc.
 *      2015 Georg Martius
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
 *  Portions of this software are derived from BlueZ, a Bluetooth protocol stack for
 *  Linux. The license for BlueZ is included below.
 */

/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011  Nokia Corporation
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
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <glib.h>

#include <bzle/bluetooth/uuid.h>
#include <bzle/bluetooth/btio.h>
#include <bzle/gatt/att.h>
#include <bzle/gatt/gattrib.h>
#include <bzle/gatt/gatt.h>
#include <bzle/gatt/utils.h>
#include <ankidrive.h>

#include <anki-simplified.h>

static GIOChannel *iochannel = NULL;
static GAttrib *attrib = NULL;
static GMainLoop *event_loop;
static GThread* g_thread;

struct characteristic_data {
  GAttrib *attrib;
  uint16_t start;
  uint16_t end;
};

static char *opt_src = NULL;
static char *opt_dst = NULL;
static char *opt_dst_type = NULL;
static char *opt_sec_level = NULL;
static bt_uuid_t *opt_uuid = NULL;
static int opt_psm = 0;
static int opt_mtu = 0;

typedef struct anki_vehicle {
  struct gatt_char read_char;
  struct gatt_char write_char;
} anki_vehicle_t;

static anki_vehicle_t vehicle;
static localization_t loc;

static int verbose=0;

static void discover_services(void);

static enum state {
  STATE_DISCONNECTED,
  STATE_CONNECTING,
  STATE_CONNECTED
} conn_state;

#define error(fmt, arg...)                      \
  fprintf(stderr, "Error: " fmt, ## arg)

#define failed(fmt, arg...)				\
  fprintf(stderr, "Command Failed: " fmt, ## arg)

void set_state(enum state st){conn_state = st; }

static void handle_vehicle_msg_response(const uint8_t *data, uint16_t len)
{
  if (len > sizeof(anki_vehicle_msg_t)) {
    error("Invalid vehicle response\n");
    return;
  }

  const anki_vehicle_msg_t *msg = (const anki_vehicle_msg_t *)data;
  switch(msg->msg_id) {
  case ANKI_VEHICLE_MSG_V2C_LOCALIZATION_POSITION_UPDATE:
    {
      const anki_vehicle_msg_localization_position_update_t *m = (const anki_vehicle_msg_localization_position_update_t *)msg;

      // printf("LOCALE_UPDATE: localisationID: %02x pieceID: %02x\n", m->_reserved[0],m->_reserved[1]);
      loc.update_time++;
      loc.segm=m->_reserved[0];
      loc.subsegm=m->_reserved[1];
      loc.is_clockwise=m->is_clockwise;
      break;
    }
  default:
    // printf("Received unhandled vehicle message of type 0x%02x\n", msg->msg_id);
    break;
  }
}

static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data)
{
  uint16_t handle = att_get_u16(&pdu[1]);

  if (pdu[0] == ATT_OP_HANDLE_NOTIFY) {
    uint16_t handle = att_get_u16(&pdu[1]);
    if (handle != vehicle.read_char.value_handle) {
      error("Invalid vehicle read handle: 0x%04x\n", handle);
      return;
    }
    const uint8_t *data = &pdu[3];
    const uint16_t datalen = len-3;

    handle_vehicle_msg_response(data, datalen);
    return;
  }
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
  if (err) {
    set_state(STATE_DISCONNECTED);
    error("%s\n", err->message);
    return;
  }

  attrib = g_attrib_new(iochannel);
  g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
		    events_handler, attrib, NULL);
  g_attrib_register(attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
		    events_handler, attrib, NULL);
  set_state(STATE_CONNECTED);
  if(verbose) printf("Connection successful\n");

  discover_services();
}

static void disconnect_io()
{
  if (conn_state == STATE_DISCONNECTED)
    return;

  g_attrib_unref(attrib);
  attrib = NULL;
  opt_mtu = 0;

  g_io_channel_shutdown(iochannel, FALSE, NULL);
  g_io_channel_unref(iochannel);
  iochannel = NULL;

  set_state(STATE_DISCONNECTED);
}

static void discover_char_cb(guint8 status, GSList *characteristics, gpointer user_data)
{
  GSList *l;

  if (status) {
    error("Discover all characteristics failed: %s\n",
	  att_ecode2str(status));
    return;
  }

  for (l = characteristics; l; l = l->next) {
    struct gatt_char *chars = l->data;

    if (strncasecmp(chars->uuid, ANKI_STR_CHR_READ_UUID, strlen(ANKI_STR_CHR_READ_UUID)) == 0) {
      memmove(&(vehicle.read_char), chars, sizeof(struct gatt_char));
      if(verbose) { printf("Anki Read Characteristic: %s ", chars->uuid);
	printf("[handle: 0x%04x, char properties: 0x%02x, char value "
	       "handle: 0x%04x]\n", chars->handle,
	       chars->properties, chars->value_handle);
      }
    }

    if (strncasecmp(chars->uuid, ANKI_STR_CHR_WRITE_UUID, strlen(ANKI_STR_CHR_WRITE_UUID)) == 0) {
      memmove(&(vehicle.write_char), chars, sizeof(struct gatt_char));
      if(verbose) { printf("Anki Write Characteristic: %s ", chars->uuid);
	printf("[handle: 0x%04x, char properties: 0x%02x, char value "
	       "handle: 0x%04x]\n", chars->handle,
	       chars->properties, chars->value_handle);
      }
    }
  }

  if (vehicle.read_char.handle > 0 && vehicle.write_char.handle > 0) {
    // register for notifications when the vehicle sends data.
    // We do this by setting the notification bit on the
    // client configuration characteristic:
    // see:
    // https://developer.bluetooth.org/gatt/descriptors/Pages/DescriptorViewer.aspx?u=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    uint8_t notify_cmd[] = { 0x01, 0x00 };
    gatt_write_cmd(attrib, vehicle.write_char.properties, notify_cmd,  2, NULL, NULL);
  }
}

static void char_read_cb(guint8 status, const guint8 *pdu, guint16 plen,
			 gpointer user_data)
{
  if (status != 0) {
    error("Characteristic value/descriptor read failed: %s\n",
	  att_ecode2str(status));
    return;
  }

  uint8_t value[plen];
  ssize_t vlen = dec_read_resp(pdu, plen, value, sizeof(value));
  if (vlen < 0) {
    error("Protocol error\n");
    return;
  }

  handle_vehicle_msg_response(value, vlen);
}

static gboolean channel_watcher(GIOChannel *chan, GIOCondition cond,
				gpointer user_data)
{
  disconnect_io();
  return FALSE;
}


// Discover Services
static void discover_services_cb(guint8 status, GSList *ranges, gpointer user_data)
{
  GSList *l;

  if (status) {
    error("Discover primary services by UUID failed: %s\n",
	  att_ecode2str(status));
    return;
  }

  if (ranges == NULL) {
    error("No service UUID found\n");
    return;
  }

  for (l = ranges; l; l = l->next) {
    struct att_range *range = l->data;
    if(verbose) 
      printf("Starting handle: 0x%04x Ending handle: 0x%04x\n", range->start, range->end);
  }
  gatt_discover_char(attrib, 0x1, 0xffff, NULL, discover_char_cb, NULL);
}


static void discover_services(void)
{
  if (conn_state != STATE_CONNECTED) {
    failed("Disconnected\n");
    return;
  }

  bt_uuid_t uuid;
  if (bt_string_to_uuid(&uuid, ANKI_STR_SERVICE_UUID) < 0) {
    error("Error attempting to discover service for UUID: %s\n", ANKI_STR_SERVICE_UUID);
    return;
  }

  gatt_discover_primary(attrib, &uuid, discover_services_cb, NULL);
}

static int strtohandle(const char *src)
{
  char *e;
  int dst;

  errno = 0;
  dst = strtoll(src, &e, 16);
  if (errno != 0 || *e != '\0')
    return -EINVAL;

  return dst;
}

static void anki_s_intern_disconnect()
{
  if (conn_state != STATE_CONNECTED) {
    failed("Disconnected\n");
    return;
  }
  int handle = vehicle.write_char.value_handle;
  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_disconnect(&msg);
  gatt_write_char(attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
}

static int anki_s_intern_sdk_mode(int mode)
{
  if (conn_state != STATE_CONNECTED) {
    failed("Disconnected\n");
    return 0;
  }
  int handle = vehicle.write_char.value_handle;
  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_set_sdk_mode(&msg, mode, ANKI_VEHICLE_SDK_OPTION_OVERRIDE_LOCALIZATION);
  gatt_write_char(attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
  return 1;
}

int anki_s_uturn()
{
  if (conn_state != STATE_CONNECTED) {
    failed("Disconnected\n");
    return 0;
  }
  int handle = vehicle.write_char.value_handle;
  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_turn_180(&msg);
  gatt_write_char(attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
  return 1;
}


static int anki_s_intern_get_localization_position_update()
{
  if (conn_state != STATE_CONNECTED) {
    failed("Disconnected\n");
    return 0;
  }
  int handle = vehicle.write_char.value_handle;

  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_get_localization_position_update(&msg);
  gatt_write_char(attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
  return 1;
}

int anki_s_set_speed(int speed, int accel)
{
  if (conn_state != STATE_CONNECTED) {
    failed("Disconnected\n");
    return 0;
  }
  int handle = vehicle.write_char.value_handle;

  if(verbose) printf("setting speed to %d (accel = %d)\n", speed, accel);

  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_set_speed(&msg, speed, accel);
  gatt_write_char(attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
  return 1;
}

int anki_s_change_lane(int relative_offset, int h_speed, int h_accel)
{
  if (conn_state != STATE_CONNECTED) {
    failed("Disconnected\n");
    return 0;
  }
  int handle = vehicle.write_char.value_handle;
  float offset = relative_offset;

  if(verbose) printf("changing lane at %d (accel = %d | offset = %1.2f)\n", h_speed, h_accel, offset);

  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_set_offset_from_road_center(&msg, 0.0);
  gatt_write_char(attrib, handle, (uint8_t*)&msg, plen, NULL, NULL);

  anki_vehicle_msg_t lane_msg;
  size_t lane_plen = anki_vehicle_msg_change_lane(&lane_msg, h_speed, h_accel, offset);
  gatt_write_char(attrib, handle, (uint8_t*)&lane_msg, lane_plen, NULL, NULL);
  return 1;
}

localization_t anki_s_get_localization(){
  return loc;
}

void *event_loop_thread(gpointer data) {
  event_loop = g_main_loop_new(NULL, FALSE);
  // put into extra thread
  g_main_loop_run(event_loop);
}

int anki_s_init(const char *src, const char *dst, int _verbose){
  if (conn_state != STATE_DISCONNECTED)
    return 0;

  verbose = _verbose;
  if (dst == NULL) {
    error("Remote Bluetooth address required\n");
    return 0;
  }


  loc.segm=0;
  loc.subsegm=0;
  loc.is_clockwise=0;
  loc.update_time=0;

  opt_src = g_strdup(src);
  opt_dst = g_strdup(dst);
  opt_dst_type = g_strdup("random");
  opt_sec_level = g_strdup("low");

  g_thread = g_thread_new("eventloopthread",(GThreadFunc)event_loop_thread,NULL);

  GError *gerr = NULL;

  if(verbose) printf("Attempting to connect to %s\n", opt_dst);
  set_state(STATE_CONNECTING);
  iochannel = gatt_connect(opt_src, opt_dst, opt_dst_type, opt_sec_level,
                           opt_psm, opt_mtu, connect_cb, &gerr);
  if (iochannel == NULL) {
    set_state(STATE_DISCONNECTED);
    error("%s\n", gerr->message);
    g_error_free(gerr);
    return 0;
  } else
    g_io_add_watch(iochannel, G_IO_HUP, channel_watcher, NULL);

  g_usleep(G_USEC_PER_SEC);
  // set sdk mode
  anki_s_intern_sdk_mode(1);

  // enable localization update
  anki_s_intern_get_localization_position_update();

  return 1;
}

void anki_s_close(){
  anki_s_intern_disconnect();
  g_usleep(50000);
  g_main_loop_quit(event_loop);
  g_thread_join(g_thread);
  g_thread_unref(g_thread);
  g_main_loop_unref(event_loop);
  g_free(opt_src);
  g_free(opt_dst_type);
  g_free(opt_sec_level);
}
