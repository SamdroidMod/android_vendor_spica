/***********************************************************************

   Copyright 2009 Broadcom Corporation

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2, as published by
   the Free Software Foundation (the "GPL").

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
   writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA  02111-1307, USA.


************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stdint.h>
#include <dirent.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/select.h>
#include <sys/poll.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <gdbus.h>
#include <gmain.h>

#include "hcid.h"
#include "textfile.h"
#include "manager.h"
#include "adapter.h"
#include "device.h"
#include "error.h"
#include "glib-helper.h"
#include "dbus-common.h"
#include "agent.h"
#include "dbus-hci.h"

#include "textfile.h"
#include "error.h"
#include "dtun_device.h"

#include "dtun_clnt.h"

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

extern void dtun_pin_reply( tDTUN_ID id,  pin_code_reply_cp *pr);
extern void dtun_ssp_confirm_reply(bdaddr_t *dba, boolean confirm);
extern DBusHandlerResult error_failed(DBusConnection *conn,
                                        DBusMessage *msg, const char * desc);

extern void dtun_sig_opc_enable(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_opc_open(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_opc_progress(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_opc_object_received(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_opc_object_pushed(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_opc_close(tDTUN_DEVICE_SIGNAL *p_data);

extern void dtun_sig_ops_progress(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_ops_object_received(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_ops_open(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_ops_access_request(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_ops_close(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_op_create_vcard(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_op_owner_vcard_not_set(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_op_store_vcard(tDTUN_DEVICE_SIGNAL *p_data);
extern void dtun_sig_op_set_default_path(tDTUN_DEVICE_SIGNAL *p_data);

typedef enum {
	AVDTP_STATE_IDLE,
	AVDTP_STATE_CONFIGURED,
	AVDTP_STATE_OPEN,
	AVDTP_STATE_STREAMING,
	AVDTP_STATE_CLOSING,
	AVDTP_STATE_ABORTING,
} avdtp_state_t;


struct pending_request {
	DBusConnection *conn;
	DBusMessage *msg;
	unsigned int id;
};

struct sink {
        struct audio_device *dev;
//	bdaddr_t peer_addr;
	unsigned int cb_id;
	avdtp_state_t state;
	struct pending_request *connect;
	struct pending_request *disconnect;
	DBusConnection *conn;
};

struct pending_get_scn {
	DBusConnection *conn;
	DBusMessage *msg;
	uint16_t uuid16;
};
static GMainLoop *event_loop;
static struct btd_adapter *adapter;
DBusConnection *connection;


#define FAKE_PATH		"/org/bluez/hci0"

DBusConnection*sig_connection = NULL;

DBusMessage *dtun_am_creat_dev_msg = NULL;
struct audio_device *dtun_saved_pdev = NULL;
struct pending_get_scn *dtun_pending_get_scn = NULL;

unsigned char dtun_pending_get_services_flg = 0;
bdaddr_t dtun_pending_get_services_adr;


static    bdaddr_t sba = { {0x11, 0x22, 0x33, 0x44, 0x55, 0x66} };

// KJH 20091206 :: remote device name error fix (milo)
static int char_len;

extern void pin_code_request(int dev, bdaddr_t *sba, bdaddr_t *dba);
extern void link_key_notify(int dev, bdaddr_t *sba, void *ptr);

struct sink *sink_init(struct audio_device *dev);


#define PRINTFUNC() debug("\t\t%s()\n", __FUNCTION__);

#define AUDIO_MANAGER_PATH "/org/bluez/audio"
#define AUDIO_MANAGER_INTERFACE "org.bluez.audio.Manager"
#define AUDIO_SINK_INTERFACE "org.bluez.AudioSink"

#define BLUETOOTH_SPP_PATH "/org/bluez/Serial"
#define BLUETOOTH_SPP_INTERFACE "org.bluez.Serial.port"

static void register_devices_stored(const char *adapter);
extern struct audio_device *device_register(DBusConnection *conn,
					const char *path, const bdaddr_t *bda, const bdaddr_t *src);


static struct audio_device *default_hs = NULL;
static struct audio_device *default_dev = NULL;

static GSList *devices = NULL;

#define GENERIC_AUDIO_UUID	"00001203-0000-1000-8000-00805F9B34FB"

#define HSP_HS_UUID		"00001108-0000-1000-8000-00805F9B34FB"
#define HSP_AG_UUID		"00001112-0000-1000-8000-00805F9B34FB"

#define HFP_HS_UUID		"0000111E-0000-1000-8000-00805F9B34FB"
#define HFP_AG_UUID		"0000111F-0000-1000-8000-00805F9B34FB"

#define ADVANCED_AUDIO_UUID	"0000110D-0000-1000-8000-00805F9B34FB"

#define A2DP_SOURCE_UUID	"0000110A-0000-1000-8000-00805F9B34FB"
#define A2DP_SINK_UUID		"0000110B-0000-1000-8000-00805F9B34FB"

#define AVRCP_REMOTE_UUID	"0000110E-0000-1000-8000-00805F9B34FB"
#define AVRCP_TARGET_UUID	"0000110C-0000-1000-8000-00805F9B34FB"

#define HID_SERVICE_UUID "00001124-0000-1000-8000-00805f9b34fb"

#define PANU_UUID	"00001115-0000-1000-8000-00805f9b34fb"
#define NAP_UUID	"00001116-0000-1000-8000-00805f9b34fb"
#define GN_UUID		"00001117-0000-1000-8000-00805f9b34fb"

#define SERIAL_PORT_UUID	"00001101-0000-1000-8000-00805F9B34FB"
#define DIALUP_NET_UUID		"00001103-0000-1000-8000-00805F9B34FB"

#define SYNC_UUID "00001104-0000-1000-8000-00805F9B34FB"
#define OPP_UUID "00001105-0000-1000-8000-00805F9B34FB"

#define FTP_UUID "00001106-0000-1000-8000-00805F9B34FB"

#define CTP_UUID "00001109-0000-1000-8000-00805F9B34FB"
#define ICP_UUID "00001110-0000-1000-8000-00805F9B34FB"

#define BPP_UUID "00001122-0000-1000-8000-00805F9B34FB"

#define FAX_UUID "00001111-0000-1000-8000-00805F9B34FB"
#define LAP_UUID "00001102-0000-1000-8000-00805F9B34FB"

#define BIP_UUID "0000111A-0000-1000-8000-00805F9B34FB"
#define PBAP_UUID "00001130-0000-1000-8000-00805F9B34FB"

#define VIDEO_DIST_UUID "00001305-0000-1000-8000-00805F9B34FB"
#define SIM_ACC_UUID "0000112D-0000-1000-8000-00805F9B34FB"

#define COD_SERVICE_MASK                     0xFFE000
#define COD_SERVICE_LIMITED_DISCOVERABILITY  0x002000
#define COD_SERVICE_POSITIONING              0x010000
#define COD_SERVICE_NETWORKING               0x020000
#define COD_SERVICE_RENDER                   0x040000
#define COD_SERVICE_CAPTURE                  0x080000
#define COD_SERVICE_OBJECT_TRANSFER          0x100000
#define COD_SERVICE_AUDIO                    0x200000
#define COD_SERVICE_TELEPHONY                0x400000
#define COD_SERVICE_INFORMATION              0x800000

#define COD_MAJOR_CLS_MASK               0x1F00

#define COD_MAJOR_CLS_MISC               0x0000
#define COD_MAJOR_CLS_COMPUTER           0x0100
#define COD_MAJOR_CLS_PHONE              0x0200
#define COD_MAJOR_CLS_NETWORKING         0x0300
#define COD_MAJOR_CLS_AUDIO_VIDEO        0x0400
#define COD_MAJOR_CLS_PERIPHERAL         0x0500
#define COD_MAJOR_CLS_IMAGING            0x0600
#define COD_MAJOR_CLS_WEARABLE           0x0700
#define COD_MAJOR_CLS_TOY                0x0800
#define COD_MAJOR_CLS_HEALTH             0x0900
#define COD_MAJOR_CLS_UNCATEGORIZED      0x1F00

#define COD_AUDIO_VIDEO_HIFI_AUDIO       0x0428
#define COD_AUDIO_VIDEO_HEADPHONES       0x0418
#define COD_AUDIO_VIDEO_LOUDSPEAKER      0x0414
#define COD_AUDIO_VIDEO_CAR_AUDIO        0x0420

const char *dtunc_uuid_table[] =
{
	NULL,
	SERIAL_PORT_UUID,
	DIALUP_NET_UUID,
	FAX_UUID,           /* Fax profile. */
	LAP_UUID,          /* LAN access profile. */
	HSP_HS_UUID,          /* Headset profile. */
	HFP_HS_UUID,          /* Hands-free profile. */
	OPP_UUID,          /* Object push  */
	FTP_UUID,          /* File transfer */
	CTP_UUID,           /* Cordless Terminal */
	ICP_UUID,          /* Intercom Terminal */
	SYNC_UUID,          /* Synchronization */
	BPP_UUID,          /* Basic printing profile */
	BIP_UUID,          /* Basic Imaging profile */
	PANU_UUID,          /* PAN User */
	NAP_UUID,          /* PAN Network access point */
	GN_UUID,         /* PAN Group Ad-hoc networks */
	SIM_ACC_UUID,          /* SIM Access profile */
	A2DP_SINK_UUID,          /* Advanced audio distribution */
	AVRCP_TARGET_UUID,          /* A/V remote control */
	HID_SERVICE_UUID,          /* HID */
	VIDEO_DIST_UUID,          /* Video distribution */
	PBAP_UUID,          /* PhoneBook Access */
	NULL,          /* HFP HS role */
	NULL          /* HSP HS role */
};

