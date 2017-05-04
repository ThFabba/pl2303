#include "pl2303.h"

static NTSTATUS Pl2303UsbSubmitUrb(_In_ PDEVICE_OBJECT DeviceObject, _In_ PURB Urb);
static NTSTATUS Pl2303UsbGetDescriptor(_In_ PDEVICE_OBJECT DeviceObject,
                                       _In_ UCHAR DescriptorType,
                                       _Out_ PVOID *Buffer,
                                       _Inout_ PULONG BufferLength);
static NTSTATUS Pl2303UsbVendorRead(_In_ PDEVICE_OBJECT DeviceObject,
                                    _Out_ UCHAR *Buffer,
                                    _In_ USHORT Value,
                                    _In_ USHORT Index);
static NTSTATUS Pl2303UsbVendorWrite(_In_ PDEVICE_OBJECT DeviceObject,
                                     _In_ USHORT Value,
                                     _In_ USHORT Index);
static NTSTATUS Pl2303UsbConfigureDevice(_In_ PDEVICE_OBJECT DeviceObject,
                                         _In_ PUSB_CONFIGURATION_DESCRIPTOR ConfigDescriptor,
                                         _In_ PUSB_INTERFACE_DESCRIPTOR InterfaceDescriptor);
static NTSTATUS Pl2303UsbUnconfigureDevice(_In_ PDEVICE_OBJECT DeviceObject);
_Function_class_(IO_COMPLETION_ROUTINE)
static NTSTATUS NTAPI Pl2303UsbReadCompletion(_In_ PDEVICE_OBJECT DeviceObject,
                                              _In_ PIRP Irp,
                                              _In_reads_(sizeof(URB)) PVOID Context);
_Function_class_(IO_COMPLETION_ROUTINE)
static NTSTATUS NTAPI Pl2303UsbWriteCompletion(_In_ PDEVICE_OBJECT DeviceObject,
                                               _In_ PIRP Irp,
                                               _In_reads_(sizeof(URB)) PVOID Context);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Pl2303UsbSubmitUrb)
#pragma alloc_text(PAGE, Pl2303UsbGetDescriptor)
#pragma alloc_text(PAGE, Pl2303UsbVendorRead)
#pragma alloc_text(PAGE, Pl2303UsbVendorWrite)
#pragma alloc_text(PAGE, Pl2303UsbConfigureDevice)
#pragma alloc_text(PAGE, Pl2303UsbUnconfigureDevice)
#pragma alloc_text(PAGE, Pl2303UsbStart)
#pragma alloc_text(PAGE, Pl2303UsbStop)
#pragma alloc_text(PAGE, Pl2303UsbSetLine)
#pragma alloc_text(PAGE, Pl2303UsbRead)
#pragma alloc_text(PAGE, Pl2303UsbWrite)
#endif /* defined ALLOC_PRAGMA */

static
NTSTATUS
Pl2303UsbSubmitUrb(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PURB Urb)
{
    NTSTATUS Status;
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;
    PIRP Irp;
    IO_STATUS_BLOCK IoStatus;
    PIO_STACK_LOCATION IoStack;
    KEVENT Event;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Urb=%p\n",
                __FUNCTION__, DeviceObject,    Urb);

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_SUBMIT_URB,
                                        DeviceExtension->LowerDevice,
                                        NULL,
                                        0,
                                        NULL,
                                        0,
                                        TRUE,
                                        &Event,
                                        &IoStatus);
    if (!Irp)
    {
        Pl2303Error(         "%s. Allocating IRP for submitting URB failed\n",
                    __FUNCTION__);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    IoStack = IoGetNextIrpStackLocation(Irp);
    NT_ASSERT(IoStack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL);
    IoStack->Parameters.Others.Argument1 = Urb;

    Status = IoCallDriver(DeviceExtension->LowerDevice, Irp);

    if (Status == STATUS_PENDING)
    {
        Status = KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        NT_ASSERT(Status == STATUS_SUCCESS);
        Status = IoStatus.Status;
    }

    return Status;
}

