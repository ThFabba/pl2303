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
    HANDLE KeyHandle;
    UNICODE_STRING ValueName;
    PKEY_VALUE_PARTIAL_INFORMATION ValueInformation;
    ULONG ValueInformationLength;
    ULONG SkipExternalNaming;
    USHORT ComPortNameLength;
    PWCHAR ComPortNameBuffer = NULL;
    const UNICODE_STRING DosDevices = RTL_CONSTANT_STRING(L"\\DosDevices\\");
    PCONFIGURATION_INFORMATION ConfigInfo;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, PhysicalDeviceObject=%p\n",
                __FUNCTION__, DeviceObject,    PhysicalDeviceObject);

    Status = IoRegisterDeviceInterface(PhysicalDeviceObject,
                                       &GUID_DEVINTERFACE_COMPORT,
                                       NULL,
                                       &DeviceExtension->InterfaceLinkName);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. IoRegisterDeviceInterface failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }

    Pl2303Debug(         "%s. Device Interface is '%wZ'\n",
                __FUNCTION__, &DeviceExtension->InterfaceLinkName);

    Status = IoOpenDeviceRegistryKey(PhysicalDeviceObject,
                                     PLUGPLAY_REGKEY_DEVICE,
                                     KEY_QUERY_VALUE,
                                     &KeyHandle);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. IoOpenDeviceRegistryKey failed with %08lx\n",
                    __FUNCTION__, Status);
        RtlFreeUnicodeString(&DeviceExtension->InterfaceLinkName);
        return Status;
    }

    ValueInformationLength = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data[sizeof(ULONG)]);
    ValueInformation = ExAllocatePoolWithTag(PagedPool, ValueInformationLength, PL2303_TAG);
    if (!ValueInformation)
    {
        Pl2303Error(         "%s. Allocating registry value information failed\n",
                    __FUNCTION__);
        RtlFreeUnicodeString(&DeviceExtension->InterfaceLinkName);
        return STATUS_NO_MEMORY;
    }
    RtlInitUnicodeString(&ValueName, L"SkipExternalNaming");
    Status = ZwQueryValueKey(KeyHandle,
                             &ValueName,
                             KeyValuePartialInformation,
                             ValueInformation,
                             ValueInformationLength,
                             &ValueInformationLength);

    if (NT_SUCCESS(Status) &&
        ValueInformationLength == (ULONG)FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION,
                                                      Data[sizeof(ULONG)]) &&
        ValueInformation->Type == REG_DWORD &&
        ValueInformation->DataLength == sizeof(ULONG))
    {
        SkipExternalNaming = *(PCULONG)ValueInformation->Data;
    }
    else
    {
        SkipExternalNaming = 0;
    }
    ExFreePoolWithTag(ValueInformation, PL2303_TAG);

    if (!SkipExternalNaming)
    {
        RtlInitUnicodeString(&ValueName, L"PortName");
        Status = ZwQueryValueKey(KeyHandle,
                                 &ValueName,
                                 KeyValuePartialInformation,
                                 NULL,
                                 0,
                                 &ValueInformationLength);
        if (Status == STATUS_BUFFER_TOO_SMALL)
        {
            ASSERT(ValueInformationLength != 0);
            ValueInformation = ExAllocatePoolWithTag(PagedPool,
                                                     ValueInformationLength,
                                                     PL2303_TAG);
            if (!ValueInformation)
            {
                Pl2303Error(         "%s. Allocating registry value information failed\n",
                            __FUNCTION__);
                RtlFreeUnicodeString(&DeviceExtension->InterfaceLinkName);
                return STATUS_NO_MEMORY;
            }
            Status = ZwQueryValueKey(KeyHandle,
                                     &ValueName,
                                     KeyValuePartialInformation,
                                     ValueInformation,
                                     ValueInformationLength,
                                     &ValueInformationLength);
            if (!NT_SUCCESS(Status))
            {
                Pl2303Error(         "%s. ZwQueryValueKey failed with %08lx\n",
                            __FUNCTION__, Status);
                ExFreePoolWithTag(ValueInformation, PL2303_TAG);
                RtlFreeUnicodeString(&DeviceExtension->InterfaceLinkName);
                return Status;
            }
            // TODO: handle these properly instead of asserting
            ASSERT(ValueInformation->Type == REG_SZ);
            ASSERT(ValueInformation->Data[ValueInformation->DataLength - 1] == 0);
            ASSERT(ValueInformation->Data[ValueInformation->DataLength - 2] == 0);
            ASSERT(ValueInformation->DataLength < MAXUSHORT);
            ComPortNameLength = DosDevices.Length + (USHORT)ValueInformation->DataLength;
            ComPortNameBuffer = ExAllocatePoolWithTag(PagedPool,
                                                      ComPortNameLength,
                                                      PL2303_TAG);
            if (!ComPortNameBuffer)
            {
                Pl2303Error(         "%s. Allocating COM port name failed\n",
                            __FUNCTION__);
                ExFreePoolWithTag(ValueInformation, PL2303_TAG);
                RtlFreeUnicodeString(&DeviceExtension->InterfaceLinkName);
                return STATUS_NO_MEMORY;
            }
            RtlInitEmptyUnicodeString(&DeviceExtension->ComPortName,
                                      ComPortNameBuffer,
                                      ComPortNameLength);
            RtlCopyUnicodeString(&DeviceExtension->ComPortName, &DosDevices);
            (VOID)RtlAppendUnicodeToString(&DeviceExtension->ComPortName,
                                           (PCWSTR)ValueInformation->Data);

            ExFreePoolWithTag(ValueInformation, PL2303_TAG);
        }
        else
        {
            Pl2303Debug(         "%s. ZwQueryValueKey failed with %08lx\n",
                        __FUNCTION__, Status);
            Status = STATUS_SUCCESS;
        }
    }
    ASSERT(DeviceExtension->ComPortName.Buffer == ComPortNameBuffer);

    Pl2303Debug(         "%s. COM Port name is is '%wZ'\n",
                __FUNCTION__, &DeviceExtension->ComPortName);

    ConfigInfo = IoGetConfigurationInformation();
    ConfigInfo->SerialCount++;

    Pl2303Debug(         "%s. New serial port count: %lu\n",
                __FUNCTION__, ConfigInfo->SerialCount);

    return Status;
}

