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

#ifndef LINUX_NATIVE
#include <cutils/properties.h>
#endif

#include "dtun_clnt.h"

#include <glib.h>

/* some local debug macros */
#ifdef DTUN_STANDALONE
#define info(format, ...) fprintf (stdout, format, ## __VA_ARGS__)
#define debug(format, ...) fprintf (stdout, format, ## __VA_ARGS__)
#define verbose(format, ...) fprintf (stdout, format, ## __VA_ARGS__)
#define error(format, ...) fprintf (stderr, format, ## __VA_ARGS__)
#else
#define LOG_TAG "DTUN_CLNT"
#include "utils/Log.h"
#define info(format, ...) LOGI (format, ## __VA_ARGS__)
#define debug(format, ...) LOGD (format, ## __VA_ARGS__)
#define verbose(format, ...) LOGV (format, ## __VA_ARGS__)
#define error(format, ...) LOGE (format, ## __VA_ARGS__)
#endif

#define PRINTFUNC() debug("\t\t%s()\n", __FUNCTION__);

typedef enum {
    DTUN_STATE_STOPPED,
    DTUN_STATE_STARTING,
    DTUN_STATE_RUNNING,
    DTUN_STATE_SHUTTING_DOWN
} tDTUN_STATE;

typedef struct {
    tDTUN_PROCESS_INIT init_cb;
    tDTUN_SIGNAL *client_tbl;
    tDTUN_STATE state;
    tSUBSYSTEM sub;
    int srv_fd;
} tDTUN_INTERFACE_CB;

tDTUN_INTERFACE_CB dtun_cb[DTUN_INTERFACE_MAX];

char ctrl_rx_buf[128];

pthread_mutex_t dtun_mutex = PTHREAD_MUTEX_INITIALIZER;
 
typedef gboolean (*tDTUN_PROCESS_HANDLER1)(GIOChannel *chan, GIOCondition cond, gpointer data);
 
/*******************************************************************************
 **
 ** DTUN Server structs
 **
 **
 *******************************************************************************/

typedef unsigned short tSUB;
typedef unsigned short tRESULT;
typedef unsigned short tHDR_LEN;
typedef unsigned short tCTRL_MSG_ID;

#pragma pack(1)
 
typedef union
{
    tRESULT                          result;
    tDTUN_DEVICE_METHOD              dtun_method;
    tDTUN_DEVICE_SIGNAL              dtun_sig;
} tPARAMS;

/* main packet header */
typedef struct
{
    tCTRL_MSG_ID id;
    tHDR_LEN     len;
} tHDR;

/* tCTRL_MSG_ID */
typedef struct
{
    tHDR          hdr;
    tSUB          sub;
    tPARAMS       params;
} tCTRL_MSG_PKT;

#pragma pack(0)

/*******************************************************************************
**
** Function          misc functions
**
** Description
**
**
** Returns          void
**
*******************************************************************************/

