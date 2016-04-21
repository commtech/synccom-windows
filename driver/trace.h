/*
Copyright (C) 2016  Commtech, Inc.

This file is part of synccom-windows.

synccom-windows is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published bythe Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.

synccom-windows is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
more details.

You should have received a copy of the GNU General Public License along
with synccom-windows.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <evntrace.h> // For TRACE_LEVEL definitions

#if !defined(EVENT_TRACING)

//
// TODO: These defines are missing in evntrace.h
// in some DDK build environments (XP).
//
#if !defined(TRACE_LEVEL_NONE)
  #define TRACE_LEVEL_NONE        0
  #define TRACE_LEVEL_CRITICAL    1
  #define TRACE_LEVEL_FATAL       1
  #define TRACE_LEVEL_ERROR       2
  #define TRACE_LEVEL_WARNING     3
  #define TRACE_LEVEL_INFORMATION 4
  #define TRACE_LEVEL_VERBOSE     5
  #define TRACE_LEVEL_RESERVED6   6
  #define TRACE_LEVEL_RESERVED7   7
  #define TRACE_LEVEL_RESERVED8   8
  #define TRACE_LEVEL_RESERVED9   9
#endif


//
// Define Debug Flags
//
#define DBG_INIT                0x00000001
#define DBG_PNP                 0x00000002
#define DBG_POWER               0x00000004
#define DBG_WMI                 0x00000008
#define DBG_CREATE_CLOSE        0x00000010
#define DBG_IOCTL               0x00000020
#define DBG_WRITE               0x00000040
#define DBG_READ                0x00000080


VOID
TraceEvents    (
    _In_ ULONG   DebugPrintLevel,
    _In_ ULONG   DebugPrintFlag,
    _Printf_format_string_
    _In_ PCSTR   DebugMessage,
    ...
    );

#define WPP_INIT_TRACING(DriverObject, RegistryPath)
#define WPP_CLEANUP(DriverObject)

#else

#define WPP_CHECK_FOR_NULL_STRING  //to prevent exceptions due to NULL strings

//{1F67CDC8 - 3E4C - 42C6 - 980C - A79E79C728BC}
#define WPP_CONTROL_GUIDS \
	WPP_DEFINE_CONTROL_GUID(OsrUsbFxTraceGuid, (1f67cdc8, 3e4c, 42c6, 980c, a79e79c728bc), \
	WPP_DEFINE_BIT(DBG_INIT)              \
	WPP_DEFINE_BIT(DBG_PNP)               \
	WPP_DEFINE_BIT(DBG_POWER)             \
	WPP_DEFINE_BIT(DBG_WMI)               \
	WPP_DEFINE_BIT(DBG_CREATE_CLOSE)      \
	WPP_DEFINE_BIT(DBG_IOCTL)             \
	WPP_DEFINE_BIT(DBG_WRITE)             \
	WPP_DEFINE_BIT(DBG_READ)              \
	)

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level  >= lvl)


#endif



