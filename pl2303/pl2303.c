#define INITGUID
#include "pl2303.h"

DRIVER_INITIALIZE DriverEntry;
static DRIVER_UNLOAD Pl2303Unload;
__drv_dispatchType(IRP_MJ_POWER)
static DRIVER_DISPATCH Pl2303DispatchPower;
__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
__drv_dispatchType(IRP_MJ_INTERNAL_DEVICE_CONTROL)
static DRIVER_DISPATCH Pl2303DispatchDeviceControl;
__drv_dispatchType(IRP_MJ_SYSTEM_CONTROL)
static DRIVER_DISPATCH Pl2303DispatchSystemControl;
__drv_dispatchType(IRP_MJ_CREATE)
static DRIVER_DISPATCH Pl2303DispatchCreate;
__drv_dispatchType(IRP_MJ_CLOSE)
static DRIVER_DISPATCH Pl2303DispatchClose;
__drv_dispatchType(IRP_MJ_READ)
static DRIVER_DISPATCH Pl2303DispatchRead;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Pl2303Unload)
#pragma alloc_text(PAGE, Pl2303DispatchPower)
#pragma alloc_text(PAGE, Pl2303DispatchSystemControl)
#pragma alloc_text(PAGE, Pl2303DispatchDeviceControl)
#pragma alloc_text(PAGE, Pl2303DispatchCreate)
#pragma alloc_text(PAGE, Pl2303DispatchClose)
#pragma alloc_text(PAGE, Pl2303DispatchRead)
#endif /* defined ALLOC_PRAGMA */

NTSTATUS
NTAPI
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    PAGED_CODE();

    Pl2303Debug(         "%s. DriverObject=%p, RegistryPath='%wZ'\n",
                __FUNCTION__, DriverObject,    RegistryPath);

    DriverObject->DriverUnload = Pl2303Unload;
    DriverObject->DriverExtension->AddDevice = Pl2303AddDevice;
    DriverObject->MajorFunction[IRP_MJ_PNP] = Pl2303DispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = Pl2303DispatchPower;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = Pl2303DispatchSystemControl;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = Pl2303DispatchDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = Pl2303DispatchDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = Pl2303DispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = Pl2303DispatchClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = Pl2303DispatchRead;

    return STATUS_SUCCESS;
}

static
VOID
NTAPI
Pl2303Unload(
    _In_ PDRIVER_OBJECT DriverObject)
{
    PAGED_CODE();

    Pl2303Debug(         "%s. DriverObject=%p\n",
                __FUNCTION__, DriverObject);
}

