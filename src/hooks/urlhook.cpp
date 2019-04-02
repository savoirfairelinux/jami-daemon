/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#include "urlhook.h"
#include <cstdlib>

namespace jami {

int UrlHook::runAction(const std::string &command, const std::string &args)
{
    //FIXME : use fork and execve, so no need to escape shell arguments
    const std::string cmd = command + (args.empty() ? "" : " ") +
                            "\"" + args + "\" &";

#if __APPLE__
  #include "TargetConditionals.h"
  #if defined(TARGET_IPHONE_SIMULATOR) || defined(TARGET_OS_IPHONE)
    return 0;
  #endif
#elif defined(RING_UWP)
    return 0;
#else
    return system(cmd.c_str());
#endif
}

} // namespace jami