#define DTUN_NUM_UUIDS_IN_TABLE  25

const char *pbap_uuids[] = { PBAP_UUID, NULL };
const char *opp_uuids[] = { OPP_UUID, NULL };

boolean dtun_auth_on = false;
tDTUN_SIG_DM_AUTHORIZE_REQ_INFO dtun_cur_authorize_req_info;

#define MAX_EXPOSED_SDP_HANDLES 32
#define FREE_EXPOSED_HANDLE 0xFFFFFFFF
#define ASSIGNED_EXPOSED_HANDLE 0xFFFFFFFE

uint32_t sdp_handles[MAX_EXPOSED_SDP_HANDLES]; //SDP handles

static const char *state2str(avdtp_state_t state)
{
        switch (state) {
        case AVDTP_STATE_IDLE:
                return "disconnected";
        case AVDTP_STATE_CONFIGURED:
                return "connecting";
        case AVDTP_STATE_OPEN:
                return "connected";
        case AVDTP_STATE_STREAMING:
                return "playing";
        default:
                error("Invalid sink state %d", state);
                return NULL;
        }
};
/*******************************************************************************
**
** Function          DTUN DM CLIENT TEST  (DBUS SERVER SIDE)
**
*/

void dtun_add_devices_do_append(char *key, char *value, void *data)
{
    tDTUN_DEVICE_METHOD method;
    char tmp[3], *str = value;
    int i;

    PRINTFUNC();

    info( "key = %s value = %s" , key, value );

    str2ba(key, &method.add_dev.info.bd_addr);

    memset(tmp, 0, sizeof(tmp));
    for (i = 0; i < 16; i++) {
        memcpy(tmp, str + (i * 2), 2);
        method.add_dev.info.key[i] = (uint8_t) strtol(tmp, NULL, 16);
    }

    memcpy(tmp, str + 33, 2);
    method.add_dev.info.key_type = (uint8_t) strtol(tmp, NULL, 10);

    method.add_dev.hdr.id = DTUN_METHOD_DM_ADD_DEV;
    method.add_dev.hdr.len = sizeof(tDTUN_METHOD_DM_ADD_DEV_INFO);
    dtun_client_call_method(&method);

}

void dtun_init_device_uuid( struct btd_device *device, char *str_uuid )
{
    struct audio_device *audio_dev;

    info( "Adding uuid %s", str_uuid ) ;
	
    if (strcmp( str_uuid, A2DP_SINK_UUID ) == 0 )
    {
        info( "Calling sink_init" ) ;
    
        audio_dev = manager_get_device(sig_connection, &sba, device_get_bdaddr(device),TRUE);

        // + KJH 20091223 :: bluetoothd crash fix (IMS 280640, 280644) (milo)
        //milo [CSP280640] : bluetoothd crash occurs because of accessing null pointer //
        if(!audio_dev) {
          error("couldn't get device pointer, just return");
          return;
        }//

        audio_dev->sink = sink_init(audio_dev);
    }

}


int dtunops_setup (void)
{
    manager_register_adapter( 0, TRUE );

    adapter = manager_find_adapter_by_id(0);
    
    if (!adapter) {
        error("Getting device data failed: hci0");
        return -1;
    }

    info( "manager start adapter" );
    adapter_set_bdaddr(adapter, &sba);
    manager_start_adapter(0);

    obex_dbus_init ( sig_connection );	

//TODO:	adapter_set_class(adapter, cls);	
//TODO:	adapter_update_ssp_mode(adapter, mode);
    return 0;
}

void dtunops_cleanup (void)
{
    return;
}

int dtunops_start (int index)
{
    return 0;
}

int dtunops_stop (int index)
{
    return 0;
}
			
int dtunops_set_powered  (int index, gboolean powered)
{
    error( "...dtunops_set_powered unimplemented!!!" );

    return 0;
}

int dtunops_set_connectable (int index)
{
    tDTUN_DEVICE_METHOD method;

    method.set_mode.hdr.id = DTUN_METHOD_DM_SET_MODE;
    method.set_mode.hdr.len = 1; // no payload
    method.set_mode.mode  =  MODE_CONNECTABLE;
	   
    dtun_client_call_method(&method);

    adapter_mode_changed(adapter, SCAN_PAGE);

    return 0;
}

int dtunops_set_discoverable (int index)
{
    tDTUN_DEVICE_METHOD method;

    method.set_mode.hdr.id = DTUN_METHOD_DM_SET_MODE;
    method.set_mode.hdr.len = 1; // no payload
    method.set_mode.mode  =  MODE_DISCOVERABLE;
	   
    dtun_client_call_method(&method);

    adapter_mode_changed(adapter, SCAN_PAGE|SCAN_INQUIRY);

    return 0;
}		

int dtunops_set_limited_discoverable (int index, const uint8_t *cls,
						gboolean limited)
{
     error( "...dtunops_set_limited_discoverable unimplemented!!!" );
     return 0;
}

int dtunops_start_discovery (int index, gboolean periodic)
{
    dtun_client_call_id_only(DTUN_METHOD_DM_START_DISCOVERY);
    return 0;
}

int dtunops_stop_discovery (int index)
{
    dtun_client_call_id_only(DTUN_METHOD_DM_CANCEL_DISCOVERY);
    return 0;
}

int dtunops_resolve_name (int index, bdaddr_t *bdaddr)
{
    error( "...dtunops_resolve_name unimplemented!!!" );
    return 0;
}

int dtunops_cancel_resolve_name (int index, bdaddr_t *bdaddr)
{
    error( "...dtunops_cancel_resolve_name unimplemented!!!" );
    return 0;
}


int dtunops_set_name (int index, const char *name)
{
    tDTUN_DEVICE_METHOD method;
    read_local_name_rp rp;

    method.set_name.hdr.id = DTUN_METHOD_DM_SET_NAME;
    method.set_name.hdr.len = DTUN_MAX_DEV_NAME_LEN;
    strncpy( method.set_name.name, name, DTUN_MAX_DEV_NAME_LEN);

    dtun_client_call_method(&method);

    rp.status = 0;
    memcpy( &rp.name, name, DTUN_MAX_DEV_NAME_LEN );
    rp.name[DTUN_MAX_DEV_NAME_LEN-1] = 0;
	
    adapter_update_local_name(&sba, 0, &rp);

    return 0;
}

int dtunops_read_name (int index)
{
    error( "...dtunops_read_name unimplemented!!!" );
    return 0;
}

static struct btd_adapter_ops dtun_ops = {
	.setup = dtunops_setup,
	.cleanup = dtunops_cleanup,
	.start = dtunops_start,
	.stop = dtunops_stop,
	.set_powered = dtunops_set_powered,
	.set_connectable = dtunops_set_connectable,
	.set_discoverable = dtunops_set_discoverable,
	.set_limited_discoverable = dtunops_set_limited_discoverable,
	.start_discovery = dtunops_start_discovery,
	.stop_discovery = dtunops_stop_discovery,
	.resolve_name = dtunops_resolve_name,
	.cancel_resolve_name = dtunops_cancel_resolve_name,
	.set_name = dtunops_set_name,
	.read_name = dtunops_read_name,
};