static const char *id_name[] =
{
    /* DM methods */
    "DTUN_METHOD_BEGIN",
    "DTUN_METHOD_DM_GET_LOCAL_INFO",
    "DTUN_METHOD_DM_START_DISCOVERY",
    "DTUN_METHOD_DM_CANCEL_DISCOVERY",
    "DTUN_METHOD_DM_GET_REMOTE_SERVICE_CHANNEL",
    "DTUN_METHOD_DM_GET_REMOTE_SERVICES",
    "DTUN_METHOD_DM_GET_ALL_REMOTE_SERVICES",
    "DTUN_METHOD_DM_CREATE_BONDING",
    "DTUN_METHOD_DM_REMOVE_BONDING",
    "DTUN_METHOD_DM_PIN_REPLY",
    "DTUN_METHOD_DM_PIN_NEG_REPLY",
    "DTUN_METHOD_DM_AUTHORIZE_RSP",
    "DTUN_METHOD_DM_SET_MODE",
    "DTUN_METHOD_DM_SET_NAME",
    "DTUN_METHOD_DM_ADD_DEV",
    "DTUN_METHOD_DM_SSP_CONFIRM_REPLY",
    "DTUN_METHOD_DM_DISC_RMT_DEV",
    "DTUN_METHOD_DM_ADD_SDP_REC",
    "DTUN_METHOD_DM_DEL_SDP_REC",
    "DTUN_METHOD_DM_SET_TESTMODE",
    /* AV methods */
    "DTUN_METHOD_AM_AV_OPEN", 
    "DTUN_METHOD_AM_AV_DISC", 
    "DTUN_METHOD_AM_AV_STARTSTOP",     
    /* OBEX methods */
    "DTUN_METHOD_OPC_ENABLE",
    "DTUN_METHOD_OPC_CLOSE",
    "DTUN_METHOD_OPS_CLOSE",
    "DTUN_METHOD_OPC_PUSH_OBJECT",
    "DTUN_METHOD_OPC_PULL_VCARD",
    "DTUN_METHOD_OPC_EXCH_VCARD",

    "DTUN_METHOD_OP_GRANT_ACCESS",
    "DTUN_METHOD_OP_SET_OWNER_VCARD",
    "DTUN_METHOD_OP_SET_EXCHANGE_FOLDER",
    "DTUN_METHOD_OP_CREATE_VCARD",
    "DTUN_METHOD_OP_STORE_VCARD",
    "DTUN_METHOD_OP_SET_DEFAULT_PATH",
    "DTUN_METHOD_END",

    /* DM signals */
    "DTUN_SIG_BEGIN",
    "DTUN_SIG_DM_LOCAL_INFO",
    "DTUN_SIG_DM_DISCOVERY_STARTED",
    "DTUN_SIG_DM_DISCOVERY_COMPLETE",
    "DTUN_SIG_DM_DEVICE_FOUND",
    "DTUN_SIG_DM_RMT_NAME",
    "DTUN_SIG_DM_RMT_SERVICE_CHANNEL",
    "DTUN_SIG_DM_RMT_SERVICES",
    "DTUN_SIG_DM_PIN_REQ",
    "DTUN_SIG_DM_AUTHORIZE_REQ",
    "DTUN_SIG_DM_AUTH_COMP",
    "DTUN_SIG_DM_LINK_DOWN",
    "DTUN_SIG_DM_SSP_CFM_REQ",
    "DTUN_SIG_DM_LINK_UP",
    "DTUN_SIG_DM_SDP_REC_HANDLE",
    "DTUN_SIG_DM_TESTMODE_STATE",
    /* AV signals */
    "DTUN_SIG_AM_AV_EVENT",
    /* OPC signals */
    "DTUN_SIG_OPC_ENABLE",
    "DTUN_SIG_OPC_OPEN",               /* Connection to peer is open. */
    "DTUN_SIG_OPC_PROGRESS",           /* Object being sent or received. */
    "DTUN_SIG_OPC_OBJECT_RECEIVED",
    "DTUN_SIG_OPC_OBJECT_PUSHED",
    "DTUN_SIG_OPC_CLOSE",              /* Connection to peer closed. */
    /* OPS signals */
    "DTUN_SIG_OPS_PROGRESS",
    "DTUN_SIG_OPS_OBJECT_RECEIVED",
    "DTUN_SIG_OPS_OPEN",
    "DTUN_SIG_OPS_ACCESS_REQUEST",
    "DTUN_SIG_OPS_CLOSE",
    "DTUN_SIG_OP_CREATE_VCARD",
    "DTUN_SIG_OP_OWNER_VCARD_NOT_SET",
    "DTUN_SIG_OP_STORE_VCARD",
    "DTUN_SIG_OP_SET_DEFAULT_PATH",
    "DTUN_SIG_END",

    /* Common */
    "DTUN_DM_ERROR",

    "DTUN_ID_MAX"

};

static const char *iface_name[] =
{
    /* DM methods */
    "DTUN_INTERFACE"
};