static
NTSTATUS
Pl2303UsbGetDescriptor(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ UCHAR DescriptorType,
    _Out_ PVOID *Buffer,
    _Inout_ PULONG BufferLength)
{
    NTSTATUS Status;
    PURB Urb;

    PAGED_CODE();
    NT_ASSERT(Buffer);
    NT_ASSERT(BufferLength);
    NT_ASSERT(*BufferLength > 0);

    Pl2303Debug(         "%s. DeviceObject=%p, DescriptorType=%u, Buffer=%p, BufferLength=%p\n",
                __FUNCTION__, DeviceObject,    DescriptorType,    Buffer,    BufferLength);

    Urb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                                PL2303_URB_TAG);
    if (!Urb)
    {
        Pl2303Error(         "%s. Allocating URB failed\n",
                    __FUNCTION__);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *Buffer = ExAllocatePoolWithTag(NonPagedPool, *BufferLength, PL2303_TAG);
    if (!*Buffer)
    {
        Pl2303Error(         "%s. Allocating URB transfer buffer of size %lu failed\n",
                    __FUNCTION__, *BufferLength);
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    UsbBuildGetDescriptorRequest(Urb,
                                 sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST),
                                 DescriptorType,
                                 0,
                                 0,
                                 *Buffer,
                                 NULL,
                                 *BufferLength,
                                 NULL);

    Status = Pl2303UsbSubmitUrb(DeviceObject, Urb);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx, %08lx\n",
                    __FUNCTION__, Status, Urb->UrbHeader.Status);
        ExFreePoolWithTag(*Buffer, PL2303_TAG);
        *Buffer = NULL;
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        *BufferLength = 0;
        return Status;
    }
    if (!USBD_SUCCESS(Urb->UrbHeader.Status))
    {
        Pl2303Error(         "%s. Urb failed with %08lx\n",
                    __FUNCTION__, Urb->UrbHeader.Status);
        Status = Urb->UrbHeader.Status;
        ExFreePoolWithTag(*Buffer, PL2303_TAG);
        *Buffer = NULL;
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        *BufferLength = 0;
        return Status;
    }

    *BufferLength = Urb->UrbControlDescriptorRequest.TransferBufferLength;
    ExFreePoolWithTag(Urb, PL2303_URB_TAG);

    return Status;
}

static
NTSTATUS
Pl2303UsbVendorRead(
    _In_ PDEVICE_OBJECT DeviceObject,
    _Out_ UCHAR *Buffer,
    _In_ USHORT Value,
    _In_ USHORT Index)
{
    NTSTATUS Status;
    PURB Urb;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Buffer=%p, Value=0x%x, Index=0x%x\n",
                __FUNCTION__, DeviceObject,    Buffer,    Value,      Index);

    Urb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                                PL2303_URB_TAG);
    if (!Urb)
    {
        Pl2303Error(         "%s. Allocating URB failed\n",
                    __FUNCTION__);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    UsbBuildVendorRequest(Urb,
                          URB_FUNCTION_VENDOR_DEVICE,
                          sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                          USBD_TRANSFER_DIRECTION_IN | USBD_SHORT_TRANSFER_OK,
                          0,
                          PL2303_VENDOR_READ_REQUEST,
                          Value,
                          Index,
                          Buffer,
                          NULL,
                          1,
                          NULL);

    Status = Pl2303UsbSubmitUrb(DeviceObject, Urb);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx, %08lx\n",
                    __FUNCTION__, Status, Urb->UrbHeader.Status);
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }
    if (!USBD_SUCCESS(Urb->UrbHeader.Status))
    {
        Pl2303Error(         "%s. URB failed with %08lx\n",
                    __FUNCTION__, Urb->UrbHeader.Status);
        Status = Urb->UrbHeader.Status;
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }

    Pl2303Debug(         "%s. Vendor Read 0x%x/0x%x returned length %lu: 0x%x\n",
                __FUNCTION__, Value,
                              Index,
                              Urb->UrbControlVendorClassRequest.TransferBufferLength,
                              Buffer[0]);

    ExFreePoolWithTag(Urb, PL2303_URB_TAG);

    return Status;
}

