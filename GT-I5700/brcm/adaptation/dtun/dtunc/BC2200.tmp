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
/**** NOTE: For eclair, DO NOT USE THIS FILE. PLEASE UPDATE dtun_hcd.c in dtun/dtunc_bz4 *******/
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
#include "dbus-error.h"
#include "dbus-service.h"
#include "dbus-security.h"
#include "agent.h"
#include "dbus-hci.h"

#include "textfile.h"
#include "error.h"
#include "dtun_device.h"

#include "dtun_clnt.h"
#include "btld_prop.h"

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
//	bdaddr_t peer_addr;
	unsigned int cb_id;
	uint8_t state;
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
static struct adapter *adapter;
DBusConnection *connection;


#define FAKE_PATH		"/org/bluez/hci0"

DBusConnection*sig_connection = NULL;

DBusMessage *dtun_am_creat_dev_msg = NULL;
struct audio_device *dtun_saved_pdev = NULL;
struct pending_get_scn *dtun_pending_get_scn = NULL;
struct pending_request *dtun_pending_get_services = NULL;


static    bdaddr_t sba = { {0x11, 0x22, 0x33, 0x44, 0x55, 0x66} };

extern void pin_code_request(int dev, bdaddr_t *sba, bdaddr_t *dba);
extern void link_key_notify(int dev, bdaddr_t *sba, void *ptr);



#define PRINTFUNC() debug("\t\t%s()\n", __FUNCTION__);

#define AUDIO_MANAGER_PATH "/org/bluez/audio"
#define AUDIO_MANAGER_INTERFACE "org.bluez.audio.Manager"
#define AUDIO_SINK_INTERFACE "org.bluez.audio.Sink"


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

