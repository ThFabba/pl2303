#include "pl2303.h"

static NTSTATUS Pl2303InitializeDevice(_In_ PDEVICE_OBJECT DeviceObject,
                                       _In_ PDEVICE_OBJECT PhysicalDeviceObject);
static NTSTATUS Pl2303DestroyDevice(_In_ PDEVICE_OBJECT DeviceObject);
static NTSTATUS Pl2303StartDevice(_In_ PDEVICE_OBJECT DeviceObject);
static NTSTATUS Pl2303StopDevice(_In_ PDEVICE_OBJECT DeviceObject);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Pl2303InitializeDevice)
#pragma alloc_text(PAGE, Pl2303DestroyDevice)
#pragma alloc_text(PAGE, Pl2303StartDevice)
#pragma alloc_text(PAGE, Pl2303StopDevice)
#pragma alloc_text(PAGE, Pl2303AddDevice)
#pragma alloc_text(PAGE, Pl2303DispatchPnp)
#endif /* defined ALLOC_PRAGMA */

static
NTSTATUS
Pl2303InitializeDevice(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS Status;
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;
    PCONFIGURATION_INFORMATION ConfigInfo;

    PAGED_CODE();

    Status = IoRegisterDeviceInterface(PhysicalDeviceObject,
                                       &GUID_DEVINTERFACE_COMPORT,
                                       NULL,
                                       &DeviceExtension->InterfaceLinkName);

    if (!NT_SUCCESS(Status))
        return Status;

    ConfigInfo = IoGetConfigurationInformation();
    ConfigInfo->SerialCount++;

    return Status;
}

static
NTSTATUS
Pl2303DestroyDevice(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    RtlFreeUnicodeString(&DeviceExtension->InterfaceLinkName);

    return STATUS_SUCCESS;
}

static
NTSTATUS
Pl2303StartDevice(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    return IoSetDeviceInterfaceState(&DeviceExtension->InterfaceLinkName,
                                     TRUE);
}

static
NTSTATUS
Pl2303StopDevice(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    return IoSetDeviceInterfaceState(&DeviceExtension->InterfaceLinkName,
                                     FALSE);
}

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

    Status = IoCreateDevice(DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_SERIAL_PORT,
                            FILE_DEVICE_SECURE_OPEN,
                            TRUE,
                            &DeviceObject);

    if (!NT_SUCCESS(Status))
        return Status;

    DeviceExtension = DeviceObject->DeviceExtension;
    RtlZeroMemory(DeviceExtension, sizeof(*DeviceExtension));

    /* TODO: verify we can do this */
    DeviceObject->Flags |= DO_POWER_PAGABLE;

    DeviceObject->Flags |= DO_BUFFERED_IO;

    ASSERT(DeviceExtension->LowerDevice == NULL);
    Status = IoAttachDeviceToDeviceStackSafe(DeviceObject,
                                             PhysicalDeviceObject,
                                             &DeviceExtension->LowerDevice);
    if (!NT_SUCCESS(Status))
    {
        IoDeleteDevice(DeviceObject);
        return STATUS_NO_SUCH_DEVICE;
    }
    ASSERT(DeviceExtension->LowerDevice);

    Status = Pl2303InitializeDevice(DeviceObject, PhysicalDeviceObject);
    if (!NT_SUCCESS(Status))
    {
        IoDetachDevice(DeviceExtension->LowerDevice);
        IoDeleteDevice(DeviceObject);
        return Status;
    }

    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

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
            if (IoForwardIrpSynchronously(DeviceExtension->LowerDevice, Irp))
                Status = Irp->IoStatus.Status;
            else
                Status = STATUS_UNSUCCESSFUL;

            if (NT_SUCCESS(Status))
            {
                Status = Pl2303StartDevice(DeviceObject);
                if (NT_SUCCESS(Status))
                    DeviceExtension->PnpState = Started;
            }

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
            (VOID)Pl2303StopDevice(DeviceObject);
            break;
        case IRP_MN_REMOVE_DEVICE:
            DeviceExtension->PreviousPnpState = DeviceExtension->PnpState;
            DeviceExtension->PnpState = Deleted;
            if (DeviceExtension->PreviousPnpState != SurpriseRemovePending)
                (VOID)Pl2303StopDevice(DeviceObject);
            Irp->IoStatus.Status = STATUS_SUCCESS;
            Status = IoCallDriver(DeviceExtension->LowerDevice, Irp);
            IoDetachDevice(DeviceExtension->LowerDevice);
            (VOID)Pl2303DestroyDevice(DeviceObject);
            IoDeleteDevice(DeviceObject);
            return Status;
        default:
            /* Unsupported request - leave Irp->IoStack.Status untouched */
            IoSkipCurrentIrpStackLocation(Irp);
            return IoCallDriver(DeviceExtension->LowerDevice, Irp);
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DeviceExtension->LowerDevice, Irp);
}