static int dtunops_init(void)
{
	return btd_register_adapter_ops(&dtun_ops);
}
static void dtunops_exit(void)
{
	btd_adapter_cleanup_ops(&dtun_ops);
}


void dtun_dm_sig_local_info(tDTUN_DEVICE_SIGNAL *msg)
{
    tDTUN_DEVICE_METHOD method;
    char str[DTUN_MAX_DEV_NAME_LEN];
    char *str_ptr = str;
    int retval;
    read_local_name_rp rp;
    int i;

    PRINTFUNC();

    // - KJH
    //error( "Just trying to add an error log" );
	
    memcpy( &sba, msg->local_info.bdaddr.b, 6 );

    retval = read_local_name(&sba, str);

    /* the Customer needs to modify this to put the customized phone name */
    if( retval < 0 )
        strcpy( str, DTUN_DEFAULT_DEV_NAME );

    method.set_name.hdr.id = DTUN_METHOD_DM_SET_NAME;
    method.set_name.hdr.len = DTUN_MAX_DEV_NAME_LEN;
    strncpy( method.set_name.name, str, DTUN_MAX_DEV_NAME_LEN);

    dtun_client_call_method(&method);

    dtunops_init();
    adapter_ops_setup();

    rp.status = 0;
    memcpy( &rp.name, str, DTUN_MAX_DEV_NAME_LEN );
    rp.name[DTUN_MAX_DEV_NAME_LEN-1] = 0;
	
    adapter_update_local_name(&sba, 0, &rp);
	
    for(i=0; i<MAX_EXPOSED_SDP_HANDLES; i++ )
        sdp_handles[i] = FREE_EXPOSED_HANDLE; //SDP handles

}

void dtun_dm_sig_discovery_started(tDTUN_DEVICE_SIGNAL *msg)
{
    /* send start discovery started signal in dbus srv */
    PRINTFUNC();

    start_inquiry(&sba, 0, FALSE);
}

/* send  discovery complete signal in dbus srv */
void dtun_dm_sig_discovery_complete(tDTUN_DEVICE_SIGNAL *msg)
{
    PRINTFUNC();

    inquiry_complete(&sba, 0, FALSE);

}
/* Unicode macros and utf8_validate() from GLib Owen Taylor, Havoc
 * Pennington, and Tom Tromey are the authors and authorized relicense.
 */

/** computes length and mask of a unicode character
 * @param Char the char
 * @param Mask the mask variable to assign to
 * @param Len the length variable to assign to
 */
#define UTF8_COMPUTE(Char, Mask, Len)					      \
  if (Char < 128)							      \
    {									      \
      Len = 1;								      \
      Mask = 0x7f;							      \
    }									      \
  else if ((Char & 0xe0) == 0xc0)					      \
    {									      \
      Len = 2;								      \
      Mask = 0x1f;							      \
    }									      \
  else if ((Char & 0xf0) == 0xe0)					      \
    {									      \
      Len = 3;								      \
      Mask = 0x0f;							      \
    }									      \
  else if ((Char & 0xf8) == 0xf0)					      \
    {									      \
      Len = 4;								      \
      Mask = 0x07;							      \
    }									      \
  else if ((Char & 0xfc) == 0xf8)					      \
    {									      \
      Len = 5;								      \
      Mask = 0x03;							      \
    }									      \
  else if ((Char & 0xfe) == 0xfc)					      \
    {									      \
      Len = 6;								      \
      Mask = 0x01;							      \
    }									      \
  else                                                                        \
    {                                                                         \
      Len = 0;                                                               \
      Mask = 0;                                                               \
    }

/**
 * computes length of a unicode character in UTF-8
 * @param Char the char
 */
#define UTF8_LENGTH(Char)              \
  ((Char) < 0x80 ? 1 :                 \
   ((Char) < 0x800 ? 2 :               \
    ((Char) < 0x10000 ? 3 :            \
     ((Char) < 0x200000 ? 4 :          \
      ((Char) < 0x4000000 ? 5 : 6)))))
   
/**
 * Gets a UTF-8 value.
 *
 * @param Result variable for extracted unicode char.
 * @param Chars the bytes to decode
 * @param Count counter variable
 * @param Mask mask for this char
 * @param Len length for this char in bytes
 */
#define UTF8_GET(Result, Chars, Count, Mask, Len)			      \
  (Result) = (Chars)[0] & (Mask);					      \
  for ((Count) = 1; (Count) < (Len); ++(Count))				      \
    {									      \
      if (((Chars)[(Count)] & 0xc0) != 0x80)				      \
	{								      \
	  (Result) = -1;						      \
	  break;							      \
	}								      \
      (Result) <<= 6;							      \
      (Result) |= ((Chars)[(Count)] & 0x3f);				      \
    }

/**
 * Check whether a unicode char is in a valid range.
 *
 * @param Char the character
 */
#define UNICODE_VALID(Char)                   \
    ((Char) < 0x110000 &&                     \
     (((Char) & 0xFFFFF800) != 0xD800) &&     \
     ((Char) < 0xFDD0 || (Char) > 0xFDEF) &&  \
     ((Char) & 0xFFFF) != 0xFFFF)



gboolean utf8_validate  (const char *str,
                             int               start,
                             int               len)
{
  const unsigned char *p;
  const unsigned char *end;
  
  p = str + start;
  end = p + len;
  
  while (p < end)
    {
      // KJH 20091206 :: remote device name error fix (milo)
      int i, mask;
      unsigned int result;

      /* nul bytes considered invalid */
      if (*p == '\0')
        break;
      
      /* Special-case ASCII; this makes us go a lot faster in
       * D-Bus profiles where we are typically validating
       * function names and such. We have to know that
       * all following checks will pass for ASCII though,
       * comments follow ...
       */      
      if (*p < 128)
        {
          ++p;
          continue;
        }
      
      UTF8_COMPUTE (*p, mask, char_len);

      if (char_len == 0)  /* ASCII: char_len == 1 */
        break;

      /* check that the expected number of bytes exists in the remaining length */
      if (end - p < char_len) /* ASCII: p < end and char_len == 1 */
        break;
        
      UTF8_GET (result, p, i, mask, char_len);

      /* Check for overlong UTF-8 */
      if (UTF8_LENGTH (result) != char_len) /* ASCII: UTF8_LENGTH == 1 */
        break;
#if 0
      /* The UNICODE_VALID check below will catch this */
      if (_DBUS_UNLIKELY (result == (dbus_unichar_t)-1)) /* ASCII: result = ascii value */
        break;
#endif

      if (!UNICODE_VALID (result)) /* ASCII: always valid */
        break;

      /* UNICODE_VALID should have caught it */
      //assert(result != -1);
      
      p += char_len;
    }

  /* See that we covered the entire length if a length was
   * passed in
   */
  return p == end;
}


static inline void copy_device_name(char* dest, const char* src)
{
	// KJH 20091206 :: remote device name error fix (milo)
	int str_len = strlen(src);
	int i, div = 0;

	if(str_len > DTUN_MAX_DEV_NAME_LEN)
		str_len = DTUN_MAX_DEV_NAME_LEN; 

	gboolean bValidUtf8 = utf8_validate(src, 0, str_len);

	//milo : [CSP276031] remote device name display problem //
	if(char_len > 0 && !bValidUtf8) {

		div = str_len%char_len;

		if(div != 0) {
			info("milo/div=%d,str_len=%d,char_len=%d", div, str_len, char_len);
			str_len -= div;
			info("/milo/now str_len is %d", str_len);
		}

	}

	for(i = 0; i < str_len; i++)
	{
		// KJH 20091206 :: remote device name error fix (milo)
		//if(src[i] <= 31 || src[i] == 127 || (!bValidUtf8 && (unsigned char)src[i] >= 128))
		if(src[i] <= 31 || src[i] == 127)
		dest[i] = '.';
		else dest[i] = src[i];
	}
	
	dest[i] = 0;
}


void dtun_dm_sig_rmt_name(tDTUN_DEVICE_SIGNAL *msg)
{

    char name[DTUN_MAX_DEV_NAME_LEN+1];
	
    // KJH 20091201
    char bdaddr_str[18];
    ba2str(&msg->rmt_name.info.bd_addr, bdaddr_str);	
    //PRINTFUNC();

    copy_device_name(name, msg->rmt_name.info.bd_name);
    write_device_name(&sba, &msg->rmt_name.info.bd_addr, name);
    hcid_dbus_remote_name(&sba, &msg->rmt_name.info.bd_addr, 0, name);

    // KJH 20091201
    info("\t*** sig_rmt_name :: name = [%s] addr = [%s] ***\n\n", 
         name, bdaddr_str);
	
}