const char *dtunc_uuid_table[] =
{
	NULL,
	SERIAL_PORT_UUID,
	DIALUP_NET_UUID,
	FAX_UUID,           /* Fax profile. */
	LAP_UUID,          /* LAN access profile. */
	HSP_AG_UUID,          /* Headset profile. */
	HFP_AG_UUID,          /* Hands-free profile. */
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

const char *pbap_uuids[] = { PBAP_UUID, NULL };
const char *opp_uuids[] = { OPP_UUID, NULL };
const char *ftp_uuids[] = { FTP_UUID, NULL }; 

boolean dtun_auth_on = false;
tDTUN_SIG_DM_AUTHORIZE_REQ_INFO dtun_cur_authorize_req_info;


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

void dtun_add_devices(void)
{
    char filename[PATH_MAX + 1];

    PRINTFUNC();

    create_name(filename, PATH_MAX, STORAGEDIR, adapter->address,
            "linkkeys");

    textfile_foreach(filename, dtun_add_devices_do_append, NULL);

}

int dtun_dbus_start_device(uint16_t id)
{
    char path[MAX_PATH_LENGTH];
    const char *mode;

    PRINTFUNC();

    snprintf(path, sizeof(path), "%s/hci%d", BASE_PATH, id);

    adapter = manager_find_adapter_by_path(path);

    if (!adapter) {
        error("Getting %s path data failed!", path);
        return -1;
    }

    adapter->up = 1;
    adapter->scan_enable = 0;
    adapter->discov_timeout = 120;
    adapter->mode = MODE_OFF;

    /*
     * Get the adapter Bluetooth address
     */
    ba2str(&sba, adapter->address);

    info( "dbus start device path:  %s", path );

    mode = mode2str(adapter->mode);

    if (!g_dbus_emit_signal(sig_connection, adapter->path, ADAPTER_INTERFACE,
                    "ModeChanged",
                    DBUS_TYPE_STRING, &mode,
                    DBUS_TYPE_INVALID)) {
        error("dtun_dbus_start_device: g_dbus_emit_signal");
    }


    if (manager_get_default_adapter() < 0)
        manager_set_default_adapter(id);


    return 0;
}


void dtun_dm_sig_local_info(tDTUN_DEVICE_SIGNAL *msg)
{
    tDTUN_DEVICE_METHOD method;
    char str[DTUN_MAX_DEV_NAME_LEN];
    char *str_ptr = str;
    int retval;

    PRINTFUNC();

    memcpy( &sba, msg->local_info.bdaddr.b, 6 );

    retval = read_local_name(&sba, str);

    /* the Customer needs to modify this to put the customized phone name */
    if( retval < 0 )
        strcpy( str, DTUN_DEFAULT_DEV_NAME );

    method.set_name.hdr.id = DTUN_METHOD_DM_SET_NAME;
    method.set_name.hdr.len = DTUN_MAX_DEV_NAME_LEN;
    strncpy( method.set_name.name, str, DTUN_MAX_DEV_NAME_LEN);

    dtun_client_call_method(&method);
    register_service("pbap", pbap_uuids);
    register_service("opp", opp_uuids);
    register_service("ftp", ftp_uuids);
    dtun_dbus_start_device(0);

    dtun_add_devices();

    register_devices_stored( adapter->address );
}

void dtun_dm_sig_discovery_started(tDTUN_DEVICE_SIGNAL *msg)
{
    /* send start discovery started signal in dbus srv */
    PRINTFUNC();

    adapter->discov_active = 1;

    if (!g_dbus_emit_signal(sig_connection, adapter->path,
                            ADAPTER_INTERFACE,
                            "DiscoveryStarted",
                            DBUS_TYPE_INVALID))  {
        error("dtun_dm_sig_discovery_started: g_dbus_emit_signal");
    }
}

/* send  discovery complete signal in dbus srv */
void dtun_dm_sig_discovery_complete(tDTUN_DEVICE_SIGNAL *msg)
{
    PRINTFUNC();

    adapter->discov_active = 0;
	g_dbus_remove_watch(sig_connection, adapter->discov_listener);
    adapter->discov_listener = 0;
    g_free(adapter->discov_requestor);
    adapter->discov_requestor = NULL;

    adapter->discov_type &= ~(RESOLVE_NAME & STD_INQUIRY);

    if (!g_dbus_emit_signal(sig_connection, adapter->path,
                    ADAPTER_INTERFACE,
                    "DiscoveryCompleted",
                    DBUS_TYPE_INVALID)) {
        error("dtun_dm_sig_discovery_complete: g_dbus_emit_signal");
    }
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
      int i, mask, char_len;
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
	int len = strlen(src);
	if(len > DTUN_MAX_DEV_NAME_LEN)
		len = DTUN_MAX_DEV_NAME_LEN; 
	gboolean bValidUtf8 = utf8_validate(src, 0, len);
	int i;
	for(i = 0; i < len; i++)
	{
		if(src[i] <= 31 || src[i] == 127 || (!bValidUtf8 && (unsigned char)src[i] >= 128))
			dest[i] = '.';
		else dest[i] = src[i];
	}
	dest[i] = 0;
}




//#define DTUN_MAX_DEV_NAME 24

void dtun_dm_sig_rmt_name(tDTUN_DEVICE_SIGNAL *msg)
{
    char peer_addr[18], name[DTUN_MAX_DEV_NAME_LEN+1];
    char *pname = name;
    const char *paddr = peer_addr;

    PRINTFUNC();

    ba2str(&msg->rmt_name.info.bd_addr, peer_addr);
    //strncpy( name, msg->rmt_name.info.bd_name, DTUN_MAX_DEV_NAME_LEN );
    //name[DTUN_MAX_DEV_NAME_LEN] = 0;
    copy_device_name(name, msg->rmt_name.info.bd_name);

    write_device_name(&sba, &msg->rmt_name.info.bd_addr, name);
    info(" Rmt Name Upd: %s, %s\n", peer_addr, name );

    /* send start device found signal in dbus srv */
    if (!g_dbus_emit_signal(sig_connection, adapter->path,
                            ADAPTER_INTERFACE,
                            "RemoteNameUpdated",
                            DBUS_TYPE_STRING, &paddr,
                            DBUS_TYPE_STRING, &pname,
                            DBUS_TYPE_INVALID))  {
         error("dtun_dm_sig_rmt_name: g_dbus_emit_signal");
     }
}

void dtun_dm_sig_device_found(tDTUN_DEVICE_SIGNAL *msg)
{
    char peer_addr[18], name[64];
    const char *paddr = peer_addr;
    const char *pname = name;
    unsigned short rssi = msg->device_found.info.rssi;
    uint32_t class = msg->device_found.info.cod;

    PRINTFUNC();

    ba2str(&msg->device_found.info.bd, peer_addr);

    /* send start device found signal in dbus srv */
    info("\t*** Found device [%s] class = [%d] ***\n\n", peer_addr, class);
    // Henry : if class is zero, we don't display
    if(class != 0)
    {
      write_remote_class(&sba, &msg->device_found.info.bd, class);
    }
    else
    {
      info("henry : class is zero. we don't display");
      return;
    }

    /* send the device found signal */
    if (!g_dbus_emit_signal(sig_connection, adapter->path,
                    ADAPTER_INTERFACE,
                    "RemoteDeviceFound",
                    DBUS_TYPE_STRING, &paddr,
                    DBUS_TYPE_UINT32, &class,
                    DBUS_TYPE_INT16, &rssi,
                    DBUS_TYPE_INVALID))  {
        error("dtun_dm_sig_device_found: g_dbus_emit_signal");
    }
}

void dtun_dm_sig_pin_req(tDTUN_DEVICE_SIGNAL *msg)
{
    PRINTFUNC();
    write_remote_class(&sba, &msg->pin_req.info.bdaddr, msg->pin_req.info.cod);

    pin_code_request( 0, &sba, msg->pin_req.info.bdaddr.b );
}

static void dtun_auth_cb(DBusError *derr, void *user_data)
{
    tDTUN_DEVICE_METHOD method;
    int auth_code ;
    struct service *service;
    char peer_addr[18];


    PRINTFUNC();

    if( user_data != NULL )
    {
          auth_code = *( (int *)user_data) ;
    }
    else if( derr != NULL )
    {
          auth_code = 0;
    }
    else
    {
	   auth_code = 3;
    }

    memcpy( &method.authorize_rsp.info.bd_addr, &dtun_cur_authorize_req_info.bd_addr, 6);
    method.authorize_rsp.info.service = dtun_cur_authorize_req_info.service;

    /* set default response state to not allow AUTH */
    method.authorize_rsp.info.response = 2;

	if (derr && dbus_error_is_set(derr)) {
		method.authorize_rsp.info.response = 2;
		if (dbus_error_has_name(derr, DBUS_ERROR_NO_REPLY)) {
			debug("Canceling authorization request");
			service_cancel_auth(&sba, &dtun_cur_authorize_req_info.bd_addr);
		}
	}
	else {
		service = search_service_by_uuid(dtunc_uuid_table[dtun_cur_authorize_req_info.service]);
                fprintf(stderr, "\n\ndtun_auth_cb  %p\n\n", service);
		if( service )
		{
		       ba2str( &dtun_cur_authorize_req_info.bd_addr, peer_addr );
			switch( auth_code )
			{
			case 0:
				method.authorize_rsp.info.response = 2;
				write_trust(BDADDR_ANY, peer_addr, service->ident, FALSE);
				break;
			case 1:
				method.authorize_rsp.info.response = 2;
				debug( "TODO: Add it to blocked list" );
				write_trust(BDADDR_ANY, peer_addr, service->ident, FALSE);
				break;
			case 2:
				method.authorize_rsp.info.response = 1;
				write_trust(BDADDR_ANY, peer_addr, service->ident, FALSE);
				break;
			case 3:
				method.authorize_rsp.info.response = 1;
				write_trust(BDADDR_ANY, peer_addr, service->ident, TRUE);
				break;
			}
		}
	}
    method.authorize_rsp.hdr.id = DTUN_METHOD_DM_AUTHORIZE_RSP;
    method.authorize_rsp.hdr.len = sizeof(tDTUN_METHOD_DM_AUTHORIZE_RSP_INFO);
    dtun_client_call_method(&method);

	dtun_auth_on = false;
}


void dtun_dm_sig_authorize_req(tDTUN_DEVICE_SIGNAL *msg)
{
	int err;
	DBusError derr;
        PRINTFUNC();
       write_remote_class(&sba, &msg->authorize_req.info.bd_addr, msg->authorize_req.info.cod);

        /* in the case were dtun_auth_cb gets called from here, this needs to be intialized */
        dbus_error_init(&derr);

	if( dtun_auth_on )
	{
		err = -1;

	}
	else
	{
		dtun_auth_on = true;
		memcpy( &dtun_cur_authorize_req_info, &msg->authorize_req.info,
			sizeof( tDTUN_SIG_DM_AUTHORIZE_REQ_INFO)  );

		err = service_req_auth(&sba, &msg->authorize_req.info.bd_addr,
			dtunc_uuid_table[msg->authorize_req.info.service], dtun_auth_cb,
					NULL);
	}
	if (err < 0) {
		debug("Authorization denied: %s", strerror(-err));
		dtun_auth_cb(&derr, NULL);

		return;
	}
}

void dtun_proc_disc( bdaddr_t *bd_addr, uint8_t reason )
{

	DBusMessage *reply;
	char peer_addr[18];
	const char *paddr = peer_addr;
	struct pending_auth_info *auth;

	ba2str(bd_addr, peer_addr);

	cancel_passkey_agent_requests(adapter->passkey_agents, adapter->path,
					bd_addr);
	release_passkey_agents(adapter, bd_addr);

	auth = adapter_find_auth_request(adapter, bd_addr);

	if (auth && auth->agent)
		agent_cancel(auth->agent);

	adapter_remove_auth_request(adapter, bd_addr);

	if( dtun_auth_on && !memcmp( &dtun_cur_authorize_req_info.bd_addr, bd_addr, 6) )
	{
		service_cancel_auth(&sba, bd_addr);
		dtun_auth_on = false;
	}

	/* Check if there is a pending CreateBonding request */
	if (adapter->bonding && (bacmp(&adapter->bonding->bdaddr, bd_addr) == 0)) {
		if (adapter->bonding->cancel) {
			/* reply authentication canceled */
			error_authentication_canceled(adapter->bonding->conn,
							adapter->bonding->msg);
		} else {
			reply = new_authentication_return(adapter->bonding->msg,
							HCI_AUTHENTICATION_FAILURE);
			dbus_connection_send(adapter->bonding->conn, reply, NULL);
			dbus_message_unref(reply);
		}

//		g_dbus_remove_watch(adapter->bonding->conn,
//					adapter->bonding->listener_id);

		bonding_request_free(adapter->bonding);
		adapter->bonding = NULL;
	}


}

void dtun_dm_sig_link_down(tDTUN_DEVICE_SIGNAL *msg)
{
    PRINTFUNC();
    dtun_proc_disc( &msg->link_down.info.bd_addr, &msg->link_down.info.reason );
}

void dtun_dm_sig_link_up(tDTUN_DEVICE_SIGNAL *msg)
{
    char peer_addr[18];
   const char *paddr = peer_addr;

    PRINTFUNC();

    ba2str(&msg->link_up.info.bd_addr, peer_addr);

		g_dbus_emit_signal(sig_connection, adapter->path,
						ADAPTER_INTERFACE,
						"RemoteDeviceConnected",
						DBUS_TYPE_STRING, &paddr,
						DBUS_TYPE_INVALID);

}

void dtun_dm_sig_auth_comp(tDTUN_DEVICE_SIGNAL *msg)
{
    uint8_t st;
    evt_link_key_notify lk_ev;

    PRINTFUNC();

    info("dtun_dm_sig_auth_comp status: %d", msg->auth_comp.info.success );

    if( msg->auth_comp.info.key_present )
    {
        memcpy( lk_ev.bdaddr.b, msg->auth_comp.info.bd_addr.b, 6 );
        memcpy( lk_ev.link_key, msg->auth_comp.info.key, 16 );
        lk_ev.key_type = msg->auth_comp.info.key_type;
        link_key_notify( 0, &sba, &lk_ev );
    }

    st = msg->auth_comp.info.success;

    hcid_dbus_bonding_process_complete(&sba, &msg->auth_comp.info.bd_addr, st);

}

static void dtun_dm_sig_ssp_cfm_req(tDTUN_DEVICE_SIGNAL *msg)
{
    unsigned long ssp_pin;
    unsigned long ssp_mode;

    PRINTFUNC();

    write_remote_class(&sba, &msg->ssp_cfm_req.info.bd_addr, msg->ssp_cfm_req.info.cod);
	

    ssp_pin = msg->ssp_cfm_req.info.num_value;
    if(msg->ssp_cfm_req.info.just_work == true)
        ssp_mode = 2;     /* Java side is 2 for just work */
    else
        ssp_mode = 1;

    ssp_confirm_request( 0, &sba, &msg->ssp_cfm_req.info.bd_addr, ssp_mode, ssp_pin);

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
DBusMessage *dtun_client_get_remote_svc_channel(const char *dst, uint16_t uuid16, DBusConnection *conn, DBusMessage *msg, void *data)
{
    DBusMessage *retval = NULL;
    tDTUN_DEVICE_METHOD method;

    /* Store conn and msg for sending response later */
    dtun_pending_get_scn = g_new0(struct pending_get_scn, 1);
    dtun_pending_get_scn->msg = dbus_message_ref(msg);
    dtun_pending_get_scn->conn = dbus_connection_ref(conn);
    dtun_pending_get_scn->uuid16 = uuid16;

    /* Send message to btld */
    method.get_scn.hdr.id = DTUN_METHOD_DM_GET_REMOTE_SERVICE_CHANNEL;
    method.get_scn.hdr.len = (sizeof(tDTUN_GET_SCN) - sizeof(tDTUN_HDR));
    method.get_scn.uuid16  =  uuid16;
    str2ba(dst, (bdaddr_t *)method.get_scn.bdaddr.b);
    LOGI("%s: starting discovery on %s (uuid16=0x%04x)", __FUNCTION__, dst, uuid16);
    LOGI("   bdaddr=%02X:%02X:%02X:%02X:%02X:%02X", method.get_scn.bdaddr.b[0], method.get_scn.bdaddr.b[1], method.get_scn.bdaddr.b[2],
                                                        method.get_scn.bdaddr.b[3], method.get_scn.bdaddr.b[4], method.get_scn.bdaddr.b[5]);

    dtun_client_call_method(&method);

    return NULL;
}


void dtun_dm_sig_rmt_service_channel(tDTUN_DEVICE_SIGNAL *msg)
{
	DBusMessage *reply;
	uint32_t channel;

    LOGI("%s: success=%i, service=%08X", __FUNCTION__, msg->rmt_scn.success, (unsigned int)msg->rmt_scn.services);

	reply = dbus_message_new_method_return(dtun_pending_get_scn->msg);

    if (dtun_pending_get_scn == NULL)
    {
        LOGI("%s: callback cancelled", __FUNCTION__);
        return;
}

	if ((msg->rmt_scn.success) && (msg->rmt_scn.services))
	{
	    /* BTA does not expose SCNs, use UUID instead of channel */
	    channel = dtun_pending_get_scn->uuid16;
	}
	else
	{
	    /* discovery unsuccessful */
	    channel = -1;
	}

	dbus_message_append_args(reply, DBUS_TYPE_INT32, &channel, DBUS_TYPE_INVALID);
	dbus_connection_send(dtun_pending_get_scn->conn, reply, NULL);
	dbus_message_unref(reply);

	dbus_connection_unref(dtun_pending_get_scn->conn);
	dbus_message_unref(dtun_pending_get_scn->msg);

	g_free(dtun_pending_get_scn);
	dtun_pending_get_scn = NULL;
}

/*******************************************************************************
** GetRemoteServices handler and callback 
*******************************************************************************/
DBusMessage *dtun_client_get_remote_services(const char *dst, DBusConnection *conn, DBusMessage *msg, void *data)
{
    DBusMessage *retval = NULL;
    tDTUN_DEVICE_METHOD method;

    /* Store conn and msg for sending response later */
    dtun_pending_get_services = g_new0(struct pending_request, 1);
    dtun_pending_get_services->msg = dbus_message_ref(msg);
    dtun_pending_get_services->conn = dbus_connection_ref(conn);
        
    /* Send message to btld */
    method.rmt_dev.hdr.id = DTUN_METHOD_DM_GET_REMOTE_SERVICES;
    method.rmt_dev.hdr.len = (sizeof(tDTUN_METHOD_RMT_DEV) - sizeof(tDTUN_HDR));
    str2ba(dst, (bdaddr_t *)method.rmt_dev.bdaddr.b);
    LOGI("%s: Get remote services on %s", __FUNCTION__, dst);
        
    dtun_client_call_method(&method);
       
    return NULL;
}


DBusMessage *dtun_client_get_all_remote_services(const char *dst, DBusConnection *conn, DBusMessage *msg, void *data)
{
    DBusMessage *retval = NULL;
    tDTUN_DEVICE_METHOD method;

    /* Store conn and msg for sending response later */
    dtun_pending_get_services = g_new0(struct pending_request, 1);
    dtun_pending_get_services->msg = dbus_message_ref(msg);
    dtun_pending_get_services->conn = dbus_connection_ref(conn);
        
    /* Send message to btld */
    method.rmt_dev.hdr.id = DTUN_METHOD_DM_GET_ALL_REMOTE_SERVICES;
    method.rmt_dev.hdr.len = (sizeof(tDTUN_METHOD_RMT_DEV) - sizeof(tDTUN_HDR));
    str2ba(dst, (bdaddr_t *)method.rmt_dev.bdaddr.b);
    LOGI("%s: Get remote services on %s", __FUNCTION__, dst);
        
    dtun_client_call_method(&method);
       
    return NULL;
}

void dtun_dm_sig_rmt_services(tDTUN_DEVICE_SIGNAL *msg)
{
	DBusMessage *reply;
	uint32_t service_mask;
	boolean success;

	
    LOGI("%s: success=%i, service=%08X", __FUNCTION__, msg->rmt_services.success, (unsigned int)msg->rmt_services.services);

    if (dtun_pending_get_services == NULL)
    {
        LOGI("%s: callback cancelled", __FUNCTION__);
        return;
    }
    
    service_mask = (uint32_t)msg->rmt_services.services;
    success = msg->rmt_services.success;
	reply = dbus_message_new_method_return(dtun_pending_get_services->msg);
	
	dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INT32, &service_mask, DBUS_TYPE_INVALID);
	dbus_connection_send(dtun_pending_get_services->conn, reply, NULL);
	dbus_message_unref(reply);

	dbus_connection_unref(dtun_pending_get_services->conn);
	dbus_message_unref(dtun_pending_get_services->msg);
	
	g_free(dtun_pending_get_services);
	dtun_pending_get_services = NULL;
}

static struct audio_device *create_device(const bdaddr_t *bda)
{
	static int device_id = 0;
	char path[128];

	snprintf(path, sizeof(path) - 1,
			"%s/device%d", AUDIO_MANAGER_PATH, device_id++);

	return device_register(sig_connection, path, bda, &sba);
}

static void destroy_device(struct audio_device *device)
{
	g_dbus_unregister_all_interfaces(sig_connection, device->path);
}

static void remove_device(struct audio_device *device)
{
	if (device == default_dev) {
		debug("Removing default device");
		default_dev = NULL;
	}

	if (device == default_hs) {
		debug("Removing default headset");
		default_hs = NULL;
	}

	devices = g_slist_remove(devices, device);

	destroy_device(device);
}

static gboolean add_device(struct audio_device *device, gboolean send_signals)
{
	if (!send_signals)
		goto add;

	g_dbus_emit_signal(sig_connection, AUDIO_MANAGER_PATH,
					AUDIO_MANAGER_INTERFACE,
					"DeviceCreated",
					DBUS_TYPE_STRING, &device->path,
					DBUS_TYPE_INVALID);

	if (device->headset)
		g_dbus_emit_signal(sig_connection,
				AUDIO_MANAGER_PATH,
				AUDIO_MANAGER_INTERFACE,
				"HeadsetCreated",
				DBUS_TYPE_STRING, &device->path,
				DBUS_TYPE_INVALID);
add:

	if (default_dev == NULL && g_slist_length(devices) == 0) {
		debug("Selecting default device");
		default_dev = device;
	}

	if (!default_hs && device->headset && !devices)
		default_hs = device;

	devices = g_slist_append(devices, device);

	return TRUE;
}

static void pending_request_free(struct pending_request *pending)
{
	if (pending->conn)
		dbus_connection_unref(pending->conn);
	if (pending->msg)
		dbus_message_unref(pending->msg);
	g_free(pending);
}


static DBusMessage *dtun_sink_connect(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct audio_device *dev = data;
	struct sink *sink = dev->sink;
	struct pending_request *pending;
	tDTUN_DEVICE_METHOD method;
	const char *address;
	DBusMessage *reply;


    PRINTFUNC();

	if (sink->connect || sink->disconnect)
		return g_dbus_create_error(msg, ERROR_INTERFACE ".Failed",
						"%s", strerror(EBUSY));

	if (sink->state >= AVDTP_STATE_OPEN)
	{
	/*
		return g_dbus_create_error(msg, ERROR_INTERFACE
						".AlreadyConnected",
						"Device Already Connected");
	*/

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

static DBusMessage *dtun_sink_disconnect(DBusConnection *conn,
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


static DBusMessage *dtun_sink_is_connected(DBusConnection *conn,
					DBusMessage *msg,
					void *data)
{
	struct audio_device *dev = data;
	struct sink *sink = dev->sink;
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


static GDBusMethodTable dtun_sink_methods[] = {
	{ "Connect",		"",	"",	dtun_sink_connect,
						G_DBUS_METHOD_FLAG_ASYNC },
	{ "Disconnect",		"",	"",	dtun_sink_disconnect,
						G_DBUS_METHOD_FLAG_ASYNC },
	{ "IsConnected",	"",	"b",	dtun_sink_is_connected },
	{ NULL, NULL, NULL, NULL }
};

static GDBusSignalTable dtun_sink_signals[] = {
	{ "Connected",			""	},
	{ "Disconnected",		""	},
	{ "Playing",			""	},
	{ "Stopped",			""	},
	{ NULL, NULL }
};


struct audio_device *find_device(const bdaddr_t *bda )
{
	GSList *l;

	for (l = devices; l != NULL; l = l->next) {
		struct audio_device *dev = l->data;

		if (bacmp(bda, BDADDR_ANY) && bacmp(&dev->dst, bda))
			continue;

		return dev;
	}

	return NULL;
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

    debug( "1. At Start of dtun_am_sig_av_event\n");

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

        pdev = find_device(&msg->av_event.info.peer_addr);

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

        if (orig_msg && err_msg[0])
        {
            error_failed(sig_connection, orig_msg, err_msg);
            //dbus_message_unref( orig_msg );
            dtun_am_creat_dev_msg = NULL;

            if (pending)
                pending_request_free(pending);

            return;
        }

        if (new_bonding)
        {
            pdev = create_device(&msg->av_event.info.peer_addr);
            if (pdev)
            {
                pdev->sink = g_new0(struct sink, 1);
                pdev->sink->state = AVDTP_STATE_OPEN;
            }

            memcpy(dev_path, pdev->path, 32);
            dev_path[31] = 0;

            debug( "Creating Sink Interface\n" );

            if (!pdev || !pdev->sink)
            {
                error_failed(sig_connection, dtun_am_creat_dev_msg,
                        "Out of Memory");
                dbus_message_unref(dtun_am_creat_dev_msg);
                dtun_am_creat_dev_msg = NULL;
                return;
            }

            g_dbus_register_interface(sig_connection, dev_path,
                    AUDIO_SINK_INTERFACE, dtun_sink_methods, dtun_sink_signals,
                    NULL, pdev, NULL);

            device_store(pdev, TRUE);

            if (dtun_am_creat_dev_msg)
            {
                add_device(pdev, false);
                reply = dbus_message_new_method_return(orig_msg);

                if (!reply)
                    return;

                dbus_message_append_args(reply, DBUS_TYPE_STRING, &dpath,
                        DBUS_TYPE_INVALID);
                dbus_connection_send(sig_connection, reply, NULL);
                dbus_message_unref(dtun_am_creat_dev_msg);
                dbus_message_unref(reply);
                dtun_am_creat_dev_msg = NULL;
                printf("Sink Interface Created\n");
            } else
            {
                add_device(pdev, true);
            }
        } else
        {

            pdev->sink->state = AVDTP_STATE_OPEN;
            g_dbus_emit_signal(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                    "Connected", DBUS_TYPE_INVALID);

            if (pending)
            {
                int busywait;
                debug( "Answering Pending Req\n" );
                //busy wait added to separate the 2 dbus transactions
                for (busywait = 0; busywait < 10000; busywait++)
                    ;
                reply = dbus_message_new_method_return(pending->msg);
                if (!reply)
                    return;
                dbus_connection_send(pending->conn, reply, NULL);
                dbus_message_unref(reply);
                pending_request_free(pending);
                pdev->sink->connect = NULL;
            }
            debug("Stream successfully created");
        }
    }  /* event==2 */
    if (msg->av_event.info.event == 3)
    {
        pdev = find_device(&msg->av_event.info.peer_addr);
        if (pdev)
        {
            pdev->sink->state = AVDTP_STATE_IDLE;
            g_dbus_emit_signal(pdev->conn, pdev->path, AUDIO_SINK_INTERFACE,
                    "Disconnected", DBUS_TYPE_INVALID);

            if (pending = pdev->sink->disconnect) //Assignment on purpose
            {

                reply = dbus_message_new_method_return(pending->msg);
                if (!reply)
                    return;
                dbus_connection_send(pending->conn, reply, NULL);
                dbus_message_unref(reply);
                pending_request_free(pending);
                pdev->sink->disconnect = NULL;
                debug("Stream successfully disconnected");
            }
        }
    } /* event==3 */

    if (msg->av_event.info.event == 4)
    {
       pdev = find_device( &msg->av_event.info.peer_addr );
	if( pdev )
	{
		pdev->sink->state = AVDTP_STATE_STREAMING;	
		g_dbus_emit_signal(pdev->conn, pdev->path,
						AUDIO_SINK_INTERFACE,
						"Playing",
						DBUS_TYPE_INVALID);	
    	
        }
    } /* even==4 */
    if(msg->av_event.info.event == 15) 
    {
       pdev = find_device( &msg->av_event.info.peer_addr );
	if( pdev )
	{
		pdev->sink->state = AVDTP_STATE_OPEN;	
		g_dbus_emit_signal(pdev->conn, pdev->path,
						AUDIO_SINK_INTERFACE,
						"Stopped",
						DBUS_TYPE_INVALID);	
    	
        }
    } /* event==15 */
} /* dtun_am_sig_av_event() */




/* signal callback table */
const tDTUN_SIGNAL dm_signal_tbl[] =
{
    /* dm */
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
    /* add here */
    dtun_dm_sig_testmode_state
};


/* signal callback table */
const tDTUN_SIGNAL av_signal_tbl[] =
{
    dtun_am_sig_av_event,

    /* add here */
};

/*----------------------Audio Section-------------------------*/

static DBusMessage *dtun_am_create_device(DBusConnection *conn,
					DBusMessage *msg,
					void *data)
{
	tDTUN_DEVICE_METHOD method;


	const char *address;

    PRINTFUNC();

	if (!dbus_message_get_args(msg, NULL,
				DBUS_TYPE_STRING, &address,
				DBUS_TYPE_INVALID))
		return NULL;

	str2ba(address, (bdaddr_t *)&method.av_open.bdaddr.b);
 	dtun_am_creat_dev_msg = dbus_message_ref(msg);
       method.av_open.hdr.id = DTUN_METHOD_AM_AV_OPEN;
       method.av_open.hdr.len = 6;
	dtun_client_call_method(&method);

	return NULL;
}

static DBusMessage *dtun_am_remove_device(DBusConnection *conn,
					DBusMessage *msg,
					void *data)
{
    PRINTFUNC();
	return NULL;
}

static DBusMessage *dtun_am_list_devices(DBusConnection *conn,
					DBusMessage *msg,
					void *data)
{
	DBusMessageIter iter, array_iter;
	DBusMessage *reply;
	DBusError derr;
	GSList *l;
	gboolean hs_only = FALSE;

	dbus_error_init(&derr);

#if 0
	if (dbus_message_is_method_call(msg, AUDIO_MANAGER_INTERFACE,
					"ListHeadsets"))
		hs_only = TRUE;
	else
		hs_only = FALSE;
#endif

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
				DBUS_TYPE_STRING_AS_STRING, &array_iter);

	for (l = devices; l != NULL; l = l->next) {
		struct audio_device *device = l->data;

		if (hs_only && !device->headset)
			continue;

		dbus_message_iter_append_basic(&array_iter,
						DBUS_TYPE_STRING, &device->path);
	}

	dbus_message_iter_close_container(&iter, &array_iter);

	return reply;
#if 0
	DBusMessage *reply;

	PRINTFUNC();

	reply = dbus_message_new_method_return(msg);
	return reply;
#endif
}


static DBusMessage *dtun_am_default_device(DBusConnection *conn,
					DBusMessage *msg,
					void *data)
{
    PRINTFUNC();
	return NULL;
}

static DBusMessage *dtun_am_change_default_device(DBusConnection *conn,
					DBusMessage *msg,
					void *data)
{
    PRINTFUNC();
	return NULL;
}

static DBusMessage *dtun_am_find_by_addr(DBusConnection *conn,
					DBusMessage *msg,
					void *data)
{
    PRINTFUNC();
	return NULL;
}


static void dtun_am_unregister(void *data)
{
	info("Unregistered manager path");

	dtun_am_creat_dev_msg = NULL;
}

struct sink *sink_init(struct audio_device *dev)
{
	if (!g_dbus_register_interface(dev->conn, dev->path,
					AUDIO_SINK_INTERFACE,
					dtun_sink_methods, dtun_sink_signals, NULL,
					dev, NULL))
	{
		debug( "sink_init failed\n" );
		return NULL;
	}

	return g_new0(struct sink, 1);
}

static void parse_stored_devices(char *key, char *value, void *data)
{
	bdaddr_t *src = data;
	struct audio_device *device;
	bdaddr_t dst;

	if (!key || !value || strcmp(key, "default") == 0)
		return;

	str2ba(key, &dst);
	device = find_device(&dst);

	if (device)
		return;

	info("Loading device %s (%s)", key, value);

	device = create_device(&dst);
	if (!device)
		return;

	/* Change storage to source adapter */
	bacpy(&device->store, src);

	if (strstr(value, "sink"))
		device->sink = sink_init(device);

	add_device(device, FALSE);
}

static void register_devices_stored(const char *adapter)
{
	char filename[PATH_MAX + 1];
	struct audio_device *device;
	bdaddr_t default_src;
	bdaddr_t dst;
	bdaddr_t src;
	char *addr;
	int dev_id;

	create_name(filename, PATH_MAX, STORAGEDIR, adapter, "audio");

	printf( "register_devices_stored %s\n", filename );
	str2ba(adapter, &src);

	textfile_foreach(filename, parse_stored_devices, &src);

	addr = textfile_get(filename, "default");
	if (!addr)
		return;

	str2ba(addr, &dst);
	device = find_device(&dst);

	if (device) {
		info("Setting %s as default device", addr);
		default_hs = device;
	}

	free(addr);
}


static GDBusMethodTable dtun_am_methods[] = {
	{ "CreateDevice",		"s",	"s",	dtun_am_create_device,
							G_DBUS_METHOD_FLAG_ASYNC },
	{ "RemoveDevice",		"s",	"",	dtun_am_remove_device },
	{ "ListDevices",		"",	"as",	dtun_am_list_devices },
	{ "DefaultDevice",		"",	"s",	dtun_am_default_device },
	{ "ChangeDefaultDevice",	"s",	"",	dtun_am_change_default_device },
	{ "CreateHeadset",		"s",	"s",	dtun_am_create_device,
							G_DBUS_METHOD_FLAG_ASYNC },
	{ "RemoveHeadset",		"s",	"",	dtun_am_remove_device },
	{ "ListHeadsets",		"",	"as",	dtun_am_list_devices },
	{ "FindDeviceByAddress",	"s",	"s",	dtun_am_find_by_addr },
	{ "DefaultHeadset",		"",	"s",	dtun_am_default_device },
	{ "ChangeDefaultHeadset",	"s",	"",	dtun_am_change_default_device },
	{ }
};

static GDBusSignalTable dtun_am_signals[] = {
	{ "DeviceCreated",		"s"	},
	{ "DeviceRemoved",		"s"	},
	{ "HeadsetCreated",		"s"	},
	{ "HeadsetRemoved",		"s"	},
	{ "DefaultDeviceChanged",	"s"	},
	{ "DefaultHeadsetChanged",	"s"	},
	{ }
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

    info("\n\n #### dtun_ssp_confirm_reply() accepted = %d ####\n\n", (int)accepted);

    method.ssp_confirm.hdr.id = DTUN_METHOD_DM_SSP_CONFIRM;
    method.ssp_confirm.hdr.len = sizeof(tDTUN_METHOD_SSP_CONFIRM) - sizeof(tDTUN_HDR);
    memcpy(method.ssp_confirm.bd_addr.b, dba, 6);
    method.ssp_confirm.accepted = accepted;

    dtun_client_call_method(&method);
}


void hcid_termination_handler (int sig, siginfo_t *siginfo, void *context)
{
    info ("\n## hcid got signal %d ##\n\n", sig);

    /* stopped from init process */
    if  ((sig == SIGTERM) || (sig == SIGKILL))
    {
        /* make sure we started yet */
        if (event_loop)
        {
           g_main_loop_quit(event_loop);
        }
        else
        {
            /* stop any connection attempts */
            dtun_client_stop(DTUN_INTERFACE_DM);
            dtun_client_stop(DTUN_INTERFACE_AV);
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
        perror ("sigaction");
        return 1;
    }

    if (sigaction(SIGKILL, &act, NULL) < 0) {
        perror ("sigaction");
        return 1;
    }

    return 0;
}


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


static void sig_term(int sig)
{
	g_main_loop_quit(event_loop);
}

void dtun_client_main(void)
{
    int retval;
    struct sigaction sa;

    info("HCID DTUN client starting\n");

    hcid_register_termination_handler();

    /* start dbus tunnel on DTUN DM subsystem */
    dtun_start_interface(DTUN_INTERFACE_DM, dm_signal_tbl, dtun_process_started);

    /* start dbus tunnel on DTUN AV subsystem */
    dtun_start_interface(DTUN_INTERFACE_AV, av_signal_tbl, dtun_process_started);

    sig_connection = dbus_bus_get( DBUS_BUS_SYSTEM, NULL );

    retval = hcid_dbus_init();

    if( retval == 0 )
    {
        if( hcid_dbus_register_device( 0 ) < 0 )
            info( "DBUS Device Reg failed \n" );
    }
    else
    {
        perror( "DBUS init " );
        exit( 1 );
    }

    if (!g_dbus_register_interface(sig_connection, AUDIO_MANAGER_PATH,
					AUDIO_MANAGER_INTERFACE,
					dtun_am_methods, dtun_am_signals,
					NULL, NULL, dtun_am_unregister)) {
		error("Failed to register %s interface to %s",
				AUDIO_MANAGER_PATH, AUDIO_MANAGER_INTERFACE);
		exit(1);
	}

    dtun_client_call_id_only(DTUN_METHOD_DM_GET_LOCAL_INFO);


    event_loop = g_main_loop_new(NULL, false);

    info("hcid main loop starting\n");

    retval = property_set(DTUN_PROPERTY_HCID_ACTIVE, "1");

    if (retval)
       info("property set failed(%d)\n", retval);

#if 0
    /* rl debug start: */
    {
        tDTUN_DEVICE_METHOD method;

        debug("\n\n #### DTUN_METHOD_DM_SET_TESTMODE ####\n\n");

        method.set_testmode.hdr.id  = DTUN_METHOD_DM_SET_TESTMODE;
        method.set_testmode.hdr.len = sizeof(tDTUN_METHOD_DM_SET_TESTMODE) - sizeof(tDTUN_HDR);
        method.set_testmode.mode    = DM_SET_TRACE_LEVEL | (61<<8) | 5;
        dtun_client_call_method( &method );
    }
    /* debug end */
#endif
    g_main_loop_run(event_loop);

    info("main loop exiting\n");
    adapter->up = 0;

    retval = property_set(DTUN_PROPERTY_HCID_ACTIVE, "0");

    if (retval)
       info("property set failed(%d)\n", retval);

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

    info("hcid main loop exiting\n");

    /* stop dbus tunnel */
    dtun_client_stop(DTUN_INTERFACE_DM);
    dtun_client_stop(DTUN_INTERFACE_AV);

    hcid_dbus_unregister();

    hcid_dbus_exit();

    g_main_loop_unref(event_loop);

}