static
NTSTATUS
NTAPI
Pl2303DispatchPower(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    PDEVICE_EXTENSION DeviceExtension;

    PAGED_CODE();

    Pl2303Debug(          "%s. DeviceObject=%p, Irp=%p\n",
                 __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    NT_ASSERT(IoStack->MajorFunction == IRP_MJ_POWER);

    DeviceExtension = DeviceObject->DeviceExtension;

    if (DeviceExtension->PnpState == Deleted)
    {
        Pl2303Warn(         "%s. Device already deleted\n",
                   __FUNCTION__);
        PoStartNextPowerIrp(Irp);
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(DeviceExtension->LowerDevice, Irp);
}

static
NTSTATUS
NTAPI
Pl2303DispatchSystemControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    PDEVICE_EXTENSION DeviceExtension;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    NT_ASSERT(IoStack->MajorFunction == IRP_MJ_SYSTEM_CONTROL);

    DeviceExtension = DeviceObject->DeviceExtension;

    if (DeviceExtension->PnpState == Deleted)
    {
        Pl2303Warn(         "%s. Device already deleted\n",
                   __FUNCTION__);
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DeviceExtension->LowerDevice, Irp);
}

static
PCSTR
SerialGetIoctlName(
    _In_ ULONG IoControlCode)
{
    switch (IoControlCode)
    {
        case IOCTL_SERIAL_SET_BAUD_RATE : return "IOCTL_SERIAL_SET_BAUD_RATE";
        case IOCTL_SERIAL_GET_BAUD_RATE: return "IOCTL_SERIAL_GET_BAUD_RATE";
        case IOCTL_SERIAL_GET_MODEM_CONTROL: return "IOCTL_SERIAL_GET_MODEM_CONTROL";
        case IOCTL_SERIAL_SET_MODEM_CONTROL: return "IOCTL_SERIAL_SET_MODEM_CONTROL";
        case IOCTL_SERIAL_SET_FIFO_CONTROL: return "IOCTL_SERIAL_SET_FIFO_CONTROL";
        case IOCTL_SERIAL_SET_LINE_CONTROL: return "IOCTL_SERIAL_SET_LINE_CONTROL";
        case IOCTL_SERIAL_GET_LINE_CONTROL: return "IOCTL_SERIAL_GET_LINE_CONTROL";
        case IOCTL_SERIAL_SET_TIMEOUTS: return "IOCTL_SERIAL_SET_TIMEOUTS";
        case IOCTL_SERIAL_GET_TIMEOUTS: return "IOCTL_SERIAL_GET_TIMEOUTS";
        case IOCTL_SERIAL_SET_CHARS: return "IOCTL_SERIAL_SET_CHARS";
        case IOCTL_SERIAL_GET_CHARS: return "IOCTL_SERIAL_GET_CHARS";
        case IOCTL_SERIAL_SET_DTR: return "IOCTL_SERIAL_SET_DTR";
        case IOCTL_SERIAL_CLR_DTR: return "IOCTL_SERIAL_SET_DTR";
        case IOCTL_SERIAL_RESET_DEVICE: return "IOCTL_SERIAL_RESET_DEVICE";
        case IOCTL_SERIAL_SET_RTS: return "IOCTL_SERIAL_SET_RTS";
        case IOCTL_SERIAL_CLR_RTS: return "IOCTL_SERIAL_CLR_RTS";
        case IOCTL_SERIAL_SET_XOFF: return "IOCTL_SERIAL_SET_XOFF";
        case IOCTL_SERIAL_SET_XON: return "IOCTL_SERIAL_SET_XON";
        case IOCTL_SERIAL_SET_BREAK_ON: return "IOCTL_SERIAL_SET_BREAK_ON";
        case IOCTL_SERIAL_SET_BREAK_OFF: return "IOCTL_SERIAL_SET_BREAK_OFF";
        case IOCTL_SERIAL_SET_QUEUE_SIZE: return "IOCTL_SERIAL_SET_QUEUE_SIZE";
        case IOCTL_SERIAL_GET_WAIT_MASK: return "IOCTL_SERIAL_GET_WAIT_MASK";
        case IOCTL_SERIAL_SET_WAIT_MASK: return "IOCTL_SERIAL_SET_WAIT_MASK";
        case IOCTL_SERIAL_WAIT_ON_MASK: return "IOCTL_SERIAL_WAIT_ON_MASK";
        case IOCTL_SERIAL_IMMEDIATE_CHAR: return "IOCTL_SERIAL_IMMEDIATE_CHAR";
        case IOCTL_SERIAL_PURGE: return "IOCTL_SERIAL_PURGE";
        case IOCTL_SERIAL_GET_HANDFLOW: return "IOCTL_SERIAL_GET_HANDFLOW";
        case IOCTL_SERIAL_SET_HANDFLOW: return "IOCTL_SERIAL_SET_HANDFLOW";
        case IOCTL_SERIAL_GET_MODEMSTATUS: return "IOCTL_SERIAL_GET_MODEMSTATUS";
        case IOCTL_SERIAL_GET_DTRRTS: return "IOCTL_SERIAL_GET_DTRRTS";
        case IOCTL_SERIAL_GET_COMMSTATUS: return "IOCTL_SERIAL_GET_COMMSTATUS";
        case IOCTL_SERIAL_GET_PROPERTIES: return "IOCTL_SERIAL_GET_PROPERTIES";
        case IOCTL_SERIAL_XOFF_COUNTER: return "IOCTL_SERIAL_XOFF_COUNTER";
        case IOCTL_SERIAL_LSRMST_INSERT: return "IOCTL_SERIAL_LSRMST_INSERT";
        case IOCTL_SERIAL_CONFIG_SIZE: return "IOCTL_SERIAL_CONFIG_SIZE";
        case IOCTL_SERIAL_GET_STATS: return "IOCTL_SERIAL_GET_STATS";
        case IOCTL_SERIAL_CLEAR_STATS: return "IOCTL_SERIAL_CLEAR_STATS";
        default: return "Unknown ioctl";
    }
}

static
NTSTATUS
NTAPI
Pl2303DispatchDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    PDEVICE_EXTENSION DeviceExtension;
    ULONG IoControlCode;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    NT_ASSERT(IoStack->MajorFunction == IRP_MJ_DEVICE_CONTROL ||
           IoStack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL);

    DeviceExtension = DeviceObject->DeviceExtension;

    if (DeviceExtension->PnpState == Deleted)
    {
        Pl2303Warn(         "%s. Device already deleted\n",
                   __FUNCTION__);
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    IoControlCode = IoStack->Parameters.DeviceIoControl.IoControlCode;
    switch (IoControlCode)
    {
        case IOCTL_SERIAL_CLEAR_STATS:
        default:
            Pl2303Debug(         "%s. DeviceControl %x, code %s (%08lx)\n",
                        __FUNCTION__, IoStack->MajorFunction, SerialGetIoctlName(IoControlCode), IoControlCode);
    }

    Status = STATUS_NOT_IMPLEMENTED;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;

    //IoSkipCurrentIrpStackLocation(Irp);
    //return IoCallDriver(DeviceExtension->LowerDevice, Irp);
}

static
NTSTATUS
NTAPI
Pl2303DispatchCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    NT_ASSERT(IoStack->MajorFunction == IRP_MJ_CREATE);

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

static
NTSTATUS
NTAPI
Pl2303DispatchClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    NT_ASSERT(IoStack->MajorFunction == IRP_MJ_CLOSE);

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}

static
NTSTATUS
NTAPI
Pl2303DispatchRead(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStack;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    NT_ASSERT(IoStack->MajorFunction == IRP_MJ_READ);

    if (!IoStack->Parameters.Read.Length)
    {
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Status = Pl2303UsbRead(DeviceObject, Irp);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbRead failed with %08lx\n",
                    __FUNCTION__, Status);
    }

    return Status;
}