static
NTSTATUS
Pl2303UsbVendorWrite(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ USHORT Value,
    _In_ USHORT Index)
{
    NTSTATUS Status;
    PURB Urb;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Value=0x%x, Index=0x%x\n",
                __FUNCTION__, DeviceObject,    Value,      Index);

    Urb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                                PL2303_URB_TAG);
    if (!Urb)
    {
        Pl2303Error(         "%s. Allocating URB failed\n",
                    __FUNCTION__);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    UsbBuildVendorRequest(Urb,
                          URB_FUNCTION_VENDOR_DEVICE,
                          sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                          USBD_TRANSFER_DIRECTION_OUT,
                          0,
                          PL2303_VENDOR_WRITE_REQUEST,
                          Value,
                          Index,
                          NULL,
                          NULL,
                          0,
                          NULL);

    Status = Pl2303UsbSubmitUrb(DeviceObject, Urb);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx, %08lx\n",
                    __FUNCTION__, Status, Urb->UrbHeader.Status);
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }
    if (!USBD_SUCCESS(Urb->UrbHeader.Status))
    {
        Pl2303Error(         "%s. URB failed with %08lx\n",
                    __FUNCTION__, Urb->UrbHeader.Status);
        Status = Urb->UrbHeader.Status;
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }
    ExFreePoolWithTag(Urb, PL2303_URB_TAG);

    return Status;
}

static
NTSTATUS
Pl2303UsbConfigureDevice(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PUSB_CONFIGURATION_DESCRIPTOR ConfigDescriptor,
    _In_ PUSB_INTERFACE_DESCRIPTOR InterfaceDescriptor)
{
    NTSTATUS Status;
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;
    PURB Urb;
    USBD_INTERFACE_LIST_ENTRY InterfaceList[2];
    ULONG i;
    PUSBD_PIPE_INFORMATION PipeInfo;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, ConfigDescriptor=%p, InterfaceDescriptor=%p\n",
                __FUNCTION__, DeviceObject,    ConfigDescriptor,    InterfaceDescriptor);

    RtlZeroMemory(InterfaceList, sizeof(InterfaceList));
    InterfaceList[0].InterfaceDescriptor = InterfaceDescriptor;

    Urb = USBD_CreateConfigurationRequestEx(ConfigDescriptor,
                                            InterfaceList);
    if (!Urb)
    {
        Pl2303Error(         "%s. USBD_CreateConfigurationRequestEx failed\n",
                    __FUNCTION__);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = Pl2303UsbSubmitUrb(DeviceObject, Urb);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx, %08lx\n",
                    __FUNCTION__, Status, Urb->UrbHeader.Status);
        ExFreePoolWithTag(Urb, 0);
        return Status;
    }

    Pl2303Debug(         "%s. NumberOfPipes=%u\n",
                    __FUNCTION__, Urb->UrbSelectConfiguration.Interface.NumberOfPipes);

    for (i = 0; i < Urb->UrbSelectConfiguration.Interface.NumberOfPipes; i++)
    {
        PipeInfo = &Urb->UrbSelectConfiguration.Interface.Pipes[i];
        
        if (PipeInfo->PipeType == UsbdPipeTypeBulk &&
            USB_ENDPOINT_DIRECTION_IN(PipeInfo->EndpointAddress) &&
            !DeviceExtension->BulkInPipe)
        {
            DeviceExtension->BulkInPipe = PipeInfo->PipeHandle;
        }

        if (PipeInfo->PipeType == UsbdPipeTypeBulk &&
            USB_ENDPOINT_DIRECTION_OUT(PipeInfo->EndpointAddress) &&
            !DeviceExtension->BulkOutPipe)
        {
            DeviceExtension->BulkOutPipe = PipeInfo->PipeHandle;
        }

        if (PipeInfo->PipeType == UsbdPipeTypeInterrupt &&
            USB_ENDPOINT_DIRECTION_IN(PipeInfo->EndpointAddress) &&
            !DeviceExtension->InterruptInPipe)
        {
            DeviceExtension->InterruptInPipe = PipeInfo->PipeHandle;
        }
    }

    if (!DeviceExtension->BulkInPipe ||
        !DeviceExtension->BulkOutPipe ||
        !DeviceExtension->InterruptInPipe)
    {
        Pl2303Error(         "%s. Invalid endpoint combination\n",
                    __FUNCTION__, Status);
        NT_ASSERT(DeviceExtension->BulkInPipe &&
                  DeviceExtension->BulkOutPipe &&
                  DeviceExtension-> InterruptInPipe);
        ExFreePoolWithTag(Urb, 0);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    ExFreePoolWithTag(Urb, 0);

    return Status;
}

