#pragma once

#include <ntddk.h>
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
    PDEVICE_OBJECT NextDevice;
    DEVICE_PNP_STATE PnpState;
    DEVICE_PNP_STATE PreviousPnpState;
    UNICODE_STRING SymbolicLinkName;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;
