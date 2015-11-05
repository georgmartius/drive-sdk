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

typedef struct anki_vehicle {
  struct gatt_char read_char;
  struct gatt_char write_char;
} anki_vehicle_t;

typedef struct handle {
  GIOChannel *iochannel;
  GAttrib    *attrib;
  GMainLoop  *event_loop;
  GThread*    g_thread;

  anki_vehicle_t vehicle;
  localization_t loc;

  enum state {
    STATE_DISCONNECTED,
    STATE_CONNECTING,
    STATE_CONNECTED
  } conn_state;
  int is_ready;
  int verbose;
} handle_t;

static void discover_services(handle_t* h);

#define error(fmt, arg...)                      \
  fprintf(stderr, "Error: " fmt, ## arg)

#define failed(fmt, arg...)				\
  fprintf(stderr, "Command Failed: " fmt, ## arg)

#define set_state(state) h->conn_state = st;

static int check_connected(handle_t* h){
  int status= (h && h->conn_state == STATE_CONNECTED);
  if(!status){ failed("Disconnected\n"); }
  return status;
}

static void handle_vehicle_msg_response(handle_t* h, const uint8_t *data, uint16_t len)
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
      h->loc.update_time++;
      h->loc.segm=m->_reserved[1];
      h->loc.subsegm=m->_reserved[0];
      h->loc.is_clockwise=m->is_clockwise;
      h->loc.num_uncounted_transitions=0;
      h->loc.is_delocalized=0;
      break;
    }
  case ANKI_VEHICLE_MSG_V2C_LOCALIZATION_TRANSITION_UPDATE:
    {
      const anki_vehicle_msg_localization_transition_update_t *m = (const anki_vehicle_msg_localization_transition_update_t *)msg;
      h->loc.num_uncounted_transitions++;
      h->loc.is_delocalized=0;
      break;
    }
  case ANKI_VEHICLE_MSG_V2C_IS_READY:
    if(h->verbose) printf("Car READY\n");
    h->is_ready=1;
    break;
  case ANKI_VEHICLE_MSG_V2C_VEHICLE_DELOCALIZED:
    h->loc.update_time++;
    h->loc.segm=0;
    h->loc.subsegm=0;
    h->loc.is_delocalized=1;
    break;
  default:
    // printf("Received unhandled vehicle message of type 0x%02x\n", msg->msg_id);
    break;
  }
}

static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data)
{
  handle_t* h = (handle_t*)user_data;
  uint16_t handle = att_get_u16(&pdu[1]);

  if (pdu[0] == ATT_OP_HANDLE_NOTIFY) {
    uint16_t handle = att_get_u16(&pdu[1]);
    if (handle != h->vehicle.read_char.value_handle) {
      error("Invalid vehicle read handle: 0x%04x\n", handle);
      return;
    }
    const uint8_t *data = &pdu[3];
    const uint16_t datalen = len-3;

    handle_vehicle_msg_response(h, data, datalen);
    return;
  }
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
  handle_t* h = (handle_t*)user_data;
  if (err) {
    h->conn_state=STATE_DISCONNECTED;
    error("%s\n", err->message);
    return;
  }

  h->attrib = g_attrib_new(h->iochannel);
  g_attrib_register(h->attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
                    events_handler, h, NULL);
  g_attrib_register(h->attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
                    events_handler, h, NULL);
  h->conn_state=STATE_CONNECTED;
  if(h->verbose) printf("Connection successful, waiting for vehicle to become ready\n");

  discover_services(h);
}

static void disconnect_io(handle_t* h)
{
  if (!h || h->conn_state == STATE_DISCONNECTED)
    return;

  g_attrib_unref(h->attrib);
  h->attrib = NULL;

  g_io_channel_shutdown(h->iochannel, FALSE, NULL);
  g_io_channel_unref(h->iochannel);
  h->iochannel = NULL;

  h->conn_state=STATE_DISCONNECTED;
}

