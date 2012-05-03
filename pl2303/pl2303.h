#pragma once

#include <ntddk.h>

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
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;