void dtun_dm_sig_device_found(tDTUN_DEVICE_SIGNAL *msg)
{
    char bdaddr_str[18];
    ba2str(&msg->device_found.info.bd, bdaddr_str);

    // KJH 20091201
    info("\t*** sig_device_found :: addr = [%s] class = [%x] ***\n\n", 
         bdaddr_str, msg->device_found.info.cod);

    // KJH 20091213 : when the cod 0 is detected, this device is filtered (henry)
    if (msg->device_found.info.cod == 0) {
        error("class 0 is detected. So this device [%s] is filtered.", bdaddr_str);
        return;
    }
	
    hcid_dbus_inquiry_result(&sba, &msg->device_found.info.bd,
			msg->device_found.info.cod, msg->device_found.info.rssi, NULL);

    write_remote_class(&sba, &msg->device_found.info.bd, msg->device_found.info.cod);

    update_lastseen(&sba, &msg->device_found.info.bd);

}

void dtun_dm_sig_pin_req(tDTUN_DEVICE_SIGNAL *msg)
{
    // KJH 20091201
    char bdaddr_str[18];
    ba2str(&msg->pin_req.info.bdaddr, bdaddr_str);	
    uint32_t cur_cod = 0;

    read_remote_class( &sba, &msg->pin_req.info.bdaddr, &cur_cod );

    // KJH 20091213 :: workaround for icon issue. after sig_pin_req, current cod is different from received cod.
    // if saved cod is 0, received cod will be updated.
    info("\t*** sig_pin_req :: addr = [%s], current cod = [%x], received cod = [%x] ***\n\n", 
         bdaddr_str, cur_cod, msg->pin_req.info.cod);

    if (cur_cod != 0)
    {
        if (cur_cod != msg->pin_req.info.cod)
        error("current cod is not 0. don't update cod !!!");
    }
    else
    {
        // KJH 20091210
        info("current cod == 0, update cod !!!");
        hcid_dbus_remote_class(&sba, &msg->pin_req.info.bdaddr, msg->pin_req.info.cod);
        write_remote_class(&sba, &msg->pin_req.info.bdaddr, msg->pin_req.info.cod);
    }
    pin_code_request( 0, &sba, msg->pin_req.info.bdaddr.b );
}



static void dtun_auth_cb(DBusError *derr, void *user_data)
{
    tDTUN_DEVICE_METHOD method;
    tDTUN_SIG_DM_AUTHORIZE_REQ_INFO *pending_info = (tDTUN_SIG_DM_AUTHORIZE_REQ_INFO *) user_data;

    PRINTFUNC();

    dtun_auth_on = FALSE;

    memcpy( &method.authorize_rsp.info.bd_addr, &pending_info->bd_addr, 6);
    method.authorize_rsp.info.service = pending_info->service;


	if (derr && dbus_error_is_set(derr)) {
		method.authorize_rsp.info.response = 2;
	}
	else {
			method.authorize_rsp.info.response = 1;
	}
	
    method.authorize_rsp.hdr.id = DTUN_METHOD_DM_AUTHORIZE_RSP;
    method.authorize_rsp.hdr.len = sizeof(tDTUN_METHOD_DM_AUTHORIZE_RSP_INFO);
    dtun_client_call_method(&method);

}


void dtun_dm_sig_authorize_req(tDTUN_DEVICE_SIGNAL *msg)
{
	int err;
	DBusError derr;
        //PRINTFUNC();
	uint32_t cur_cod = 0;

	read_remote_class( &sba, &msg->authorize_req.info.bd_addr, &cur_cod );

       // KJH 20091213 :: workaround for icon issue. after sig_authorize_req, current cod is different from received cod.
       // if saved cod is 0, received cod will be updated.
       info("\t*** sig_authorize_req :: current cod = [%x] received cod = [%x] ***\n\n", 
            cur_cod, msg->authorize_req.info.cod);

       if (cur_cod != 0)
       {
           if (cur_cod != msg->authorize_req.info.cod)
           error("current cod is not 0. don't update cod !!!");
       }
       else
       {
	   // KJH 20091210
	   info("current cod == 0, update cod !!!");
	   hcid_dbus_remote_class(&sba, &msg->authorize_req.info.bd_addr, msg->authorize_req.info.cod);
          write_remote_class(&sba, &msg->authorize_req.info.bd_addr, msg->authorize_req.info.cod);
       }

        /* in the case were dtun_auth_cb gets called from here, this needs to be intialized */

       if( dtun_auth_on )
       {
             err = -1;
       }
	else
	{
		dtun_auth_on = TRUE;
		memcpy( &dtun_cur_authorize_req_info, &msg->authorize_req.info,
				sizeof( tDTUN_SIG_DM_AUTHORIZE_REQ_INFO)  );

		err = btd_request_authorization(&sba, &msg->authorize_req.info.bd_addr, 
			                     dtunc_uuid_table[msg->authorize_req.info.service], dtun_auth_cb,
						&dtun_cur_authorize_req_info);
	}
	
	if (err < 0) {
		debug("Authorization denied(%d): %s", err,strerror(-err));
		dbus_error_init(&derr);

		dbus_set_error_const(&derr, "org.bluez.Error.Failed", strerror(-err));

		dtun_auth_cb(&derr, &msg->authorize_req.info);

		return;
	}

	

}


void dtun_dm_sig_link_down(tDTUN_DEVICE_SIGNAL *msg)
{
    // KJH 20091201
    char bdaddr_str[18];
    ba2str(&msg->link_down.info.bd_addr, bdaddr_str);	
	
    //PRINTFUNC();

	struct btd_adapter *tadapter;
	struct btd_device *device;

	uint16_t handle;
	
	if (!get_adapter_and_device(&sba, &msg->link_down.info.bd_addr, &tadapter, &device, FALSE))
		return;
        // + KJH 20091223 :: bluetoothd crash fix (IMS 280640, 280644) (milo)
        //milo [CSP280644] : bluetoothd crash occurs because of accessing null pointer//
        if(!device) {
          return;
        }

	handle = device_get_conn_handle(device);

	//info( "dtun_dm_sig_link_down device = %p handle = %d", device, handle );

       // KJH 20091201
       info("\t*** sig_link_down :: device = [%p] handle = [%d] addr = [%s] reason = [%x] ***\n\n", 
            device, handle, bdaddr_str, msg->link_down.info.reason);
	   
	hcid_dbus_disconn_complete(&sba, 0, handle,
					msg->link_down.info.reason);
	
	if( msg->link_down.info.reason == 5 ) //If reason code is authentication failure
		device_check_bonding_failed( device, 	msg->link_down.info.reason );

}

uint16_t dummy_handle=0x100;

void dtun_dm_sig_link_up(tDTUN_DEVICE_SIGNAL *msg)
{

    PRINTFUNC();

	info( "dtun_dm_sig_link_up: dummy_handle = %d", dummy_handle );
	
	hcid_dbus_conn_complete( &sba, 0, dummy_handle,  &msg->link_up.info.bd_addr );
       dummy_handle++;
}

uint32_t	dtun_add_sdp_record( DBusMessage *msg )
{
	tDTUN_DEVICE_METHOD method;
	uuid_t uuid;
	const char *name;
	uint16_t channel;
	uint32_t *uuid_p;
	uint32_t uuid_net[4];   // network order
	uint64_t uuid_host[2];  // host
	int i;

	if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &name,
			DBUS_TYPE_UINT64, &uuid_host[0],
			DBUS_TYPE_UINT64, &uuid_host[1],
			DBUS_TYPE_UINT16, &channel,
			DBUS_TYPE_INVALID))
		return 0xFFFFFFFF;

	for( i=0; i<MAX_EXPOSED_SDP_HANDLES; i++ )
	{
	    if( sdp_handles[i] == FREE_EXPOSED_HANDLE ) //Found a free handle
	    {
	        sdp_handles[i] = ASSIGNED_EXPOSED_HANDLE;
		 break;
	    }
	}
	
	if( i==MAX_EXPOSED_SDP_HANDLES )
		return 0xFFFFFFFE;
	
	uuid_p = (uint32_t *)uuid_host;
	uuid_net[1] = htonl(*uuid_p++);
	uuid_net[0] = htonl(*uuid_p++);
	uuid_net[3] = htonl(*uuid_p++);
	uuid_net[2] = htonl(*uuid_p++);

	sdp_uuid128_create(&uuid, (void *)uuid_net);
	
 	method.add_sdp_rec.hdr.id = DTUN_METHOD_DM_ADD_SDP_REC;
	method.add_sdp_rec.hdr.len = sizeof( tDTUN_METHOD_DM_ADD_SDP_REC_INFO);
	method.add_sdp_rec.info.exposed_handle = i;
	memcpy( &method.add_sdp_rec.info.uuid, &uuid.value.uuid128.data, 16 );
	method.add_sdp_rec.info.channel = channel;
	
	strncpy(&method.add_sdp_rec.info.name, name, DTUN_MAX_DEV_NAME_LEN);
       method.add_sdp_rec.info.name[DTUN_MAX_DEV_NAME_LEN - 1] = 0;
	   
	dtun_client_call_method(&method);
	return (i);


}