static
NTSTATUS
Pl2303DestroyDevice(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;
    PCONFIGURATION_INFORMATION ConfigInfo;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p\n",
                __FUNCTION__, DeviceObject);

    ConfigInfo = IoGetConfigurationInformation();
    ConfigInfo->SerialCount--;

    if (DeviceExtension->ComPortName.Buffer)
        ExFreePoolWithTag(DeviceExtension->ComPortName.Buffer, PL2303_TAG);

    RtlFreeUnicodeString(&DeviceExtension->InterfaceLinkName);

    ExFreePoolWithTag(DeviceExtension->DeviceName.Buffer, PL2303_TAG);

    return STATUS_SUCCESS;
}

static
NTSTATUS
Pl2303StartDevice(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS Status;
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p\n",
                __FUNCTION__, DeviceObject);

    Status = IoSetDeviceInterfaceState(&DeviceExtension->InterfaceLinkName,
                                       TRUE);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. IoSetDeviceInterfaceState failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }

    if (DeviceExtension->ComPortName.Buffer)
    {
        Status = IoCreateSymbolicLink(&DeviceExtension->ComPortName,
                                      &DeviceExtension->DeviceName);
        if (!NT_SUCCESS(Status))
        {
            Pl2303Error(         "%s. IoCreateSymbolicLink failed with %08lx\n",
                        __FUNCTION__, Status);
            (VOID)IoSetDeviceInterfaceState(&DeviceExtension->InterfaceLinkName,
                                            FALSE);
            return Status;
        }
    }

    return Status;
}