static void discover_char_cb(guint8 status, GSList *characteristics, gpointer user_data)
{
  GSList *l;
  handle_t* h = (handle_t*)user_data;

  if (status) {
    error("Discover all characteristics failed: %s\n",
          att_ecode2str(status));
    return;
  }

  for (l = characteristics; l; l = l->next) {
    struct gatt_char *chars = l->data;

    if (strncasecmp(chars->uuid, ANKI_STR_CHR_READ_UUID, strlen(ANKI_STR_CHR_READ_UUID)) == 0) {
      memmove(&(h->vehicle.read_char), chars, sizeof(struct gatt_char));
      if(h->verbose) { printf("Anki Read Characteristic: %s ", chars->uuid);
	printf("[handle: 0x%04x, char properties: 0x%02x, char value "
	       "handle: 0x%04x]\n", chars->handle,
	       chars->properties, chars->value_handle);
      }
    }

    if (strncasecmp(chars->uuid, ANKI_STR_CHR_WRITE_UUID, strlen(ANKI_STR_CHR_WRITE_UUID)) == 0) {
      memmove(&(h->vehicle.write_char), chars, sizeof(struct gatt_char));
      if(h->verbose) { printf("Anki Write Characteristic: %s ", chars->uuid);
	printf("[handle: 0x%04x, char properties: 0x%02x, char value "
	       "handle: 0x%04x]\n", chars->handle,
	       chars->properties, chars->value_handle);
      }
    }
  }

  if (h->vehicle.read_char.handle > 0 && h->vehicle.write_char.handle > 0) {
    // register for notifications when the vehicle sends data.
    // We do this by setting the notification bit on the
    // client configuration characteristic:
    // see:
    // https://developer.bluetooth.org/gatt/descriptors/Pages/DescriptorViewer.aspx?u=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    uint8_t notify_cmd[] = { 0x01, 0x00 };
    gatt_write_cmd(h->attrib, h->vehicle.write_char.properties, notify_cmd,  2, NULL, NULL);
  }
}

static gboolean channel_watcher(GIOChannel *chan, GIOCondition cond,
                                gpointer user_data)
{
  disconnect_io((handle_t*)user_data);
  return FALSE;
}


// Discover Services
static void discover_services_cb(guint8 status, GSList *ranges, gpointer user_data)
{
  GSList *l;
  handle_t* h = (handle_t*)user_data;

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
    if(h->verbose)
      printf("Starting handle: 0x%04x Ending handle: 0x%04x\n", range->start, range->end);
  }
  gatt_discover_char(h->attrib, 0x1, 0xffff, NULL, discover_char_cb, h);
}


static void discover_services(handle_t* h)
{
  if (!check_connected(h)) return;

  bt_uuid_t uuid;
  if (bt_string_to_uuid(&uuid, ANKI_STR_SERVICE_UUID) < 0) {
    error("Error attempting to discover service for UUID: %s\n", ANKI_STR_SERVICE_UUID);
    return;
  }

  gatt_discover_primary(h->attrib, &uuid, discover_services_cb, h);
}