static
NTSTATUS
Pl2303UsbUnconfigureDevice(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS Status;
    PURB Urb;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p\n",
                __FUNCTION__, DeviceObject);

    Urb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(struct _URB_SELECT_CONFIGURATION),
                                PL2303_URB_TAG);
    if (!Urb)
    {
        Pl2303Error(         "%s. Allocating URB failed\n",
                    __FUNCTION__);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    UsbBuildSelectConfigurationRequest(Urb,
                                       sizeof(struct _URB_SELECT_CONFIGURATION),
                                       NULL);

    Status = Pl2303UsbSubmitUrb(DeviceObject, Urb);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx, %08lx\n",
                    __FUNCTION__, Status, Urb->UrbHeader.Status);
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }
    ExFreePoolWithTag(Urb, PL2303_URB_TAG);

    return Status;
}

NTSTATUS
Pl2303UsbStart(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS Status;
    PVOID Descriptor;
    ULONG DescriptorLength;
    PUSB_DEVICE_DESCRIPTOR DeviceDescriptor;
    PUSB_CONFIGURATION_DESCRIPTOR ConfigDescriptor;
    PUSB_INTERFACE_DESCRIPTOR InterfaceDescriptor;
    UCHAR Buffer[1];
    PDEVICE_EXTENSION DeviceExtension;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p\n",
                __FUNCTION__, DeviceObject);

    DeviceExtension = DeviceObject->DeviceExtension;

    DescriptorLength = sizeof(USB_DEVICE_DESCRIPTOR);
    Status = Pl2303UsbGetDescriptor(DeviceObject,
                                    USB_DEVICE_DESCRIPTOR_TYPE,
                                    &Descriptor,
                                    &DescriptorLength);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbGetDescriptor failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    NT_ASSERT(DescriptorLength == sizeof(USB_DEVICE_DESCRIPTOR));

    DeviceDescriptor = Descriptor;

    Pl2303Debug(         "%s. Device descriptor: "
                                               "bLength=%u, "
                                               "bDescriptorType=%u, "
                                               "bcdUSB=0x%x, "
                                               "bDeviceClass=0x%x, "
                                               "bDeviceSubClass=0x%x, "
                                               "bDeviceProtocol=0x%x, "
                                               "bMaxPacketSize0=%u, "
                                               "idVendor=0x%x, "
                                               "idProduct=0x%x, "
                                               "bcdDevice=0x%x, "
                                               "iManufacturer=%u, "
                                               "iProduct=%u, "
                                               "iSerialNumber=%u, "
                                               "bNumConfigurations=%u\n",
                __FUNCTION__, DeviceDescriptor->bLength,
                              DeviceDescriptor->bDescriptorType,
                              DeviceDescriptor->bcdUSB,
                              DeviceDescriptor->bDeviceClass,
                              DeviceDescriptor->bDeviceSubClass,
                              DeviceDescriptor->bDeviceProtocol,
                              DeviceDescriptor->bMaxPacketSize0,
                              DeviceDescriptor->idVendor,
                              DeviceDescriptor->idProduct,
                              DeviceDescriptor->bcdDevice,
                              DeviceDescriptor->iManufacturer,
                              DeviceDescriptor->iProduct,
                              DeviceDescriptor->iSerialNumber,
                              DeviceDescriptor->bNumConfigurations);

    /* We only support PL2303 HX right now */
    NT_ASSERT(DeviceDescriptor->bDeviceClass != USB_DEVICE_CLASS_COMMUNICATIONS);
    NT_ASSERT(DeviceDescriptor->bMaxPacketSize0 == 64);

    ExFreePoolWithTag(Descriptor, PL2303_TAG);

    DescriptorLength = sizeof(USB_CONFIGURATION_DESCRIPTOR);
    Status = Pl2303UsbGetDescriptor(DeviceObject,
                                    USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                    &Descriptor,
                                    &DescriptorLength);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbGetDescriptor failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    NT_ASSERT(DescriptorLength == sizeof(USB_CONFIGURATION_DESCRIPTOR));

    ConfigDescriptor = Descriptor;
    NT_ASSERT(ConfigDescriptor->wTotalLength != 0);
    DescriptorLength = ConfigDescriptor->wTotalLength;
    ExFreePoolWithTag(Descriptor, PL2303_TAG);
    Status = Pl2303UsbGetDescriptor(DeviceObject,
                                    USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                    &Descriptor,
                                    &DescriptorLength);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbGetDescriptor failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }

    ConfigDescriptor = Descriptor;
    NT_ASSERT(DescriptorLength == ConfigDescriptor->wTotalLength);

    Pl2303Debug(         "%s. Config descriptor: "
                                               "bLength=%u, "
                                               "bDescriptorType=%u, "
                                               "wTotalLength=%u, "
                                               "bNumInterfaces=%u, "
                                               "bConfigurationValue=%u, "
                                               "iConfiguration=%u, "
                                               "bmAttributes=0x%x, "
                                               "MaxPower=%u\n",
                __FUNCTION__, ConfigDescriptor->bLength,
                              ConfigDescriptor->bDescriptorType,
                              ConfigDescriptor->wTotalLength,
                              ConfigDescriptor->bNumInterfaces,
                              ConfigDescriptor->bConfigurationValue,
                              ConfigDescriptor->iConfiguration,
                              ConfigDescriptor->bmAttributes,
                              ConfigDescriptor->MaxPower);

    if (ConfigDescriptor->bNumInterfaces != 1)
    {
        Pl2303Error(         "%s. Configuration contains %u interfaces, expected one\n",
                    __FUNCTION__, ConfigDescriptor->bNumInterfaces);
        ExFreePoolWithTag(Descriptor, PL2303_TAG);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    InterfaceDescriptor = USBD_ParseConfigurationDescriptorEx(ConfigDescriptor,
                                                              ConfigDescriptor,
                                                              -1,
                                                              -1,
                                                              -1,
                                                              -1,
                                                              -1);
    if (!InterfaceDescriptor)
    {
        Pl2303Error(         "%s. USBD_ParseConfigurationDescriptorEx failed\n",
                    __FUNCTION__);
        ExFreePoolWithTag(Descriptor, PL2303_TAG);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    Pl2303Debug(         "%s. Interface descriptor: "
                                                  "bLength=%u, "
                                                  "bDescriptorType=%u, "
                                                  "bInterfaceNumber=%u, "
                                                  "bAlternateSetting=%u, "
                                                  "bNumEndpoints=%u, "
                                                  "bInterfaceClass=0x%x, "
                                                  "bInterfaceSubClass=0x%x, "
                                                  "bInterfaceProtocol=0x%x, "
                                                  "iInterface=%u\n",
                __FUNCTION__, InterfaceDescriptor->bLength,
                              InterfaceDescriptor->bDescriptorType,
                              InterfaceDescriptor->bInterfaceNumber,
                              InterfaceDescriptor->bAlternateSetting,
                              InterfaceDescriptor->bNumEndpoints,
                              InterfaceDescriptor->bInterfaceClass,
                              InterfaceDescriptor->bInterfaceSubClass,
                              InterfaceDescriptor->bInterfaceProtocol,
                              InterfaceDescriptor->iInterface);

    Status = Pl2303UsbConfigureDevice(DeviceObject, ConfigDescriptor, InterfaceDescriptor);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbConfigureDevice failed with %08lx\n",
                    __FUNCTION__, Status);
        ExFreePoolWithTag(Descriptor, PL2303_TAG);
        return Status;
    }
    ExFreePoolWithTag(Descriptor, PL2303_TAG);

    /* TODO: Buffer should probably be nonpaged */
    Status = Pl2303UsbVendorRead(DeviceObject, Buffer, 0x8484, 0); // expect: 2
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorRead[1] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorWrite(DeviceObject, 0x0404, 0);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorWrite[2] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorRead(DeviceObject, Buffer, 0x8484, 0); // expect: 2
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorRead[3] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorRead(DeviceObject, Buffer, 0x8383, 0); // expect: 0
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorRead[4] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorRead(DeviceObject, Buffer, 0x8484, 0); // expect: 2
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorRead[5] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorWrite(DeviceObject, 0x0404, 0);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorWrite[6] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorRead(DeviceObject, Buffer, 0x8484, 0); // expect: 2
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorRead[7] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorRead(DeviceObject, Buffer, 0x8383, 0); // expect: 0
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorRead[8] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorWrite(DeviceObject, 0, 1);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorWrite[9] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorWrite(DeviceObject, 1, 0);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorWrite[10] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }
    Status = Pl2303UsbVendorWrite(DeviceObject, 2, 0x44); // non-HX has 0x24 here instead of 0x44
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbVendorWrite[11] failed with %08lx\n",
                    __FUNCTION__, Status);
        return Status;
    }

    return Status;
}