static
NTSTATUS
Pl2303StopDevice(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS Status;
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p\n",
                __FUNCTION__, DeviceObject);

    if (DeviceExtension->ComPortName.Buffer)
        Status = IoDeleteSymbolicLink(&DeviceExtension->ComPortName);

    Status =  IoSetDeviceInterfaceState(&DeviceExtension->InterfaceLinkName,
                                        FALSE);

    return Status;
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
    UNICODE_STRING DeviceName;
    static INT DeviceNumber = 0;

    PAGED_CODE();

    Pl2303Debug(         "%s. DriverObject=%p, PhysicalDeviceObject=%p\n",
                __FUNCTION__, DriverObject,    PhysicalDeviceObject);

    DeviceName.MaximumLength = sizeof(L"\\Device\\Pl2303Serial999");
    DeviceName.Length = 0;
    DeviceName.Buffer = ExAllocatePoolWithTag(PagedPool,
                                              DeviceName.MaximumLength,
                                              PL2303_TAG);
    if (!DeviceName.Buffer)
    {
        Pl2303Error(         "%s. Allocating device name buffer failed\n",
                    __FUNCTION__);
        return STATUS_NO_MEMORY;
    }

    Status = RtlUnicodeStringPrintf(&DeviceName,
                                    L"\\Device\\Pl2303Serial%d",
                                    DeviceNumber++);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. RtlUnicodeStringPrintf failed with %08lx\n",
                    __FUNCTION__, Status);
        ExFreePoolWithTag(DeviceName.Buffer, PL2303_TAG);
        return Status;
    }

    Pl2303Debug(         "%s. Device Name is '%wZ'\n",
                __FUNCTION__, &DeviceName);
    Status = IoCreateDevice(DriverObject,
                            sizeof(DEVICE_EXTENSION),
                            &DeviceName,
                            FILE_DEVICE_SERIAL_PORT,
                            FILE_DEVICE_SECURE_OPEN,
                            TRUE,
                            &DeviceObject);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. IoCreateDevice failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }

    DeviceExtension = DeviceObject->DeviceExtension;
    RtlZeroMemory(DeviceExtension, sizeof(*DeviceExtension));
    DeviceExtension->DeviceName = DeviceName;

    /* TODO: verify we can do this */
    DeviceObject->Flags |= DO_POWER_PAGABLE;

    DeviceObject->Flags |= DO_BUFFERED_IO;

    ASSERT(DeviceExtension->LowerDevice == NULL);
    Status = IoAttachDeviceToDeviceStackSafe(DeviceObject,
                                             PhysicalDeviceObject,
                                             &DeviceExtension->LowerDevice);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. IoAttachDeviceToDeviceStackSafe failed with %08lx\n",
                    __FUNCTION__, Status);
        IoDeleteDevice(DeviceObject);
        return STATUS_NO_SUCH_DEVICE;
    }
    ASSERT(DeviceExtension->LowerDevice);

    Status = Pl2303InitializeDevice(DeviceObject, PhysicalDeviceObject);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303InitializeDevice failed with %08lx\n",
                    __FUNCTION__, Status);
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

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    ASSERT(IoStack->MajorFunction == IRP_MJ_PNP);

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

    switch (IoStack->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            if (IoForwardIrpSynchronously(DeviceExtension->LowerDevice, Irp))
                Status = Irp->IoStatus.Status;
            else
            {
                Pl2303Error(         "%s. IoForwardIrpSynchronously failed\n",
                            __FUNCTION__);
                Status = STATUS_UNSUCCESSFUL;
            }

            if (!NT_SUCCESS(Status))
            {
                Pl2303Warn(         "%s. IRP_MN_START_DEVICE failed with %08lx\n",
                           __FUNCTION__, Status);
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return Status;
            }

            Status = Pl2303StartDevice(DeviceObject);
            if (!NT_SUCCESS(Status))
            {
                Pl2303Error(         "%s. Pl2303StartDevice failed with %08lx\n",
                            __FUNCTION__, Status);
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return Status;
            }

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
            Status = Pl2303StopDevice(DeviceObject);
            if (!NT_SUCCESS(Status))
                Pl2303Warn(         "%s. Pl2303StopDevice failed with %08lx\n",
                           __FUNCTION__, Status);
            break;
        case IRP_MN_REMOVE_DEVICE:
            DeviceExtension->PreviousPnpState = DeviceExtension->PnpState;
            DeviceExtension->PnpState = Deleted;
            if (DeviceExtension->PreviousPnpState != SurpriseRemovePending)
            {
                Status = Pl2303StopDevice(DeviceObject);
                if (!NT_SUCCESS(Status))
                    Pl2303Warn(         "%s. Pl2303StopDevice failed with %08lx\n",
                               __FUNCTION__, Status);
            }
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
