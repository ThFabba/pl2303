#pragma once

#include <ntddk.h>
#include <ntstrsafe.h>
#include <ntddser.h>

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
    UNICODE_STRING InterfaceLinkName;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

/* pnp.c */
DRIVER_ADD_DEVICE Pl2303AddDevice;
__drv_dispatchType(IRP_MJ_PNP)
DRIVER_DISPATCH Pl2303DispatchPnp;
