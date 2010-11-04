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
#include <gdbus.h>
#include <dbus/dbus.h>
#include <bluetooth/bluetooth.h>

#include "dtun_obex.h"
#include "dtun_clnt.h"

/* some local debug macros */
#ifdef DTUN_STANDALONE
#define info(format, ...)  fprintf (stdout, format, ## __VA_ARGS__)
#define debug(format, ...) fprintf (stdout, format, ## __VA_ARGS__)
#define error(format, ...) fprintf (stderr, format, ## __VA_ARGS__)
#else
#define LOG_TAG "dtun_obexd"
#include "utils/Log.h"
#define info(format, ...)  LOGI (format, ## __VA_ARGS__)
#define debug(format, ...) LOGD (format, ## __VA_ARGS__)
#define error(format, ...) LOGE (format, ## __VA_ARGS__)
#endif

static DBusConnection *conn = NULL;

/*******************************************************************************
**
** Function         enableOpcSession
**
** Description      Function to send DTUN enableOpcSession method message
**
*******************************************************************************/
static DBusMessage *enableOpcSession (DBusConnection *conn, DBusMessage *msg, void *data)
{
    debug(__FUNCTION__);

	dtun_client_call_id_only(DTUN_METHOD_OPC_ENABLE);
	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         closeOpcSession
**
** Description      Function to send DTUN closeOpcSession method message
**
*******************************************************************************/
static DBusMessage *closeOpcSession (DBusConnection *conn, DBusMessage *msg, void *data)
{
    debug(__FUNCTION__);

	dtun_client_call_id_only(DTUN_METHOD_OPC_CLOSE);
	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         closeOpsSession
**
** Description      Function to send DTUN closeOpsSession method message
**
*******************************************************************************/
static DBusMessage *closeOpsSession (DBusConnection *conn, DBusMessage *msg, void *data)
{
    debug(__FUNCTION__);

	dtun_client_call_id_only(DTUN_METHOD_OPS_CLOSE);
	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         pushObject
**
** Description      Function to send DTUN pushObject method message
**
*******************************************************************************/
static DBusMessage *pushObject (DBusConnection *conn, DBusMessage *msg, void *data)
{
    tDTUN_DEVICE_METHOD method;
	const char *peer_bd_address, *file_path_name;

	if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_STRING, &peer_bd_address,
        DBUS_TYPE_STRING, &file_path_name,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: BDAddr:%s, file:%s", __FUNCTION__, peer_bd_address, file_path_name);

	method.opc_push_object.hdr.id = DTUN_METHOD_OPC_PUSH_OBJECT;
	method.opc_push_object.hdr.len = sizeof(bdaddr_t) + DTUN_PATH_LEN;
	str2ba(peer_bd_address, (bdaddr_t *)&method.opc_push_object.bdaddr);
    strncpy(method.opc_push_object.file_path_name, file_path_name, DTUN_PATH_LEN);
	dtun_client_call_method(&method);

	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         pullVcard
**
** Description      Function to send DTUN pullVcard method message
**
*******************************************************************************/
static DBusMessage *pullVcard (DBusConnection *conn, DBusMessage *msg, void *data)
{
    tDTUN_DEVICE_METHOD method;
	const char *peer_bd_address;

	if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_STRING, &peer_bd_address,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: BDAddr:%s", __FUNCTION__, peer_bd_address);

	method.opc_pull_vcard.hdr.id = DTUN_METHOD_OPC_PULL_VCARD;
	method.opc_pull_vcard.hdr.len = sizeof(bdaddr_t);
	str2ba(peer_bd_address, (bdaddr_t *)&method.opc_pull_vcard.bdaddr);
	dtun_client_call_method(&method);

	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         exchangeVcard
**
** Description      Function to send DTUN exchangeVcard method message
**
*******************************************************************************/
static DBusMessage *exchangeVcard (DBusConnection *conn, DBusMessage	*msg, void *data) {
    tDTUN_DEVICE_METHOD method;
	const char *peer_bd_address;

	if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_STRING, &peer_bd_address,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: BDAddr:%s", __FUNCTION__, peer_bd_address);

	method.opc_exch_vcard.hdr.id = DTUN_METHOD_OPC_EXCH_VCARD;
	method.opc_exch_vcard.hdr.len = sizeof(bdaddr_t);
	str2ba(peer_bd_address, (bdaddr_t *)&method.opc_exch_vcard.bdaddr);
	dtun_client_call_method(&method);

	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         grantAccess
**
** Description      Function to send DTUN grantAccess method message
**
*******************************************************************************/
static DBusMessage *grantAccess (DBusConnection *conn, DBusMessage *msg, void *data)
{
    tDTUN_DEVICE_METHOD method;
	uint16_t operation, access;
	const char *file_path_name;

     debug( "Obex: grantAccess" );
	 
	if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_UINT16, &operation,
        DBUS_TYPE_UINT16, &access,
        DBUS_TYPE_STRING, &file_path_name,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: operation:%d access:%d file:%s", __FUNCTION__, operation, access, file_path_name);

	method.op_grant_access.hdr.id = DTUN_METHOD_OP_GRANT_ACCESS;
	method.op_grant_access.hdr.len = sizeof(uint8_t) + sizeof(uint8_t) + DTUN_PATH_LEN;
    method.op_grant_access.operation = operation;
    method.op_grant_access.access = access;
    strncpy(method.op_grant_access.file_path_name, file_path_name, DTUN_PATH_LEN);
	dtun_client_call_method(&method);

	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         setOwnerVcard
**
** Description      Function to send DTUN setOwnerVcard method message
**
*******************************************************************************/
static DBusMessage *setOwnerVcard (DBusConnection *conn, DBusMessage *msg, void *data)
{
    tDTUN_DEVICE_METHOD method;
	const char *file_name;

	if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_STRING, &file_name,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: file:%s", __FUNCTION__, file_name);

	method.op_set_owner_vcard.hdr.id = DTUN_METHOD_OP_SET_OWNER_VCARD;
	method.op_set_owner_vcard.hdr.len = DTUN_PATH_LEN;
    strncpy(method.op_set_owner_vcard.file_name, file_name, DTUN_PATH_LEN);
	dtun_client_call_method(&method);

	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         setExchangeFolder
**
** Description      Function to send DTUN setExchangeFolder method message
**
*******************************************************************************/
static DBusMessage *setExchangeFolder (DBusConnection *conn, DBusMessage *msg, void *data)
{
    tDTUN_DEVICE_METHOD method;
	const char *path_name;

	if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_STRING, &path_name,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: path:%s", __FUNCTION__, path_name);

	method.op_set_exchange_folder.hdr.id = DTUN_METHOD_OP_SET_EXCHANGE_FOLDER;
	method.op_set_exchange_folder.hdr.len = DTUN_PATH_LEN;
    strncpy(method.op_set_exchange_folder.path_name, path_name, DTUN_PATH_LEN);
	dtun_client_call_method(&method);

	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         createVcard
**
** Description      Function to send DTUN createVcard method message
**
*******************************************************************************/
static DBusMessage *createVcard (DBusConnection *conn, DBusMessage *msg, void *data)
{
    tDTUN_DEVICE_METHOD method;
	const char *vcard_uri, *file_path_name;

	if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_STRING, &vcard_uri,
        DBUS_TYPE_STRING, &file_path_name,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: uri:%s file:%s", __FUNCTION__, vcard_uri, file_path_name);

	method.op_create_vcard.hdr.id = DTUN_METHOD_OP_CREATE_VCARD;
	method.op_create_vcard.hdr.len = sizeof(method.op_create_vcard.vcard_uri) + DTUN_PATH_LEN;
    strncpy(method.op_create_vcard.vcard_uri, vcard_uri, sizeof(method.op_create_vcard.vcard_uri));
    strncpy(method.op_create_vcard.file_path_name, file_path_name, DTUN_PATH_LEN);
	dtun_client_call_method(&method);

	return dbus_message_new_method_return(msg);
}

/*******************************************************************************
**
** Function         storeVcard
**
** Description      Function to send DTUN storeVcard method message
**
*******************************************************************************/
static DBusMessage *storeVcard (DBusConnection *conn, DBusMessage *msg, void *data)
{
    tDTUN_DEVICE_METHOD method;
	const char *file_path_name;
	uint16_t dup_action;

	if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_STRING, &file_path_name,
        DBUS_TYPE_UINT16, &dup_action,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: file:%s dup_action:%d", __FUNCTION__, file_path_name, dup_action);

	method.op_store_vcard.hdr.id = DTUN_METHOD_OP_STORE_VCARD;
	method.op_store_vcard.hdr.len = DTUN_PATH_LEN + sizeof(uint16_t);
    strncpy(method.op_store_vcard.file_path_name, file_path_name, DTUN_PATH_LEN);
    method.op_store_vcard.dup_action = dup_action;
	dtun_client_call_method(&method);

	return dbus_message_new_method_return(msg);
}


/*******************************************************************************
**
** Function         setDefaultPath
**
** Description      Function to send DTUN setDefaultPath method message
**
*******************************************************************************/
static DBusMessage *setDefaultPath (DBusConnection *conn, DBusMessage *msg, void *data)
{
    tDTUN_DEVICE_METHOD method;
    const char *file_path_name;

    if (!dbus_message_get_args(msg, NULL,
        DBUS_TYPE_STRING, &file_path_name,
        DBUS_TYPE_INVALID))
    {
		return NULL;
    }

    debug("%s: default_path:%s", __FUNCTION__, file_path_name);

    method.op_store_vcard.hdr.id = DTUN_METHOD_OP_SET_DEFAULT_PATH;
    method.op_store_vcard.hdr.len = DTUN_PATH_LEN;
    strncpy(method.op_set_default_path.file_path_name, file_path_name, DTUN_PATH_LEN);
    dtun_client_call_method(&method);

    return dbus_message_new_method_return(msg);
}




/* DTUN OPP Methods */
static GDBusMethodTable dtun_opp_methods[] = {
    { "EnableOpcSession",  "",    "",   enableOpcSession,  0 },
    { "CloseOpcSession",   "",    "",   closeOpcSession,   0 },
    { "CloseOpsSession",   "",    "",   closeOpsSession,   0 },
    { "PushObject",        "ss",  "",   pushObject,        0 },
    { "PullVcard",         "s",   "",   pullVcard,         0 },
    { "ExchangeVcard",     "s",   "",   exchangeVcard,     0 },
    { "GrantAccess",       "qqs", "",   grantAccess,       0 },
    { "SetOwnerVcard",     "s",   "",   setOwnerVcard,     0 },
    { "SetExchangeFolder", "s",   "",   setExchangeFolder, 0 },
    { "CreateVcard",       "ss",   "",  createVcard,       0 },
    { "StoreVcard",        "sq",   "",  storeVcard,        0 },
    { "SetDefaultPath",     "s",   "",   setDefaultPath,    0 },
    { NULL,                NULL,  NULL, NULL,              0 }
};

/* DTUN OPP Signals */
static GDBusSignalTable dtun_opp_signals[] = {
    { "OpcEnable",          "",       0 },
    { "OpcOpen",            "",       0 },
    { "OpcProgress",        "uu",     0 },
    { "OpcObjectReceived",  "qs",     0 },
    { "OpcObjectPushed",    "qs",     0 },
    { "OpcClose",           "q",      0 },

    { "OpsProgress",        "uu",     0 },
    { "OpsObjectReceived",  "qs",     0 },
    { "OpsOpen",            "",       0 },
    { "OpsAccessRequest",   "ssqqsu", 0 },
    { "OpsClose",           "",       0 },

    { "OpCreateVcard",      "qs",     0 },
    { "OpOwnerVcardNotSet", "s",      0 },
    { "OpStoreVcard",       "qs",     0 },
    { "OpSetDefaultPath",   "q",     0 },
};


/*******************************************************************************
**
** Function         dtun_sig_opc_enable
**
** Description     Function to emit dtun_sig_opc_enable
**
*******************************************************************************/
void dtun_sig_opc_enable (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpcEnable",
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_opc_open
**
** Description     Function to emit dtun_sig_opc_open
**
*******************************************************************************/
void dtun_sig_opc_open (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpcOpen",
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_opc_progress
**
** Description     Function to emit dtun_sig_opc_progress
**
*******************************************************************************/
void dtun_sig_opc_progress (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint32_t bytes_transferred = p_data->opc_progress.bytes;
    uint32_t obj_size = p_data->opc_progress.obj_size;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpcProgress",
        DBUS_TYPE_UINT32, &bytes_transferred,
        DBUS_TYPE_UINT32, &obj_size,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_opc_object_received
**
** Description      Function to emit dtun_sig_opc_object_received
**
*******************************************************************************/
void dtun_sig_opc_object_received (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);
    
    uint16_t status = (uint16_t)p_data->opc_object_received.status;
    char *name = p_data->opc_object_received.name;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpcObjectReceived",
        DBUS_TYPE_UINT16, &status,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_opc_object_pushed
**
** Description      Function to emit dtun_sig_opc_object_pushed
**
*******************************************************************************/
void dtun_sig_opc_object_pushed (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->opc_object_pushed.status;
    char *name = p_data->opc_object_pushed.name;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpcObjectPushed",
        DBUS_TYPE_UINT16, &status,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_opc_close
**
** Description      Function to emit dtun_sig_opc_close
**
*******************************************************************************/
void dtun_sig_opc_close (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->opc_close.status;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpcClose",
        DBUS_TYPE_UINT16, &status,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_ops_progress
**
** Description      Function to emit dtun_sig_ops_progress
**
*******************************************************************************/
void dtun_sig_ops_progress (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint32_t bytes_transferred = p_data->ops_progress.bytes;
    uint32_t obj_size = p_data->ops_progress.obj_size;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpsProgress",
        DBUS_TYPE_UINT32, &bytes_transferred,
        DBUS_TYPE_UINT32, &obj_size,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_ops_object_received
**
** Description      Function to emit dtun_sig_ops_object_received
**
*******************************************************************************/
void dtun_sig_ops_object_received (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);
    
    uint16_t format = (uint16_t)p_data->ops_object_received.format;
    char *name = p_data->ops_object_received.name;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpsObjectReceived",
        DBUS_TYPE_UINT16, &format,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_ops_open
**
** Description      Function to emit dtun_sig_ops_open
**
*******************************************************************************/
void dtun_sig_ops_open (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpsOpen",
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_ops_access_request
**
** Description      Function to emit dtun_sig_ops_access_request
**
*******************************************************************************/
void dtun_sig_ops_access_request (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    char *bdname = p_data->ops_access_request.bdname;
    char *bdaddr = p_data->ops_access_request.bdaddr;
    uint16_t oper = (uint16_t)p_data->ops_access_request.oper;
    uint16_t format = (uint16_t)p_data->ops_access_request.format;
    char *name = p_data->ops_access_request.name;
    uint32_t obj_size = p_data->ops_access_request.obj_size;

    debug("AccessReq: peer:%s, addr:%s, oper:%d, format:%d, file:%s, size:%d",
        bdname, bdaddr, oper, format, name, obj_size);

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpsAccessRequest",
        DBUS_TYPE_STRING, &bdname,
        DBUS_TYPE_STRING, &bdaddr,
        DBUS_TYPE_UINT16, &oper,
        DBUS_TYPE_UINT16, &format,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_UINT32, &obj_size,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_ops_close
**
** Description      Function to emit dtun_sig_ops_close
**
*******************************************************************************/
void dtun_sig_ops_close (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpsClose",
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_op_create_vcard
**
** Description      Function to emit dtun_sig_op_create_vcard
**
*******************************************************************************/
void dtun_sig_op_create_vcard (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->op_create_vcard.status;
    char *name = p_data->op_create_vcard.name;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpCreateVcard",
        DBUS_TYPE_UINT16, &status,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_op_owner_vcard_not_set
**
** Description      Function to emit dtun_sig_op_owner_vcard_not_set
**
*******************************************************************************/
void dtun_sig_op_owner_vcard_not_set (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    char *name = p_data->op_owner_vcard_not_set.name;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpOwnerVcardNotSet",
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         dtun_sig_op_store_vcard
**
** Description      Function to emit dtun_sig_op_store_vcard
**
*******************************************************************************/
void dtun_sig_op_store_vcard (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->op_store_vcard.status;
    char *name = p_data->op_store_vcard.name;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpStoreVcard",
        DBUS_TYPE_UINT16, &status,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}


/*******************************************************************************
**
** Function         dtun_sig_op_set_default_path
**
** Description      Function to emit dtun_sig_op_set_default_path
**
*******************************************************************************/
void dtun_sig_op_set_default_path (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->op_sig_set_default_path.status;

    if (!g_dbus_emit_signal(
        conn, BTLA_OBEX_PATH,
        BTLA_OBEX_INTERFACE, "OpSetDefaultPath",
        DBUS_TYPE_UINT16, &status,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/*******************************************************************************
**
** Function         obex_dbus_init
**
** Description     Init function of obexd module
**
*******************************************************************************/

static void dtun_obex_process_started (void) 
{
    debug("Obex interface Initialized");
}

int obex_dbus_init (DBusConnection *in_conn)
{

	conn = dbus_connection_ref( in_conn );
       debug( "Obex: g_dbus_register_interface  in_conn = %p conn=%p", in_conn, conn );
	if (!g_dbus_register_interface(in_conn, BTLA_OBEX_PATH, BTLA_OBEX_INTERFACE,
			dtun_opp_methods, dtun_opp_signals, NULL, NULL, NULL)) {
		return -1;
    }
    //dtun_start_interface(DTUN_INTERFACE_OBEX, obex_signal_tbl, dtun_obex_process_started);
	
    return 0;
}

/*******************************************************************************
**
** Function         obex_dbus_exit
**
** Description     Exit function of obexd module
**
*******************************************************************************/
void obex_dbus_exit (void)
{
    debug("Exit Obex interface");

   // dtun_client_stop(DTUN_INTERFACE_OBEX);

    if (!conn || !dbus_connection_get_is_connected(conn)) {
    	return;
    }

	g_dbus_unregister_interface(conn, BTLA_OBEX_PATH, BTLA_OBEX_INTERFACE);
    conn = NULL;

}