void dtun_dm_sig_sdp_handle(tDTUN_DEVICE_SIGNAL *msg)
{

	info( "dtun_dm_sig_sdp_rec_handle: handle = 0x%x", msg->sdp_handle.handle);

	sdp_handles[msg->sdp_handle.exposed_handle] = msg->sdp_handle.handle;
}

void	dtun_del_sdp_record( uint32_t handle )
{
	tDTUN_DEVICE_METHOD method;

	if( handle >= 32 ) 
		return;

	if( (sdp_handles[handle] == FREE_EXPOSED_HANDLE) || 
	     (sdp_handles[handle] == ASSIGNED_EXPOSED_HANDLE) )
		return;
	
 	method.del_sdp_rec.hdr.id = DTUN_METHOD_DM_DEL_SDP_REC;
	method.del_sdp_rec.hdr.len = 4; 
	method.del_sdp_rec.handle = sdp_handles[handle];
	dtun_client_call_method(&method);
	sdp_handles[handle] = FREE_EXPOSED_HANDLE;
	return;
}

void dtun_dm_sig_auth_comp(tDTUN_DEVICE_SIGNAL *msg)
{
    uint8_t st;
    evt_link_key_notify lk_ev;

    // KJH 20091201
    info("\t***  sig_auth_comp status: %x ***\n\n", msg->auth_comp.info.success );

    if( msg->auth_comp.info.key_present )
    {
        memcpy( lk_ev.bdaddr.b, msg->auth_comp.info.bd_addr.b, 6 );
        memcpy( lk_ev.link_key, msg->auth_comp.info.key, 16 );
        lk_ev.key_type = msg->auth_comp.info.key_type;
        link_key_notify( 0, &sba, &lk_ev );
    }

    st = msg->auth_comp.info.success;

    if( !st )
    {
	hcid_dbus_conn_complete( &sba, 0, dummy_handle,  &msg->auth_comp.info.bd_addr );
       dummy_handle++;
    }
    hcid_dbus_bonding_process_complete(&sba, &msg->auth_comp.info.bd_addr, st);

}

static void dtun_dm_sig_ssp_cfm_req(tDTUN_DEVICE_SIGNAL *msg)
{
    unsigned long ssp_pin;
    unsigned long ssp_mode;

    char bdaddr_str[18];
    ba2str(&msg->ssp_cfm_req.info.bd_addr, bdaddr_str);	
    uint32_t cur_cod = 0;

    read_remote_class( &sba, &msg->ssp_cfm_req.info.bd_addr, &cur_cod );

    // KJH 20091213 :: workaround for icon issue. after sig_ssp_cfm_req, current cod is different from received cod.
    // if saved cod is 0, received cod will be updated.
    info("\t*** dtun_dm_sig_ssp_cfm_req :: addr = [%s], current cod = [%x], received cod = [%x] ***\n\n", 
         bdaddr_str, cur_cod, msg->ssp_cfm_req.info.cod);

    if (cur_cod != 0)
    {
        if (cur_cod != msg->pin_req.info.cod)
        error("current cod is not 0. don't update cod !!!");
    }
    else
    {
       // KJH 20091210
       info("current cod == 0, update cod !!!");
       hcid_dbus_remote_class(&sba, &msg->ssp_cfm_req.info.bd_addr, msg->ssp_cfm_req.info.cod);
       write_remote_class(&sba, &msg->ssp_cfm_req.info.bd_addr, msg->ssp_cfm_req.info.cod);
    }

    ssp_pin = msg->ssp_cfm_req.info.num_value;

     info( "Just Works = %d", msg->ssp_cfm_req.info.just_work );
     if(msg->ssp_cfm_req.info.just_work == true)
         ssp_pin = 0x80000000; //This is used as Bluetooth.Error in the JAVA space
	 
     hcid_dbus_user_confirm(&sba, &msg->ssp_cfm_req.info.bd_addr, ssp_pin);     /* Java side is 2 for just work */

    return;
}


/* return testmode state to callee */
static void dtun_dm_sig_testmode_state( tDTUN_DEVICE_SIGNAL *msg )
{
    info( "dtun_dm_sig_testmode_state( state: %d )", msg->testmode_state.state );
} /* dtun_dm_sig_testmode_state() */


/*******************************************************************************
** GetRemoteServiceChannel hander and callback
*******************************************************************************/
uuid_t dtun_get_scn_uuid;
struct btd_device *dtun_get_scn_device = NULL;
void dtun_client_get_remote_svc_channel(struct btd_device *device, bdaddr_t rmt, uuid_t *search)
{
    tDTUN_DEVICE_METHOD method;

    memcpy( &dtun_get_scn_uuid, search, sizeof( uuid_t ) );
    dtun_get_scn_device = device;
	
//    uint16_t uuid16 = (uint16_t) ((search->value.uuid128.data[0] << 8) |search->value.uuid128.data[1]);

    /* Send message to btld */
    method.get_scn.hdr.id = DTUN_METHOD_DM_GET_REMOTE_SERVICE_CHANNEL;
    method.get_scn.hdr.len = (sizeof(tDTUN_GET_SCN) - sizeof(tDTUN_HDR));
    memcpy( method.get_scn.uuid,  search->value.uuid128.data, 16 );
    memcpy(&method.get_scn.bdaddr.b, &rmt.b, 6);
    LOGI("%s: starting discovery on  (uuid16=0x%04x)", __FUNCTION__, search->value.uuid128.data[0] | search->value.uuid128.data[1]);
    LOGI("   bdaddr=%02X:%02X:%02X:%02X:%02X:%02X", method.get_scn.bdaddr.b[0], method.get_scn.bdaddr.b[1], method.get_scn.bdaddr.b[2],
                                                        method.get_scn.bdaddr.b[3], method.get_scn.bdaddr.b[4], method.get_scn.bdaddr.b[5]);

    dtun_client_call_method(&method);

    return;
}


void dtun_dm_sig_rmt_service_channel(tDTUN_DEVICE_SIGNAL *msg)
{

    LOGI("%s: success=%i, service=%08X", __FUNCTION__, msg->rmt_scn.success, (unsigned int)msg->rmt_scn.services);

	if ((msg->rmt_scn.success >= 3) && (msg->rmt_scn.services) && dtun_get_scn_device )
	{
           device_add_rfcomm_record(dtun_get_scn_device, dtun_get_scn_uuid, (msg->rmt_scn.success	- 3) );
           btd_device_append_uuid(dtun_get_scn_device, bt_uuid2string(&dtun_get_scn_uuid));
	    btd_device_commit_uuids( dtun_get_scn_device );	
	}
	else
	{
	    /* discovery unsuccessful */
            error( "discovery unsuccessful!" );
	}
       dtun_get_scn_device = NULL;
	   
//TODO: Store the service channel
}

/*******************************************************************************
** GetRemoteServices handler and callback 
*******************************************************************************/


void dtun_client_get_remote_services(bdaddr_t rmt)
{
    tDTUN_DEVICE_METHOD method;

    PRINTFUNC();
        
    /* Send message to btld */
    method.rmt_dev.hdr.id = DTUN_METHOD_DM_GET_REMOTE_SERVICES;
    method.rmt_dev.hdr.len = (sizeof(tDTUN_METHOD_RMT_DEV) - sizeof(tDTUN_HDR));
    memcpy( &method.rmt_dev.bdaddr.b, &rmt.b, 6);
    LOGI("%s: Get remote services on ", __FUNCTION__);
    dtun_pending_get_services_flg = TRUE;
    memcpy( &dtun_pending_get_services_adr.b, &rmt.b, 6);
        
    dtun_client_call_method(&method);
       
    return;
}

