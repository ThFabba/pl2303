#define INITGUID
#include "pl2303.h"

DRIVER_INITIALIZE DriverEntry;
static DRIVER_UNLOAD Pl2303Unload;
__drv_dispatchType(IRP_MJ_POWER)
static DRIVER_DISPATCH Pl2303DispatchPower;
__drv_dispatchType(IRP_MJ_CREATE)
__drv_dispatchType(IRP_MJ_CLOSE)
static DRIVER_DISPATCH Pl2303DispatchCreateClose;
__drv_dispatchType(IRP_MJ_READ)
static DRIVER_DISPATCH Pl2303DispatchRead;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Pl2303Unload)
#pragma alloc_text(PAGE, Pl2303DispatchPower)
#pragma alloc_text(PAGE, Pl2303DispatchCreateClose)
#pragma alloc_text(PAGE, Pl2303DispatchRead)
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
    DriverObject->MajorFunction[IRP_MJ_READ] = Pl2303DispatchRead;

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

static
NTSTATUS
NTAPI
Pl2303DispatchRead(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IoStack;

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IoStack->MajorFunction == IRP_MJ_READ);

    *(PCHAR)Irp->AssociatedIrp.SystemBuffer = 'A';
    Irp->IoStatus.Information = 1;

    Status = STATUS_SUCCESS;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return Status;
}
