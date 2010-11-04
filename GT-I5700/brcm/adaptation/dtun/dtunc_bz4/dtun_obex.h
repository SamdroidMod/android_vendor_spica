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
#ifndef OBEX_H
#define OBEX_H

#define BTLA_BASE_INTERFACE "org.bluez"
#define BTLA_OBEX_INTERFACE BTLA_BASE_INTERFACE ".Btla"

#define BTLA_BASE_PATH      "/org/bluez"
#define BTLA_OBEX_PATH      BTLA_BASE_PATH "/btlaobex0"

int obex_dbus_init (DBusConnection *conn);
void obex_dbus_exit (void);

#endif // end OBEX_H