void hexdump(char *msg, char *p, int len)
{
#if 0
    int i;
    int pos=0;
    char buf[1024];
    
    pos += sprintf(buf+pos, "%s %d bytes\n", msg, len);

    for (i=0;i<len;i++)
    {
        if ((i%16)==0)
           pos+=sprintf(buf+pos, "\n");
        
        pos+=sprintf(buf+pos, "%02x ", p[i]);
    }
    pos+=sprintf(buf+pos, "\n");

    verbose("%s", buf);
#endif
}

int is_emulator_context(char *msg) 
{
#ifndef LINUX_NATIVE
    #define PROPERTY_VALUE_MAX  92    
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.kernel.qemu", value, "0");
    debug("[%s] is_emulator_context : %s\n", msg, value);
    if (strcmp(value, "1") == 0) {
        return 1;
    }
#endif    
    return 0;
}

void dtun_init_cb(void)
{
    int i;
    static int init_done = 0;

    if (init_done)
        return;

    for (i=0; i<DTUN_INTERFACE_MAX; i++)
    {
        dtun_cb[i].init_cb = NULL;
        dtun_cb[i].state = DTUN_STATE_STOPPED;
        dtun_cb[i].client_tbl = NULL;
        dtun_cb[i].srv_fd = DTUN_FD_NOT_CONNECTED;
        dtun_cb[i].sub = -1;
    }
    
    init_done = 1;
}

static const char *id2name(tDTUN_ID id)
{
    if (id && id < DTUN_ID_MAX)
        return id_name[id];

    return "invalid id";
}

static const char *iface2name(tDTUN_INTERFACE iface)
{
    if (iface < DTUN_INTERFACE_MAX)
        return iface_name[iface];

    return "invalid interface";
}


tDTUN_INTERFACE get_iface_by_id(eDTUN_ID id)
{
    if ((id>DTUN_METHOD_BEGIN) && (id<DTUN_SIG_END))
        return DTUN_INTERFACE;
    else
        error("[get_iface_by_id] error : invalid id %d\n", id);

    return -1;
}


int get_index_by_id(eDTUN_ID id)
{
    if ((id > DTUN_METHOD_BEGIN) && (id < DTUN_METHOD_END))
        return id - DTUN_METHOD_BEGIN - 1;
    else if ((id > DTUN_SIG_BEGIN) && (id < DTUN_SIG_END))
        return id - DTUN_SIG_BEGIN - 1;
    else
        error("[get_index_by_id] error : invalid id %d\n", id);
    return -1;
}

static int create_subprocess(int iface, tDTUN_PROCESS_HANDLER1 handler)
{
    pthread_t thread_id;
    struct sched_param param;
    int policy;
    GIOChannel *ctl_io;
    int fd = dtun_cb[iface].srv_fd;

    debug("create_subprocess : [%s]\n", iface2name(iface));

 #ifdef DTUN_THREADED
    if (pthread_create(&thread_id, NULL,
                       (void*)handler, (void*)iface)!=0)
      perror("pthread_create");


    if(pthread_getschedparam(thread_id, &policy, &param)==0)
    {
        //Bump up the priority of the thread to avoid contention between Tx/Rx dbus messages
        policy = SCHED_FIFO;
        param.sched_priority = 91;
        pthread_setschedparam(thread_id, policy, &param);
    }
#else
	ctl_io = g_io_channel_unix_new(fd);

	g_io_channel_set_close_on_unref(ctl_io, TRUE);

	g_io_add_watch(ctl_io, G_IO_IN, handler, NULL);

	g_io_channel_unref(ctl_io);
#endif
    return 0;
}

/*******************************************************************************
**
** Function
**
** Description      Local helper functions
**
**
** Returns          void
**
*******************************************************************************/

static int send_ctrl_msg(int fd, tSUB sub, tCTRL_MSG_ID type, tPARAMS *params, int param_len)
{
    tCTRL_MSG_PKT pkt;

    pkt.hdr.len = sizeof(tSUB) + param_len;
    pkt.hdr.id = type;
    pkt.sub = sub;

    if (param_len)
        memcpy(&pkt.params, params, param_len); 

    hexdump("dtun-tx", (char *)&pkt, sizeof(tHDR) + sizeof(tSUB) + param_len);

    return send(fd, (char*)&pkt, sizeof(tHDR) + sizeof(tSUB) + param_len, 0);
}