void dtun_client_get_all_remote_services(bdaddr_t rmt)
{
    tDTUN_DEVICE_METHOD method;

    PRINTFUNC();
        
    /* Send message to btld */
    method.rmt_dev.hdr.id = DTUN_METHOD_DM_GET_ALL_REMOTE_SERVICES;
    method.rmt_dev.hdr.len = (sizeof(tDTUN_METHOD_RMT_DEV) - sizeof(tDTUN_HDR));
    memcpy( &method.rmt_dev.bdaddr.b, &rmt.b, 6);
    LOGI("%s: Get all remote services on ", __FUNCTION__);
    dtun_pending_get_services_flg = TRUE;
    memcpy( &dtun_pending_get_services_adr.b, &rmt.b, 6);
        
    dtun_client_call_method(&method);
       
    return;
}

static DBusMessage *sink_connect(DBusConnection *conn,
                                DBusMessage *msg, void *data)
{
        struct audio_device *dev = data;
        struct sink *sink = dev->sink;
        struct pending_request *pending;
        tDTUN_DEVICE_METHOD method;
        const char *address;
        DBusMessage *reply;

        if (sink->connect || sink->disconnect)
                return g_dbus_create_error(msg, ERROR_INTERFACE ".Failed",
                                                "%s", strerror(EBUSY));


        if (sink->state >= AVDTP_STATE_OPEN) {
                g_dbus_emit_signal(dev->conn, dev->path,
                                                AUDIO_SINK_INTERFACE,
                                                "Connected",
                                                DBUS_TYPE_INVALID);
                reply = dbus_message_new_method_return(msg);
                return reply;
        }

        pending = g_new0(struct pending_request, 1);
        pending->conn = dbus_connection_ref(conn);
        pending->msg = dbus_message_ref(msg);
        sink->connect = pending;

        memcpy(method.av_open.bdaddr.b, &dev->dst, 6);

       method.av_open.hdr.id = DTUN_METHOD_AM_AV_OPEN;
       method.av_open.hdr.len = 6;
        dtun_client_call_method(&method);

        debug("stream creation in progress");

        return NULL;
}

static DBusMessage *sink_disconnect(DBusConnection *conn,
                                        DBusMessage *msg, void *data)
{

        struct audio_device *device = data;
        struct sink *sink = device->sink;
        struct pending_request *pending;
        int err;
        tDTUN_DEVICE_METHOD method;

        if (sink->connect || sink->disconnect)
                return g_dbus_create_error(msg, ERROR_INTERFACE ".Failed",
                                                "%s", strerror(EBUSY));

        if (sink->state < AVDTP_STATE_OPEN) {
                DBusMessage *reply = dbus_message_new_method_return(msg);
                if (!reply)
                        return NULL;
                return reply;
        }

        memcpy(method.av_disc.bdaddr.b, &device->dst, 6);

       method.av_open.hdr.id = DTUN_METHOD_AM_AV_DISC;
       method.av_open.hdr.len = 6;
        dtun_client_call_method(&method);

        pending = g_new0(struct pending_request, 1);
        pending->conn = dbus_connection_ref(conn);
        pending->msg = dbus_message_ref(msg);
        sink->disconnect = pending;

        return NULL;

}

static DBusMessage *sink_get_properties(DBusConnection *conn,
                                        DBusMessage *msg, void *data)
{
        struct audio_device *device = data;
        struct sink *sink = device->sink;
        DBusMessage *reply;
        DBusMessageIter iter;
        DBusMessageIter dict;
        const char *state;
        gboolean value;

        reply = dbus_message_new_method_return(msg);
        if (!reply)
                return NULL;

        dbus_message_iter_init_append(reply, &iter);

        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                        DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
                        DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
                        DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

        /* Playing */
        value = (sink->state == AVDTP_STATE_STREAMING);
        dict_append_entry(&dict, "Playing", DBUS_TYPE_BOOLEAN, &value);

        /* Connected */
        value = (sink->state >= AVDTP_STATE_CONFIGURED);
        dict_append_entry(&dict, "Connected", DBUS_TYPE_BOOLEAN, &value);

        /* State */
        state = state2str(sink->state);
        if (state)
                dict_append_entry(&dict, "State", DBUS_TYPE_STRING, &state);

        dbus_message_iter_close_container(&iter, &dict);

        return reply;
}

static DBusMessage *sink_is_connected(DBusConnection *conn,
                                        DBusMessage *msg,
                                        void *data)
{
        struct audio_device *device = data;
        struct sink *sink = device->sink;
        DBusMessage *reply;
        dbus_bool_t connected;

        reply = dbus_message_new_method_return(msg);
        if (!reply)
                return NULL;

        connected = (sink->state >= AVDTP_STATE_CONFIGURED);

        dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &connected,
                                        DBUS_TYPE_INVALID);

        return reply;
}

static GDBusMethodTable sink_methods[] = {
        { "Connect",            "",     "",     sink_connect,
                                                G_DBUS_METHOD_FLAG_ASYNC },
        { "Disconnect",         "",     "",     sink_disconnect,
                                                G_DBUS_METHOD_FLAG_ASYNC },
        { "IsConnected",        "",     "b",    sink_is_connected,
                                                G_DBUS_METHOD_FLAG_DEPRECATED },
        { "GetProperties",      "",     "a{sv}",sink_get_properties },
        { NULL, NULL, NULL, NULL }
};

static GDBusSignalTable sink_signals[] = {
        { "Connected",                  "",     G_DBUS_SIGNAL_FLAG_DEPRECATED },
        { "Disconnected",               "",     G_DBUS_SIGNAL_FLAG_DEPRECATED },
        { "Playing",                    "",     G_DBUS_SIGNAL_FLAG_DEPRECATED },
        { "Stopped",                    "",     G_DBUS_SIGNAL_FLAG_DEPRECATED },
        { "PropertyChanged",            "sv"    },
        { NULL, NULL }
};


static void pending_request_free(struct pending_request *pending)
{
        if (pending->conn)
                dbus_connection_unref(pending->conn);
        if (pending->msg)
                dbus_message_unref(pending->msg);
        g_free(pending);
}


static void sink_free(struct audio_device *dev)
{
        struct sink *sink = dev->sink;

        if (sink->connect)
                pending_request_free(sink->connect);

        if (sink->disconnect)
                pending_request_free(sink->disconnect);

#if 0
        if (sink->cb_id)
                avdtp_stream_remove_cb(sink->session, sink->stream,
                                        sink->cb_id);

        if (sink->dc_id)
                device_remove_disconnect_watch(dev->btd_dev, sink->dc_id);

        if (sink->session)
                avdtp_unref(sink->session);

        if (sink->retry_id)
                g_source_remove(sink->retry_id);

#endif
        g_free(sink);
        dev->sink = NULL;
}

void sink_unregister( struct audio_device *dev )
{
	g_dbus_unregister_interface(dev->conn, dev->path,
		AUDIO_SINK_INTERFACE);
}

static void path_unregister(void *data)
{
        struct audio_device *dev = data;

        debug("Unregistered interface %s on path %s",
                AUDIO_SINK_INTERFACE, dev->path);

        sink_free(dev);
}

struct sink *sink_init(struct audio_device *dev)
{
        struct sink *sink;

        LOGI("sink_init");
        if (!g_dbus_register_interface(dev->conn, dev->path,
                                        AUDIO_SINK_INTERFACE,
                                        sink_methods, sink_signals, NULL,
                                        dev, path_unregister))
                return NULL;

        debug("Registered interface %s on path %s",
                AUDIO_SINK_INTERFACE, dev->path);

        sink = g_new0(struct sink, 1);

        sink->dev = dev;

        return sink;
}

