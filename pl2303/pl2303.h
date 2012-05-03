#pragma once

#include <ntddk.h>

typedef struct _DEVICE_EXTENSION
{
    PDEVICE_OBJECT NextDevice;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;
