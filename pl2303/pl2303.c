#define INITGUID
#include "pl2303.h"

DRIVER_INITIALIZE DriverEntry;
static DRIVER_UNLOAD Pl2303Unload;
__drv_dispatchType(IRP_MJ_POWER)
static DRIVER_DISPATCH Pl2303DispatchPower;
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
    ASSERT(IoStack->MajorFunction == IRP_MJ_POWER);

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
    ASSERT(IoStack->MajorFunction == IRP_MJ_SYSTEM_CONTROL);

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
NTSTATUS
NTAPI
Pl2303DispatchCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IoStack->MajorFunction == IRP_MJ_CREATE);

    Status = Pl2303UsbSetLine(DeviceObject, 115200, 0, 0, 8);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSetLine failed with %08lx\n",
                    __FUNCTION__, Status);
    }

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

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IoStack->MajorFunction == IRP_MJ_CLOSE);

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
    ASSERT(IoStack->MajorFunction == IRP_MJ_READ);

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