void dtun_dm_sig_rmt_services(tDTUN_DEVICE_SIGNAL *msg)
{
    DBusMessage *reply;
    uint32_t service_mask;
    boolean success;
    uint32_t i;
    struct btd_adapter *tadapter;
    struct btd_device *device;
	
    LOGI("%s: success=%i, service=%08X", __FUNCTION__, msg->rmt_services.success, (unsigned int)msg->rmt_services.services);

    if (dtun_pending_get_services_flg == FALSE)
    {
        LOGI("%s: callback cancelled", __FUNCTION__);
        return;
    }
    
    service_mask = (uint32_t)msg->rmt_services.services;
    success = msg->rmt_services.success;

    if (success && (get_adapter_and_device(&sba, &dtun_pending_get_services_adr, &tadapter, &device, FALSE)))
    {
        // + KJH 20091223 :: bluetoothd crash fix (IMS 280640, 280644) (milo)	
        //milo : checking null pointer //
        if(device) {	  

        for( i=0; i<DTUN_NUM_UUIDS_IN_TABLE; i++ )
        {
            if( service_mask & (0x1 << i) ) {
                debug( "Adding UUID %s",  dtunc_uuid_table[i] );
				
                btd_device_append_uuid(device, dtunc_uuid_table[i]);
            }
        }

	 btd_device_commit_uuids( device );	

        } else {
            error("No device pointer found for peer!");
        }

    }
    else if (msg->rmt_services.ignore_err == TRUE) 
    {
      error( "No device pointer found for peer! Ignore Error = true. Ignoring error..." );
      return;
    }
    else
    {
          error( "No Services found, No device pointer found for peer!" );
    }	

    dtun_pending_get_services_flg = FALSE;
}


static void dtun_am_sig_av_event(tDTUN_DEVICE_SIGNAL *msg)
{
    char dev_path[64];
    const char *dpath = dev_path;
    DBusMessage *reply = NULL;
    struct audio_device *pdev;
    struct pending_request *pending = NULL;
    char err_msg[32];
    DBusMessage *orig_msg = NULL;
    boolean new_bonding = false;
    avdtp_state_t old_state;
    gboolean value;
    const char *state_str;

    debug("dtun_am_sig_av_event with event=%d\n", msg->av_event.info.event);

    if (msg->av_event.info.event == 2) /* BTA_AV_OPEN_EVT */
    {
        err_msg[0] = 0;

        switch (msg->av_event.info.status) {

        case 1:
            strcpy(err_msg, "Generic failure");
            break;

        case 2:
            strcpy(err_msg, "Service not found");
            break;

        case 3:
            strcpy(err_msg, "Stream connection failed");
            break;

        case 4:
            strcpy(err_msg, "No resources");
            break;

        case 5:
            strcpy(err_msg, "Role change failure");
            break;
        }

        pdev = manager_find_device(NULL, &sba, &msg->av_event.info.peer_addr, NULL, FALSE);

        if (!pdev)
        {
            info("new bonding\n");
            new_bonding = true;
            orig_msg = dtun_am_creat_dev_msg;
        } else
        {
            info("pending\n");
            pending = pdev->sink->connect;
            if (pending)
            {
                orig_msg = pending->msg;
                info("orig_msg = %x\n", orig_msg);
                pdev->sink->connect = NULL;
            }
        }

        debug( "2.  before err msg chk (%s)\n", err_msg);
        //	printf( "msg->av_event.info.path = %s\n", msg->av_event.info.path );

        // * KJH 20091231 :: Resolved a2dp_start fail - BT620S [2582769] (Henry)
        debug( "Henry : err_msg : %s", err_msg);
        // Henry : We don't need to check orig_msg. If there is any error, we have to return error_failed
        // if (orig_msg && err_msg[0])
        if(err_msg[0] != 0)
        {
            error_failed(sig_connection, orig_msg, err_msg);
            //dbus_message_unref( orig_msg );
            //dtun_am_creat_dev_msg = NULL;

            if (pending)
                pending_request_free(pending);

            return;
        }

        if (new_bonding)
        {
            struct btd_device *device;
            struct btd_adapter *tadapter;
            if (!get_adapter_and_device(&sba, &msg->av_event.info.peer_addr, &tadapter, &device, FALSE))
	         return;
            // + KJH 20091223 :: bluetoothd crash fix (IMS 280640, 280644) (milo)
            //milo : checking null pointer//
            if(!device) {
              debug("Try to bond newly, but it couldn't get device pointer");
              return;
            }
			
            btd_device_append_uuid(device, A2DP_SINK_UUID);
            btd_device_commit_uuids(device);	
            pdev = manager_find_device(NULL, &sba, &msg->av_event.info.peer_addr, NULL, FALSE);
        }

        {
            old_state = pdev->sink->state;
            pdev->sink->state = AVDTP_STATE_OPEN;
            if (pending)
            {
                int busywait;
                debug( "Answering Pending Req\n" );
                //busy wait added to separate the 2 dbus transactions
#if 0 /* TBD, why is needed??? */
                for (busywait = 0; busywait < 10000; busywait++)
                    ;
#endif
                reply = dbus_message_new_method_return(pending->msg);
                if (!reply)
                    return;
                dbus_connection_send(pending->conn, reply, NULL);
                dbus_message_unref(reply);
                pending_request_free(pending);
                pdev->sink->connect = NULL;
            }
            if (old_state <= AVDTP_STATE_OPEN)
            {
                value = TRUE;
                state_str = "connected";

            g_dbus_emit_signal(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                               "Connected", DBUS_TYPE_INVALID);
            emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                  "Connected", DBUS_TYPE_BOOLEAN, &value);
            emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                  "State", DBUS_TYPE_STRING, &state_str);
            debug("Stream successfully created");
        }
            else if (old_state == AVDTP_STATE_STREAMING)
            {
                value = FALSE;
                state_str = "connected";

                g_dbus_emit_signal(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                   "Stopped", DBUS_TYPE_INVALID);
                emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                      "Playing", DBUS_TYPE_BOOLEAN, &value);
                emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                      "State", DBUS_TYPE_STRING, &state_str);
                debug("Stream stopped");
            }
        }
    }  /* event==2 */
    if (msg->av_event.info.event == 3)
    {
        value = FALSE;
        state_str = "disconnected";

        pdev = manager_find_device(NULL, &sba, &msg->av_event.info.peer_addr, NULL, FALSE);
        if (pdev)
        {
	    if (pdev->sink == NULL)
	    {
                debug("a2dp close: audio sink is removed ");
		return;
	    }
            pdev->sink->state = AVDTP_STATE_IDLE;
            if (pending = pdev->sink->disconnect) //Assignment on purpose
            {
                reply = dbus_message_new_method_return(pending->msg);
                if (!reply)
                    return;
                dbus_connection_send(pending->conn, reply, NULL);
                dbus_message_unref(reply);
                pending_request_free(pending);
                pdev->sink->disconnect = NULL;
            }
            g_dbus_emit_signal(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                    "Disconnected", DBUS_TYPE_INVALID);
            emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                  "Connected", DBUS_TYPE_BOOLEAN, &value);
            emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                  "State", DBUS_TYPE_STRING, &state_str);

            debug("Stream successfully disconnected");
        }
        else
            debug("Stream was already removed ");
    } /* event==3 */

    if (msg->av_event.info.event == 4)
    {
       pdev = manager_find_device(NULL, &sba, &msg->av_event.info.peer_addr, NULL, FALSE);
	if( pdev )
	{
            value = TRUE;
            state_str = "playing";
		pdev->sink->state = AVDTP_STATE_STREAMING;	
            g_dbus_emit_signal(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
						"Playing", DBUS_TYPE_INVALID);	
            emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                  "Playing", DBUS_TYPE_BOOLEAN, &value);
            emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                  "State", DBUS_TYPE_STRING, &state_str);
        }
    } /* even==4 */
    if(msg->av_event.info.event == 15) 
    {
       pdev = manager_find_device(NULL, &sba, &msg->av_event.info.peer_addr, NULL, FALSE);
	if( pdev )
	{
            value = FALSE;
            state_str = "connected";

            if (pdev->sink->state > AVDTP_STATE_OPEN) {	
                g_dbus_emit_signal(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
	            				"Stopped", DBUS_TYPE_INVALID);	
                emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                      "Playing", DBUS_TYPE_BOOLEAN, &value);
                emit_property_changed(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                                      "State", DBUS_TYPE_STRING, &state_str);
            }
            pdev->sink->state = AVDTP_STATE_OPEN;	
            debug("Stream suspended");
        }
    } /* event==15 */
} /* dtun_am_sig_av_event() */

