/*
 * PL2303 USB-Serial Driver
 * Copyright (C) 2012-2019  Thomas Faber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

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
__drv_dispatchType(IRP_MJ_WRITE)
static DRIVER_DISPATCH Pl2303DispatchWrite;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, Pl2303Unload)
#pragma alloc_text(PAGE, Pl2303DispatchPower)
#pragma alloc_text(PAGE, Pl2303DispatchSystemControl)
#pragma alloc_text(PAGE, Pl2303DispatchCreate)
#pragma alloc_text(PAGE, Pl2303DispatchClose)
#pragma alloc_text(PAGE, Pl2303DispatchRead)
#pragma alloc_text(PAGE, Pl2303DispatchWrite)
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
    DriverObject->MajorFunction[IRP_MJ_WRITE] = Pl2303DispatchWrite;

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

static
NTSTATUS
NTAPI
Pl2303DispatchWrite(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Inout_ PIRP Irp)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IoStack;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    NT_ASSERT(IoStack->MajorFunction == IRP_MJ_WRITE);

    if (!IoStack->Parameters.Write.Length)
    {
        Status = STATUS_SUCCESS;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    Status = Pl2303UsbWrite(DeviceObject, Irp);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbWrite failed with %08lx\n",
                    __FUNCTION__, Status);
    }

    return Status;
}
