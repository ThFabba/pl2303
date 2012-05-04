#include "pl2303.h"

DRIVER_INITIALIZE DriverEntry;
static DRIVER_UNLOAD Pl2303Unload;
static DRIVER_ADD_DEVICE Pl2303AddDevice;
__drv_dispatchType(IRP_MJ_PNP)
static DRIVER_DISPATCH Pl2303DispatchPnp;
__drv_dispatchType(IRP_MJ_POWER)
static DRIVER_DISPATCH Pl2303DispatchPower;
__drv_dispatchType(IRP_MJ_CREATE)
__drv_dispatchType(IRP_MJ_CLOSE)
static DRIVER_DISPATCH Pl2303DispatchCreateClose;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Pl2303Unload)
#pragma alloc_text(PAGE, Pl2303AddDevice)
#pragma alloc_text(PAGE, Pl2303DispatchPnp)
#pragma alloc_text(PAGE, Pl2303DispatchPower)
#pragma alloc_text(PAGE, Pl2303DispatchCreateClose)
#endif /* defined ALLOC_PRAGMA */

NTSTATUS
NTAPI
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    PAGED_CODE();

    DriverObject->DriverUnload = Pl2303Unload;
    DriverObject->DriverExtension->AddDevice = Pl2303AddDevice;
    DriverObject->MajorFunction[IRP_MJ_PNP] = Pl2303DispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = Pl2303DispatchPower;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = Pl2303DispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = Pl2303DispatchCreateClose;

    return STATUS_SUCCESS;
}

static
VOID
NTAPI
Pl2303Unload(
    _In_ PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();
}

static
NTSTATUS
NTAPI
Pl2303AddDevice(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_EXTENSION DeviceExtension;

    PAGED_CODE();

    /* TODO: IoCreateDeviceSecure? */
    Status = IoCreateDevice(DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_SERIAL_PORT,
                            FILE_DEVICE_SECURE_OPEN, /* TODO: ? */
                            FALSE,
                            &DeviceObject);

    if (!NT_SUCCESS(Status))
        return Status;

    DeviceExtension = DeviceObject->DeviceExtension;

    /* TODO: verify we can do this */
    DeviceObject->Flags |= DO_POWER_PAGABLE;

    DeviceObject->Flags |= DO_DIRECT_IO;

    /* TODO: IoAttachDeviceToDeviceStackSafe? */
    DeviceExtension->NextDevice = IoAttachDeviceToDeviceStack(DeviceObject,
                                                              PhysicalDeviceObject);
    if (!DeviceExtension->NextDevice)
    {
        IoDeleteDevice(DeviceObject);
        return STATUS_NO_SUCH_DEVICE;
    }

    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
Pl2303DispatchPnp(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;
    PDEVICE_EXTENSION DeviceExtension;

    PAGED_CODE();

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IoStack->MajorFunction == IRP_MJ_PNP);

    DeviceExtension = DeviceObject->DeviceExtension;

    if (DeviceExtension->PnpState == Deleted)
    {
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    switch (IoStack->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            if (IoForwardIrpSynchronously(DeviceExtension->NextDevice, Irp))
                Status = Irp->IoStatus.Status;
            else
                Status = STATUS_UNSUCCESSFUL;

            if (NT_SUCCESS(Status))
                DeviceExtension->PnpState = Started;

            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return Status;
        case IRP_MN_QUERY_STOP_DEVICE:
            DeviceExtension->PreviousPnpState = DeviceExtension->PnpState;
            DeviceExtension->PnpState = StopPending;
            break;
        case IRP_MN_QUERY_REMOVE_DEVICE:
            DeviceExtension->PreviousPnpState = DeviceExtension->PnpState;
            DeviceExtension->PnpState = RemovePending;
            break;
        case IRP_MN_CANCEL_REMOVE_DEVICE:
        case IRP_MN_CANCEL_STOP_DEVICE:
            DeviceExtension->PnpState = DeviceExtension->PreviousPnpState;
            break;
        case IRP_MN_STOP_DEVICE:
            DeviceExtension->PnpState = Stopped;
            break;
        case IRP_MN_SURPRISE_REMOVAL:
            DeviceExtension->PnpState = SurpriseRemovePending;
            break;
        case IRP_MN_REMOVE_DEVICE:
            DeviceExtension->PnpState = Deleted;
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Status = IoCallDriver(DeviceExtension->NextDevice, Irp);
            IoDetachDevice(DeviceExtension->NextDevice);
            IoDeleteDevice(DeviceObject);
            return Status;
        default:
            /* Unsupported request - leave Irp->IoStack.Status untouched */
            IoSkipCurrentIrpStackLocation(Irp);
            return IoCallDriver(DeviceExtension->NextDevice, Irp);
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DeviceExtension->NextDevice, Irp);
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

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IoStack->MajorFunction == IRP_MJ_POWER);

    DeviceExtension = DeviceObject->DeviceExtension;

    if (DeviceExtension->PnpState == Deleted)
    {
        PoStartNextPowerIrp(Irp);
        Status = STATUS_NO_SUCH_DEVICE;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(DeviceObject, Irp);
}

static
NTSTATUS
NTAPI
Pl2303DispatchCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IoStack->MajorFunction == IRP_MJ_CREATE ||
           IoStack->MajorFunction == IRP_MJ_CLOSE);

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