static int recv_ctrl_msg(int fd, tCTRL_MSG_PKT *pkt)
{
    int result;

    /* wait for header */
    result = recv(fd, (char *)pkt, sizeof(tHDR), MSG_WAITALL);

    if (result<=0)
        return result;

    /* wait for payload */
    result = recv(fd, (char*)pkt+sizeof(tHDR), pkt->hdr.len, MSG_WAITALL);

    if (result<=0)
        return result;

    return result;
}


static int register_subsystem(int fd, tSUB sub)
{
    tCTRL_MSG_PKT pkt;
    int result;

    send_ctrl_msg(fd, sub, REGISTER_SUBSYS_REQ, NULL, 0);

    result = recv_ctrl_msg(fd, &pkt);

    if (result <= 0)
        return result;

    return pkt.params.result;
}

static int connect_srv(char *ip_str)
{
    int                 sockfd, n;
    struct sockaddr_in  servaddr;

    info("Connect DTUN server [%s]\n", ip_str);

    if ( (sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        perror("socket error");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip_str);
    servaddr.sin_port        = htons(DTUN_PORT);

    n = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    if (n<0)
    {
       perror("connect");
       close(sockfd);
       return -1;
    }

    printf("Connected (%d)\n", sockfd);

    return sockfd;
}


boolean wait_for_signal(int fd)
{
    ssize_t ret;
    tDTUN_DEVICE_SIGNAL *signal;
    tCTRL_MSG_PKT ctrl_pkt;

    /* read header */
    ret = recv(fd, (char*)&ctrl_pkt, sizeof(tHDR), MSG_WAITALL);

    hexdump("dtun-rx", (char*)&ctrl_pkt, sizeof(tHDR));

    if (ret == 0)
    {
        info("DTUN server disconnected\n");
        return false;
    }
    /* read method body if non zero len */
    if (ctrl_pkt.hdr.len)
    {
       ret = recv(fd, ((char*)&ctrl_pkt)+sizeof(tHDR), ctrl_pkt.hdr.len, MSG_WAITALL);

       hexdump("dtun-rx", ((char*)&ctrl_pkt)+sizeof(tHDR), ctrl_pkt.hdr.len);

       if (ret == 0)
       {
           info("DTUN server disconnected\n");
           return false;
       }
    }

    signal = &ctrl_pkt.params.dtun_sig;

    info("dtun-rx received signal [%s] (id %d)\n", id2name(signal->hdr.id), signal->hdr.id);

    {
        int index = get_index_by_id(signal->hdr.id);
        tDTUN_INTERFACE iface = get_iface_by_id(signal->hdr.id);
        
        if ((index<0) || (iface<0))
        {
            error("WARNING : no index or interface found\n");
            return false;
        }

        if (!dtun_cb[iface].client_tbl[index])
        {
            error("WARNING : no dtun signal handler defined for id %s (index %d)\n", id2name(signal->hdr.id), index);
            return false;
        }


        (*dtun_cb[iface].client_tbl[index])(signal);
        //Allow others to run for some time
        usleep( 16000 );

    }
    
    return true;
}

/* for standalone client */
static void client_signal_handler_threaded(void *param)
{
    int iface = (int)param;

    debug("\nclient_signal_handler started (fd %d)\n", dtun_cb[iface].srv_fd);

    if (dtun_cb[iface].init_cb)
        dtun_cb[iface].init_cb();

    /* wait for response */
    while (dtun_cb[iface].state == DTUN_STATE_RUNNING)
    {
        if (wait_for_signal(dtun_cb[iface].srv_fd) == false)
            break;
    }

    close(dtun_cb[iface].srv_fd);

    dtun_cb[iface].state = DTUN_STATE_STOPPED;
    dtun_cb[iface].srv_fd = -1;

    debug("client_signal_handler stopped\n\n");
}

static gboolean client_signal_handler(GIOChannel *chan, GIOCondition cond, gpointer data)
{

    ssize_t ret;    
    tDTUN_DEVICE_SIGNAL *signal;
    tCTRL_MSG_PKT ctrl_pkt;
	size_t len;
	GIOError err;
    
    /* read header */
	if ((err = g_io_channel_read(chan, (char*)&ctrl_pkt, sizeof(tHDR), &len))) {
		if (err == G_IO_ERROR_AGAIN)
			return TRUE;

		error("Read from control socket failed: %s (%d)",
							strerror(errno), errno);
		return FALSE;
	}

    hexdump("dtun-rx", (char*)&ctrl_pkt, sizeof(tHDR));

    /* read method body if non zero len */
    if (ctrl_pkt.hdr.len)
    {
        if ((err = g_io_channel_read(chan, ((char*)&ctrl_pkt)+sizeof(tHDR), ctrl_pkt.hdr.len, &len))) 
        {
		if (err == G_IO_ERROR_AGAIN)
			return TRUE;

		error("Read from control socket failed: %s (%d)",
							strerror(errno), errno);
		return FALSE;
	}
       hexdump("dtun-rx", ((char*)&ctrl_pkt)+sizeof(tHDR), ctrl_pkt.hdr.len);
    } 

    signal = &ctrl_pkt.params.dtun_sig;

    info("dtun-rx signal [%s] (id %d) len %d\n", id2name(signal->hdr.id), signal->hdr.id, ctrl_pkt.hdr.len);

    {
        int index = get_index_by_id(signal->hdr.id);
        tDTUN_INTERFACE iface = get_iface_by_id(signal->hdr.id);
        
        if ((index<0) || (iface<0))
        {
            error("WARNING : no index or interface found\n");
            return false;
        }

        if (!dtun_cb[iface].client_tbl[index])
        {
            error("WARNING : no dtun signal handler defined for id %s (index %d)\n", id2name(signal->hdr.id), index);
            return false;
        }        


        (*dtun_cb[iface].client_tbl[index])(signal);

    }
    
    return true;
}

/*******************************************************************************
**
** Function          dtun_client_call_method
**
** Description      Send method call
**
**
** Returns          void
**
*******************************************************************************/

boolean dtun_client_call_method(tDTUN_DEVICE_METHOD *method)
{
    tDTUN_INTERFACE iface = get_iface_by_id(method->hdr.id);
        
    info("\tClient calling %s (id %d)\n", id2name(method->hdr.id), method->hdr.id);

    /* make sure we are connected to server*/
    if (dtun_cb[iface].state == DTUN_STATE_RUNNING)
    {
        send_ctrl_msg(dtun_cb[iface].srv_fd, dtun_cb[iface].sub, DTUN_METHOD_CALL, (tPARAMS *)method, method->hdr.len+sizeof(tDTUN_HDR));
    }
    return true;
}

/*******************************************************************************
**
** Function          dtun_client_call_id_only
**
** Description      Send method call without any arguments
**
**
** Returns          void
**
*******************************************************************************/

boolean dtun_client_call_id_only(eDTUN_ID id)
{
    tDTUN_DEVICE_METHOD method;

    method.hdr.id = id;
    method.hdr.len = 0; // no payload

    return dtun_client_call_method(&method);
}

/*******************************************************************************
**
** Function          dtun_client_start
**
** Description
**
**
** Returns          void
**
*******************************************************************************/

void dtun_start_interface(tDTUN_INTERFACE iface, tDTUN_SIGNAL *tbl, tDTUN_PROCESS_INIT init_cb)
{
    int result;
    int fd = DTUN_FD_NOT_CONNECTED;
    tSUBSYSTEM sub = iface + SUBSYSTEM_DTUN; // remap to subsystem

    dtun_init_cb();
    
    if (dtun_cb[iface].state !=DTUN_STATE_STOPPED)
        return;

    info("dtun_start_interface : iface [%s] starting...\n", iface2name(iface));

    dtun_cb[iface].state = DTUN_STATE_STARTING;

    /* make sure we abort connection attempts in case dtun is shutdown */
    while ((dtun_cb[iface].state = DTUN_STATE_STARTING) && (fd == DTUN_FD_NOT_CONNECTED))
    {
        /* connect to dtun server */
        fd = connect_srv(DTUN_SERVER_ADDR);

           /* only enable alt address for emulator */
        if ( (fd<0) && is_emulator_context("dtun_start_interface") )
           {
              info("primary server failed, try alternate\n");

              /* try alternate address */
              fd = connect_srv(DTUN_SERVER_ADDR_ALT);
           }

        /* wait 1 sec if we failed to get a connection */
        if (fd<0)
           sleep(1);

    }

    dtun_cb[iface].srv_fd = fd;
    dtun_cb[iface].sub = sub;
    dtun_cb[iface].init_cb = init_cb;
    
    result = register_subsystem(fd, sub);

    if (dtun_cb[iface].client_tbl == NULL)
    {
       dtun_cb[iface].client_tbl = tbl;
    }
    else
    {        
        error("WARNING : dtun interface busy\n");
    }
    
    /* we are now in running state */
    dtun_cb[iface].state = DTUN_STATE_RUNNING;

    /* create subprocess to process incoming signals */
    create_subprocess(iface, client_signal_handler);  
}

/*******************************************************************************
**
** Function          dtun_client_stop
**
** Description
**
**
** Returns          void
**
*******************************************************************************/

void dtun_client_stop(tDTUN_INTERFACE iface)
{
    info("dtun_client_stop : iface [%s] stopping...\n", iface2name(iface));

    close(dtun_cb[iface].srv_fd);
    
    dtun_cb[iface].state = DTUN_STATE_STOPPED;
    dtun_cb[iface].srv_fd = -1;
}



/*******************************************************************************
**
** Function          DTUN DM CLIENT TEST
**
**
********************************************************************************/

void test_sig_local_info(tDTUN_DEVICE_SIGNAL *msg)
{
    PRINTFUNC();
}

void test_sig_discovery_started(tDTUN_DEVICE_SIGNAL *msg)
{
    /* send start discovery started signal in dbus srv */
    PRINTFUNC();

    /* fixme -- hookup to DBUS server */

}

void test_sig_discovery_complete(tDTUN_DEVICE_SIGNAL *msg)
{
    /* send  discovery completge signal  in dbus srv */
    PRINTFUNC();

    /* fixme -- hookup to DBUS server */
}

void test_sig_device_found(tDTUN_DEVICE_SIGNAL *msg)
{
    /* send start device found signal in dbus srv */
    PRINTFUNC();

    debug("\tFound device [%02x:%02x:%02x:%02x:%02x:%02x] ***\n\n", 
              msg->device_found.info.bd.b[0],  msg->device_found.info.bd.b[1],
              msg->device_found.info.bd.b[2],  msg->device_found.info.bd.b[3],
              msg->device_found.info.bd.b[4],  msg->device_found.info.bd.b[5]);

    /* fixme -- hookup to DBUS server */
}


void test_sig_rmt_name(tDTUN_DEVICE_SIGNAL *msg)
{
    PRINTFUNC();
}


/* signal callback table */
const tDTUN_SIGNAL test_signal_tbl[] =
{
    /* dm */
    test_sig_local_info,
    test_sig_discovery_started,
    test_sig_discovery_complete,
    test_sig_device_found,
    test_sig_rmt_name

    /* add here */
};


void dtun_client_test(void)
{
    /* start dbus tunnel on DTUN DM subsystem */
    dtun_start_interface(DTUN_INTERFACE, test_signal_tbl, NULL);

    info("\n\n");

    /* call some test methods */
    sleep(1);
    dtun_client_call_id_only(DTUN_METHOD_DM_START_DISCOVERY);

    //while (1)
        sleep(20);

    /* stop dbus tunnel */
    dtun_client_stop(DTUN_INTERFACE);
   // dtun_client_stop(DTUN_INTERFACE_AV);
}


/*******************************************************************************
**
** Function        TEST  MAIN
**
**
*******************************************************************************/

#ifdef BLUEZ3
int main(int argc, char** argv)
{
#ifdef BT_ALT_STACK
     dtun_client_main();
#else
     dtun_client_test();
#endif
    return 0;
}
#endif

