#pragma once

#include <ntddk.h>
#include <ntstrsafe.h>
#include <ntddser.h>

#define PL2303_TAG '32LP'

#define inline __inline

typedef enum _DEVICE_PNP_STATE
{
    NotStarted,
    Started,
    StopPending,
    Stopped,
    RemovePending,
    SurpriseRemovePending,
    Deleted
} DEVICE_PNP_STATE, *PDEVICE_PNP_STATE;

typedef struct _DEVICE_EXTENSION
{
    PDEVICE_OBJECT LowerDevice;
    DEVICE_PNP_STATE PnpState;
    DEVICE_PNP_STATE PreviousPnpState;
    UNICODE_STRING DeviceName;
    UNICODE_STRING InterfaceLinkName;
    UNICODE_STRING ComPortName;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

/* Debugging functions */
static
inline
VOID
Pl2303Debug(
    _In_ PCSTR Format,
    ...)
{
    va_list Arguments;
    va_start(Arguments, Format);
    (VOID)vDbgPrintExWithPrefix("Pl2303: ",
                                DPFLTR_IHVDRIVER_ID,
                                DPFLTR_TRACE_LEVEL,
                                Format,
                                Arguments);
    va_end(Arguments);
}

static
inline
VOID
Pl2303Warn(
    _In_ PCSTR Format,
    ...)
{
    va_list Arguments;
    va_start(Arguments, Format);
    (VOID)vDbgPrintExWithPrefix("Pl2303: ",
                                DPFLTR_IHVDRIVER_ID,
                                DPFLTR_WARNING_LEVEL,
                                Format,
                                Arguments);
    va_end(Arguments);
}

static
inline
VOID
Pl2303Error(
    _In_ PCSTR Format,
    ...)
{
    va_list Arguments;
    va_start(Arguments, Format);
    (VOID)vDbgPrintExWithPrefix("Pl2303: ",
                                DPFLTR_IHVDRIVER_ID,
                                DPFLTR_ERROR_LEVEL,
                                Format,
                                Arguments);
    va_end(Arguments);
}

/* pnp.c */
DRIVER_ADD_DEVICE Pl2303AddDevice;
__drv_dispatchType(IRP_MJ_PNP)
DRIVER_DISPATCH Pl2303DispatchPnp;
