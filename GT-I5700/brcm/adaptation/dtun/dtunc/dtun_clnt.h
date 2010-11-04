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

#ifndef DTUN_CLNT_H
#define DTUN_CLNT_H

#include "dtun.h"

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

boolean dtun_client_call_method(tDTUN_DEVICE_METHOD *method);


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

boolean dtun_client_call_id_only(eDTUN_ID id);

/*******************************************************************************
**
** Function          dtun_server_register_interface
**
** Description      
**    
**
** Returns          void
**
*******************************************************************************/

int dtun_client_register_interface(tDTUN_INTERFACE iface, tDTUN_SIGNAL *tbl);

/*******************************************************************************
**
** Function          dtun_start_interface
**
** Description      
**
**    
** Returns          void
**
*******************************************************************************/

void dtun_start_interface(tDTUN_INTERFACE iface, tDTUN_SIGNAL *tbl, tDTUN_PROCESS_INIT init_cb);

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

void dtun_client_stop(tDTUN_INTERFACE iface);

#endif