NTSTATUS
Pl2303UsbStop(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS Status;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p\n",
                __FUNCTION__, DeviceObject);

    Status = Pl2303UsbUnconfigureDevice(DeviceObject);

    return Status;
}

NTSTATUS
Pl2303UsbSetLine(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ ULONG BaudRate,
    _In_ UCHAR StopBits,
    _In_ UCHAR Parity,
    _In_ UCHAR DataBits)
{
    NTSTATUS Status;
    struct _LINE
    {
        ULONG BaudRate;
        UCHAR StopBits;
        UCHAR Parity;
        UCHAR DataBits;
    } Line;
    PURB Urb;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, BaudRate=%lu, StopBits=%u, Parity=%u, "
                             "DataBits=%u\n",
                __FUNCTION__, DeviceObject,    BaudRate,     StopBits,    Parity,
                              DataBits);

    Urb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                                PL2303_URB_TAG);
    if (!Urb)
    {
        Pl2303Error(         "%s. Allocating URB failed\n",
                    __FUNCTION__);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    UsbBuildVendorRequest(Urb,
                          URB_FUNCTION_CLASS_DEVICE,
                          sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                          USBD_TRANSFER_DIRECTION_OUT,
                          0,
                          PL2303_SET_LINE_REQUEST,
                          0,
                          0,
                          &Line,
                          NULL,
                          sizeof(Line),
                          NULL);

    /* TODO: this should probably be nonpaged */
    Line.BaudRate = BaudRate;
    Line.StopBits = StopBits;
    Line.Parity = Parity;
    Line.DataBits = DataBits;

    Status = Pl2303UsbSubmitUrb(DeviceObject, Urb);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx, %08lx\n",
                    __FUNCTION__, Status, Urb->UrbHeader.Status);
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }
    if (!USBD_SUCCESS(Urb->UrbHeader.Status))
    {
        Pl2303Error(         "%s. URB failed with %08lx\n",
                    __FUNCTION__, Urb->UrbHeader.Status);
        Status = Urb->UrbHeader.Status;
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }
    ExFreePoolWithTag(Urb, PL2303_URB_TAG);

    return Status;
}