static void disconnect(handle_t* h)
{
  if (!check_connected(h)) return;

  int handle = h->vehicle.write_char.value_handle;
  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_disconnect(&msg);
  gatt_write_char(h->attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
}

static int sdk_mode(handle_t* h,int mode)
{
  if (!check_connected(h))  return 0;

  int handle = h->vehicle.write_char.value_handle;
  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_set_sdk_mode(&msg, mode, ANKI_VEHICLE_SDK_OPTION_OVERRIDE_LOCALIZATION);
  gatt_write_char(h->attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
  return 1;
}


static void *event_loop_thread(gpointer data) {
  handle_t* h = (handle_t*)data;
  h->event_loop = g_main_loop_new(NULL, FALSE);
  // put into extra thread
  g_main_loop_run(h->event_loop);
}

//----------- EXPORTED FUNCTIONS START

int anki_s_is_connected(AnkiHandle ankihandle){
  handle_t* h = (handle_t*)ankihandle;
  return (h && h->conn_state == STATE_CONNECTED);
}

int anki_s_uturn(AnkiHandle ankihandle)
{
  handle_t* h = (handle_t*)ankihandle;

  if (!check_connected(h))  return 1;

  int handle = h->vehicle.write_char.value_handle;
  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_turn_180(&msg);
  gatt_write_char(h->attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
  return 0;
}


int anki_s_set_speed(AnkiHandle ankihandle, int speed, int accel)
{
  handle_t* h = (handle_t*)ankihandle;
  if (!check_connected(h))  return 1;

  if(h->verbose) printf("setting speed to %d (accel = %d)\n", speed, accel);

  int handle = h->vehicle.write_char.value_handle;
  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_set_speed(&msg, speed, accel);
  gatt_write_char(h->attrib, handle, (uint8_t *)&msg, plen, NULL, NULL);
  return 0;
}

int anki_s_change_lane(AnkiHandle ankihandle, int relative_offset, int h_speed, int h_accel)
{
  handle_t* h = (handle_t*)ankihandle;
  if (!check_connected(h))  return 1;

  int handle = h->vehicle.write_char.value_handle;
  float offset = relative_offset;

  if(h->verbose) printf("changing lane at %d (accel = %d | offset = %1.2f)\n", h_speed, h_accel, offset);

  anki_vehicle_msg_t msg;
  size_t plen = anki_vehicle_msg_set_offset_from_road_center(&msg, 0.0);
  gatt_write_char(h->attrib, handle, (uint8_t*)&msg, plen, NULL, NULL);

  anki_vehicle_msg_t lane_msg;
  size_t lane_plen = anki_vehicle_msg_change_lane(&lane_msg, h_speed, h_accel, offset);
  gatt_write_char(h->attrib, handle, (uint8_t*)&lane_msg, lane_plen, NULL, NULL);
  return 0;
}

int anki_s_cancel_lane_change(AnkiHandle ankihandle)
{
  handle_t* h = (handle_t*)ankihandle;
  if (!check_connected(h))  return 1;

  int handle = h->vehicle.write_char.value_handle;
  if(h->verbose) printf("cancel changing lane\n");

  anki_vehicle_msg_t clane_msg;
  size_t clane_plen = anki_vehicle_msg_cancel_lane_change(&clane_msg);
  gatt_write_char(h->attrib, handle, (uint8_t*)&clane_msg, clane_plen, NULL, NULL);
  return 0;
}

localization_t anki_s_get_localization(AnkiHandle ankihandle){
  handle_t* h = (handle_t*)ankihandle;
  if (!check_connected(h)) {
    localization_t l;
    l.segm=-1;
    l.update_time=-1;
    return l;
  }
  return h->loc;
}

AnkiHandle anki_s_init(const char *src, const char *dst, int _verbose){
  handle_t* h = (handle_t*)malloc(sizeof(handle_t));
  h->iochannel        = NULL;
  h->attrib           = NULL;
  h->verbose          = _verbose;
  h->loc.segm         = 0;
  h->loc.subsegm      = 0;
  h->loc.is_clockwise = 0;
  h->loc.update_time  = 0;
  h->loc.num_uncounted_transitions = 0;
  h->conn_state       = STATE_DISCONNECTED;
  h->is_ready         = 0;
  if (dst == NULL) {
    error("Remote Bluetooth address required\n");
    return NULL;
  }

  h->g_thread = g_thread_new("eventloopthread",(GThreadFunc)event_loop_thread, h);

  GError *gerr = NULL;
  int psm      = 0;
  int mtu      = 0;

  if(h->verbose) printf("Attempting to connect to %s\n", dst);

  h->iochannel = gatt_connect(src, dst, "random", "low", psm, mtu, connect_cb, &gerr, h);
  if (h->iochannel == NULL) {
    h->conn_state=STATE_DISCONNECTED;
    error("%s\n", gerr->message);
    g_error_free(gerr);
    return NULL;
  } else
    g_io_add_watch(h->iochannel, G_IO_HUP, channel_watcher, h);

  // check for connected callback and car beeing ready
  int max_wait=50;
  while(h->is_ready == 0 && max_wait>0){
    g_usleep(G_USEC_PER_SEC/10);
    max_wait--;
  }
  if(h->is_ready == 0) {
    error("could not connect to car\n");
    return NULL;
  }

  // set sdk mode
  sdk_mode(h,1);

  return h;
}

void anki_s_close(AnkiHandle ankihandle){
  handle_t* h = (handle_t*)ankihandle;
  if (!h) return;
  disconnect(h);
  g_usleep(1000000);
  g_main_loop_quit(h->event_loop);
  g_usleep(50000);
  g_thread_join(h->g_thread);
  g_main_loop_unref(h->event_loop);
  free(h);
}
