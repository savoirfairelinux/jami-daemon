/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "requestconfig.h"
#include "guiserver.h"
#include "subcall.h"


ResponseMessage
RequestZeroconf::execute()
{
  return message("500","TODO");
}

ResponseMessage
Request::execute()
{
  return message("500","TODO");
}

ResponseMessage
RequestZeroconfEvent::execute()
{
  return message("500","TODO");
}

ResponseMessage
RequestCallStatus::execute()
{
  return message("500","TODO");
}

ResponseMessage
RequestConfigGetAll::execute()
{
  return message("500","TODO");
}

ResponseMessage
RequestConfigGet::execute()
{
  return message("500","TODO");
}

ResponseMessage
RequestConfigSet::execute()
{
  return message("500","TODO");
}

ResponseMessage
RequestConfigSave::execute()
{
  return message("500","TODO");
}

ResponseMessage
RequestList::execute()
{
  return message("500","TODO");
}
