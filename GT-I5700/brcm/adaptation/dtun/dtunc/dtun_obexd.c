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
#include <cutils/properties.h>

#include "obex.h"
#include "dtun_clnt.h"
#include "btld_prop.h"

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

static GMainLoop *event_loop;
static DBusConnection *sig_connection = NULL;

/*******************************************************************************
**
** Function         set_dbus_sig_connection
**
** Description      Set Obex D-Bus signal connection
**
*******************************************************************************/
void set_dbus_sig_connection (DBusConnection *conn) {
	sig_connection = conn;
}

/*******************************************************************************
**
** Function         dtun_sig_opc_enable
**
** Description      Function to emit dtun_sig_opc_enable
**
*******************************************************************************/
static void dtun_sig_opc_enable (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
** Description      Function to emit dtun_sig_opc_open
**
*******************************************************************************/
static void dtun_sig_opc_open (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
** Description      Function to emit dtun_sig_opc_progress
**
*******************************************************************************/
static void dtun_sig_opc_progress (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t bytes_transferred = p_data->opc_progress.bytes;
    uint32_t obj_size = p_data->opc_progress.obj_size;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
        BTLA_OBEX_INTERFACE, "OpcProgress",
        DBUS_TYPE_UINT16, &bytes_transferred,
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
static void dtun_sig_opc_object_received (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);
    
    uint16_t status = (uint16_t)p_data->opc_object_received.status;
    char *name = p_data->opc_object_received.name;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_opc_object_pushed (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->opc_object_pushed.status;
    char *name = p_data->opc_object_pushed.name;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_opc_close (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->opc_close.status;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_ops_progress (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t bytes_transferred = p_data->ops_progress.bytes;
    uint32_t obj_size = p_data->ops_progress.obj_size;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
        BTLA_OBEX_INTERFACE, "OpsProgress",
        DBUS_TYPE_UINT16, &bytes_transferred,
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
static void dtun_sig_ops_object_received (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);
    
    uint16_t format = (uint16_t)p_data->ops_object_received.format;
    char *name = p_data->ops_object_received.name;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_ops_open (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_ops_access_request (tDTUN_DEVICE_SIGNAL *p_data)
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
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_ops_close (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_op_create_vcard (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->op_create_vcard.status;
    char *name = p_data->op_create_vcard.name;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_op_owner_vcard_not_set (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    char *name = p_data->op_owner_vcard_not_set.name;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
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
static void dtun_sig_op_store_vcard (tDTUN_DEVICE_SIGNAL *p_data)
{
    debug(__FUNCTION__);

    uint16_t status = (uint16_t)p_data->op_store_vcard.status;
    char *name = p_data->op_store_vcard.name;
    char *contact_name = p_data->op_store_vcard.contact_name;
    uint16_t store_id = (uint16_t)p_data->op_store_vcard.store_id;

    if (!g_dbus_emit_signal(
        sig_connection, BTLA_BASE_PATH,
        BTLA_OBEX_INTERFACE, "OpStoreVcard",
        DBUS_TYPE_UINT16, &status,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_STRING, &contact_name,
        DBUS_TYPE_UINT16, &store_id,
        DBUS_TYPE_INVALID))
    {
        error("%s: Failed to emit signal", __FUNCTION__);
    }
}

/* D-TUN signal callback table */
static tDTUN_SIGNAL obex_signal_tbl[] =
{
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
};

/*******************************************************************************
**
** Function         dtun_obex_process_started
**
** Description     Place holder if something needs to be done at the start
**
*******************************************************************************/
static void dtun_obex_process_started (void) { }

/*******************************************************************************
**
** Function         obexd_termination_handler
**
** Description     Termination handler
**
*******************************************************************************/
void obexd_termination_handler (int sig, siginfo_t *siginfo, void *context)
{
    info ("obexd got Signal: %d", sig);

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
            dtun_client_stop(DTUN_INTERFACE_OBEX);          
            exit(0);
        }
    }
}

/*******************************************************************************
**
** Function         obexd_register_termination_handler
**
** Description     Function to register the termination handler
**
*******************************************************************************/
int obexd_register_termination_handler (void)
{
    struct sigaction act;
 
    memset (&act, '\0', sizeof(act));
 
    /* Use the sa_sigaction field because the handles has two additional parameters */
    act.sa_sigaction = &obexd_termination_handler;
 
    /* The SA_SIGINFO flag tells sigaction() to use the sa_sigaction field, not sa_handler. */
    act.sa_flags = SA_SIGINFO;
 
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror ("sigaction");
        return 1;
    }
    return 0;
}

/*******************************************************************************
**
** Function         property_is_active
**
** Description     Helper function to check property status
**
*******************************************************************************/
int property_is_active(char *property) 
{
    #define PROPERTY_VALUE_MAX  92    
    char value[PROPERTY_VALUE_MAX];

    /* default if not set it 0 */
    property_get(property, value, "0");

    LOGI("property_is_active: %s=%s", property, value);

    if (strcmp(value, "1") == 0) {
        return 1;
    }
    return 0;
}

/*******************************************************************************
**
** Function         dtun_client_main
**
** Description     obexd DTUN client starts here
**
*******************************************************************************/
void dtun_client_main(void)
{
    int retval;

    info("OBEX DTUN client starting");

    obexd_register_termination_handler();

    /* start dbus tunnel on DTUN OBEX subsystem */
    dtun_start_interface(DTUN_INTERFACE_OBEX, obex_signal_tbl, dtun_obex_process_started);

    retval = obex_dbus_init();

    if( retval != 0 )
    {
        perror("Obex D-Bus init:");
        exit(1);
    }

    event_loop = g_main_loop_new(NULL, false);

    info("g_main_loop_run()");

    retval = property_set(DTUN_PROPERTY_OBEXD_ACTIVE, "1");  

    if (retval)
       info("property set failed(%d)", retval); 

    g_main_loop_run(event_loop);

    retval = property_set(DTUN_PROPERTY_OBEXD_ACTIVE, "0");  

    if (retval)
       info("property set failed(%d)", retval); 

    /* fixme -- for some reason the property write sometimes fails
       although the return code says it was ok. Below code is a
       used to ensure we don't exit until we actually see that the 
       property was changed */
    while (property_is_active(DTUN_PROPERTY_OBEXD_ACTIVE))
    {        
        info("obexd property write failed...retrying");
        usleep(200000);
        property_set(DTUN_PROPERTY_OBEXD_ACTIVE, "0");             
    }

    info("main loop exiting");

    //obexd crashes with exception upon SIGTERM if obex_dbus_exit() is called here
    //obex_dbus_exit();
    dtun_client_stop(DTUN_INTERFACE_OBEX);
}
