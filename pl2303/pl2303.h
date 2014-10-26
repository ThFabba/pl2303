#pragma once

#include <ntddk.h>
#include <ntstrsafe.h>
#include <ntddser.h>
#include <usb.h>
#include <usbdlib.h>
#include <usbioctl.h>

/* Pool tags */
#define PL2303_TAG      '32LP'
#define PL2303_URB_TAG  'U2LP'

/* USB requests */
#define PL2303_VENDOR_READ_REQUEST  0x01
#define PL2303_VENDOR_WRITE_REQUEST 0x01
#define PL2303_SET_LINE_REQUEST     0x20

/* Misc defines */
#if defined(_MSC_VER) && !defined(inline)
#define inline __inline
#endif

#ifndef __WARNING_USING_VARIABLE_FROM_FAILED_FUNCTION_CALL
#define __WARNING_USING_VARIABLE_FROM_FAILED_FUNCTION_CALL 6102
#endif

/* Types */
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
    USBD_PIPE_HANDLE BulkInPipe;
    USBD_PIPE_HANDLE BulkOutPipe;
    USBD_PIPE_HANDLE InterruptInPipe;
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

/* usb.c */
NTSTATUS Pl2303UsbStart(_In_ PDEVICE_OBJECT DeviceObject);
NTSTATUS Pl2303UsbStop(_In_ PDEVICE_OBJECT DeviceObject);
NTSTATUS Pl2303UsbSetLine(_In_ PDEVICE_OBJECT DeviceObject,
                          _In_ ULONG BaudRate,
                          _In_ UCHAR StopBits,
                          _In_ UCHAR Parity,
                          _In_ UCHAR DataBits);
NTSTATUS Pl2303UsbRead(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS Pl2303UsbWrite(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