NTSTATUS
Pl2303UsbSetControlLines(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ USHORT DtrRts)
{
    NTSTATUS Status;
    PURB Urb;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, DtrRts=%u\n",
                __FUNCTION__, DeviceObject,    DtrRts);

    Urb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                                PL2303_URB_TAG);
    if (!Urb)
    {
        Pl2303Error(         "%s. Allocating URB failed\n",
                    __FUNCTION__);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    NT_ASSERT((DtrRts & ~(SERIAL_DTR_STATE | SERIAL_RTS_STATE)) == 0);
    UsbBuildVendorRequest(Urb,
                          URB_FUNCTION_CLASS_DEVICE,
                          sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
                          USBD_TRANSFER_DIRECTION_OUT,
                          0,
                          PL2303_SET_CONTROL_REQUEST,
                          DtrRts,
                          0,
                          NULL,
                          NULL,
                          0,
                          NULL);

    Status = Pl2303UsbSubmitUrb(DeviceObject, Urb);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx, %08lx\n",
                    __FUNCTION__, Status, Urb->UrbHeader.Status);
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }
    if (!USBD_SUCCESS(Urb->UrbHeader.Status))
    {
        Pl2303Error(         "%s. URB failed with %08lx\n",
                    __FUNCTION__, Urb->UrbHeader.Status);
        Status = Urb->UrbHeader.Status;
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        return Status;
    }
    ExFreePoolWithTag(Urb, PL2303_URB_TAG);

    return Status;
}

_Function_class_(IO_COMPLETION_ROUTINE)
static
NTSTATUS
NTAPI
Pl2303UsbReadCompletion(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp,
    _In_reads_(sizeof(URB)) PVOID Context)
{
    PURB Urb = Context;

    NT_ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p, Context=%p\n",
                __FUNCTION__, DeviceObject,    Irp,    Context);

    if (NT_SUCCESS(Irp->IoStatus.Status))
    {
        if (USBD_SUCCESS(Urb->UrbHeader.Status))
            Irp->IoStatus.Information = Urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        else
            Pl2303Warn(         "%s. URB failed with %08lx\n",
                       __FUNCTION__, Urb->UrbHeader.Status);
    }
    else
    {
        Pl2303Warn(         "%s. IRP failed with %08lx\n",
                   __FUNCTION__, Irp->IoStatus.Status);
    }

    ExFreePoolWithTag(Urb, PL2303_URB_TAG);

    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS
Pl2303UsbRead(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Status;
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;
    PURB Urb;
    PIO_STACK_LOCATION IoStack;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    Urb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                PL2303_URB_TAG);
    if (!Urb)
    {
        Pl2303Error(         "%s. Allocating URB failed\n",
                    __FUNCTION__);
        Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    UsbBuildInterruptOrBulkTransferRequest(Urb,
                                           sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                           DeviceExtension->BulkInPipe,
                                           Irp->AssociatedIrp.SystemBuffer,
                                           NULL,
                                           IoStack->Parameters.Read.Length,
                                           USBD_TRANSFER_DIRECTION_IN | USBD_SHORT_TRANSFER_OK,
                                           NULL);

    IoStack = IoGetNextIrpStackLocation(Irp);
    IoStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    IoStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
    IoStack->Parameters.Others.Argument1 = Urb;

    Status = IoSetCompletionRoutineEx(DeviceObject,
                                      Irp,
                                      Pl2303UsbReadCompletion,
                                      Urb,
                                      TRUE,
                                      TRUE,
                                      TRUE);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. IoSetCompletionRoutineEx failed with %08lx\n",
                    __FUNCTION__, Status);
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    IoMarkIrpPending(Irp);
    (VOID)IoCallDriver(DeviceExtension->LowerDevice, Irp);

    return STATUS_PENDING;
}


_Function_class_(IO_COMPLETION_ROUTINE)
static
NTSTATUS
NTAPI
Pl2303UsbWriteCompletion(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp,
    _In_reads_(sizeof(URB)) PVOID Context)
{
    PURB Urb = Context;

    NT_ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p, Context=%p\n",
                __FUNCTION__, DeviceObject,    Irp,    Context);

    if (NT_SUCCESS(Irp->IoStatus.Status))
    {
        if (USBD_SUCCESS(Urb->UrbHeader.Status))
            Irp->IoStatus.Information = Urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
        else
            Pl2303Warn(         "%s. URB failed with %08lx\n",
                       __FUNCTION__, Urb->UrbHeader.Status);
    }
    else
    {
        Pl2303Warn(         "%s. IRP failed with %08lx\n",
                   __FUNCTION__, Irp->IoStatus.Status);
    }

    ExFreePoolWithTag(Urb, PL2303_URB_TAG);

    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS
Pl2303UsbWrite(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    NTSTATUS Status;
    PDEVICE_EXTENSION DeviceExtension = DeviceObject->DeviceExtension;
    PURB Urb;
    PIO_STACK_LOCATION IoStack;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p, Irp=%p\n",
                __FUNCTION__, DeviceObject,    Irp);

    Urb = ExAllocatePoolWithTag(NonPagedPool,
                                sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                PL2303_URB_TAG);
    if (!Urb)
    {
        Pl2303Error(         "%s. Allocating URB failed\n",
                    __FUNCTION__);
        Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    IoStack = IoGetCurrentIrpStackLocation(Irp);
    UsbBuildInterruptOrBulkTransferRequest(Urb,
                                           sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                                           DeviceExtension->BulkOutPipe,
                                           Irp->AssociatedIrp.SystemBuffer,
                                           NULL,
                                           IoStack->Parameters.Write.Length,
                                           USBD_TRANSFER_DIRECTION_OUT,
                                           NULL);

    IoStack = IoGetNextIrpStackLocation(Irp);
    IoStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    IoStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
    IoStack->Parameters.Others.Argument1 = Urb;

    Status = IoSetCompletionRoutineEx(DeviceObject,
                                      Irp,
                                      Pl2303UsbWriteCompletion,
                                      Urb,
                                      TRUE,
                                      TRUE,
                                      TRUE);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. IoSetCompletionRoutineEx failed with %08lx\n",
                    __FUNCTION__, Status);
        Irp->IoStatus.Information = 0;
        Irp->IoStatus.Status = Status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return Status;
    }

    IoMarkIrpPending(Irp);
    (VOID)IoCallDriver(DeviceExtension->LowerDevice, Irp);

    return STATUS_PENDING;
}