#if 0
static void dtun_spp_sig_port_event(tDTUN_DEVICE_SIGNAL *msg)
{
    uint8_t port = msg->spp_port_event.port;
    uint8_t event = msg->spp_port_event.event;

    info("#### %s %d %d ####\n", __FUNCTION__, port, event);
    /* Just need to send the port number and event code to SPP service and let SPP service to inform the application associate with the port */
    if (!g_dbus_emit_signal(sig_connection, "/org/bluez/Serial",
                        "org.bluez.Serial.port",
                        "SerialPortEvent",
                        DBUS_TYPE_BYTE, &port,
                        DBUS_TYPE_BYTE, &event,
                        DBUS_TYPE_INVALID) ) {

           error("#### %s send dbus signal failed ####\n", __FUNCTION__);
    }

}
#endif

/* signal callback table */
const tDTUN_SIGNAL dtun_signal_tbl[] =
{
    /* DM signals */
    dtun_dm_sig_local_info,
    dtun_dm_sig_discovery_started,
    dtun_dm_sig_discovery_complete,
    dtun_dm_sig_device_found,
    dtun_dm_sig_rmt_name,
    dtun_dm_sig_rmt_service_channel,
    dtun_dm_sig_rmt_services,           /* DTUN_SIG_DM_RMT_SERVICES */
    dtun_dm_sig_pin_req,
    dtun_dm_sig_authorize_req,
    dtun_dm_sig_auth_comp,
    dtun_dm_sig_link_down,
    dtun_dm_sig_ssp_cfm_req,
    dtun_dm_sig_link_up,
    dtun_dm_sig_sdp_handle,    
    dtun_dm_sig_testmode_state,
    /* AV signals */
    dtun_am_sig_av_event,
    /* OPC signals */
    dtun_sig_opc_enable,             /* DTUN_SIG_OPC_ENABLE */
    dtun_sig_opc_open,               /* DTUN_SIG_OPC_OPEN */
    dtun_sig_opc_progress,           /* DTUN_SIG_OPC_PROGRESS */
    dtun_sig_opc_object_received,    /* DTUN_SIG_OPC_OBJECT_RECEIVED */
    dtun_sig_opc_object_pushed,      /* DTUN_SIG_OPC_OBJECT_PUSHED */
    dtun_sig_opc_close,              /* DTUN_SIG_OPC_CLOSE */

    /* OPS signals */
    dtun_sig_ops_progress,           /* DTUN_SIG_OPS_PROGRESS */
    dtun_sig_ops_object_received,    /* DTUN_SIG_OPS_OBJECT_RECEIVED */
    dtun_sig_ops_open,               /* DTUN_SIG_OPS_OPEN */
    dtun_sig_ops_access_request,     /* DTUN_SIG_OPS_ACCESS_REQUEST */
    dtun_sig_ops_close,              /* DTUN_SIG_OPS_CLOSE */
    dtun_sig_op_create_vcard,        /* DTUN_SIG_OP_CREATE_VCARD */
    dtun_sig_op_owner_vcard_not_set, /* DTUN_SIG_OP_OWNER_VCARD_NOT_SET */
    dtun_sig_op_store_vcard,         /* DTUN_SIG_OP_STORE_VCARD */
    dtun_sig_op_set_default_path     /* DTUN_SIG_OP_SET_DEFAULT_PATH */
};

void dtun_process_started(void)
{
     /* get dbus connection in dtun thread context */
     //sig_connection = dbus_bus_get( DBUS_BUS_SYSTEM, NULL );
}

 void dtun_pin_reply( tDTUN_ID id,  pin_code_reply_cp *pr)
{
    tDTUN_DEVICE_METHOD method;

    method.pin_reply.hdr.id = id;

    if( id == DTUN_METHOD_DM_PIN_REPLY )
    {
        method.pin_reply.hdr.len = (sizeof( tDTUN_METHOD_DM_PIN_REPLY) - sizeof(tDTUN_HDR));

        method.pin_reply.pin_len = pr->pin_len;
        memcpy(method.pin_reply.bdaddr.b, pr->bdaddr.b, 6);
        memcpy(method.pin_reply.pin_code, pr->pin_code, pr->pin_len);
    }
    else
    {
        method.pin_reply.hdr.len = (sizeof( tDTUN_METHOD_DM_PIN_NEG_REPLY) - sizeof(tDTUN_HDR));

        method.pin_reply.pin_len = 0;
        memcpy(method.pin_reply.bdaddr.b, pr->bdaddr.b, 6);
    }

    dtun_client_call_method(&method);
}


void dtun_ssp_confirm_reply(bdaddr_t *dba, boolean accepted)
{
    tDTUN_DEVICE_METHOD method;

    info("#### dtun_ssp_confirm_reply() accepted = %d ####\n", (int)accepted);

    method.ssp_confirm.hdr.id = DTUN_METHOD_DM_SSP_CONFIRM;
    method.ssp_confirm.hdr.len = sizeof(tDTUN_METHOD_SSP_CONFIRM) - sizeof(tDTUN_HDR);
    memcpy(method.ssp_confirm.bd_addr.b, dba, 6);
    method.ssp_confirm.accepted = accepted;

    dtun_client_call_method(&method);
}


void hcid_termination_handler (int sig, siginfo_t *siginfo, void *context)
{
    info ("## bluetoothd terminate (%d) ##\n", sig);

    /* stopped from init process */
    if  ((sig == SIGTERM) || (sig == SIGINT))
    {
        /* make sure we started yet */
        if (event_loop)
        {        
           g_main_loop_quit(event_loop);
        }
        else
        {
            /* stop any connection attempts */
            dtun_client_stop(DTUN_INTERFACE);
            exit(0);
        }
    }
}


int hcid_register_termination_handler (void)
{
    struct sigaction act;

    memset (&act, '\0', sizeof(act));

    /* Use the sa_sigaction field because the handles has two additional parameters */
    act.sa_sigaction = &hcid_termination_handler;

    /* The SA_SIGINFO flag tells sigaction() to use the sa_sigaction field, not sa_handler. */
    act.sa_flags = SA_SIGINFO;

    if (sigaction(SIGTERM, &act, NULL) < 0) {
        error ("sigaction : %s (%d)", strerror(errno), errno);
        return 1;
    }

    if (sigaction(SIGINT, &act, NULL) < 0) {
        error ("sigaction : %s (%d)", strerror(errno), errno);
        return 1;
    }

    return 0;
}


#if 1 // KJH 20091204 :: off/of issue fix (wenbin)
int property_is_active(char *property)
{
#define PROPERTY_VALUE_MAX  92

    char value[PROPERTY_VALUE_MAX];

    /* default if not set it 0 */
    property_get(property, value, "0");

    LOGI("property_is_active : %s=%s\n", property, value);

    if (strcmp(value, "1") == 0) {
        return 1;
    }
    return 0;
}
#endif


void dtun_client_main(void)
{
    int retval;

    info("HCID DTUN client starting\n");

    hcid_register_termination_handler();

    /* start dbus tunnel on DTUN subsystem */
    dtun_start_interface(DTUN_INTERFACE, dtun_signal_tbl, dtun_process_started);

    sig_connection = dbus_bus_get( DBUS_BUS_SYSTEM, NULL );

    agent_init();
    retval = hcid_dbus_init();

    dtun_client_call_id_only(DTUN_METHOD_DM_GET_LOCAL_INFO);

    event_loop = g_main_loop_new(NULL, false);

    info("hcid main loop starting\n");

    retval = property_set(DTUN_PROPERTY_HCID_ACTIVE, "1");

    if (retval)
       info("property set failed(%d)\n", retval);

    g_main_loop_run(event_loop);

    info("main loop exiting\n");
    adapter_stop( adapter );

    obex_dbus_exit ();	

    retval = property_set(DTUN_PROPERTY_HCID_ACTIVE, "0");

    if (retval)
       info("property set failed(%d)\n", retval);

#if 1 // KJH 20091204 :: off/of issue fix (wenbin)
    /* fixme -- for some reason the property_set writes sometimes fails
       although the return code says it was ok. Below code is a
       used to ensure we don't exit until we actually see that the
       property was changed */
    while (property_is_active(DTUN_PROPERTY_HCID_ACTIVE))
    {
        info("hcid property write failed...retrying\n");
        usleep(200000);
        property_set(DTUN_PROPERTY_HCID_ACTIVE, "0");
    }
#endif

    info("hcid main loop exiting\n");

    /* stop dbus tunnel */
    dtun_client_stop(DTUN_INTERFACE);

    hcid_dbus_unregister();

    hcid_dbus_exit();

    g_main_loop_unref(event_loop);

}
