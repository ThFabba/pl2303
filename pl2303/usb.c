#include "pl2303.h"

static NTSTATUS Pl2303UsbSubmitUrb(_In_ PDEVICE_OBJECT DeviceObject, _In_ PURB Urb);
static NTSTATUS Pl2303UsbGetDescriptor(_In_ PDEVICE_OBJECT DeviceObject,
                                       _In_ UCHAR DescriptorType,
                                       _Out_ PVOID *Buffer,
                                       _Inout_ PULONG BufferLength);
static NTSTATUS Pl2303UsbConfigureDevice(_In_ PDEVICE_OBJECT DeviceObject,
                                         _In_ PUSB_CONFIGURATION_DESCRIPTOR ConfigDescriptor,
                                         _In_ PUSB_INTERFACE_DESCRIPTOR InterfaceDescriptor);
static NTSTATUS Pl2303UsbUnconfigureDevice(_In_ PDEVICE_OBJECT DeviceObject);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Pl2303UsbSubmitUrb)
#pragma alloc_text(PAGE, Pl2303UsbGetDescriptor)
#pragma alloc_text(PAGE, Pl2303UsbConfigureDevice)
#pragma alloc_text(PAGE, Pl2303UsbUnconfigureDevice)
#pragma alloc_text(PAGE, Pl2303UsbStart)
#pragma alloc_text(PAGE, Pl2303UsbStop)
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
    ASSERT(IoStack->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL);
    IoStack->Parameters.Others.Argument1 = Urb;

    Status = IoCallDriver(DeviceExtension->LowerDevice, Irp);

    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
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
    ASSERT(Buffer);
    ASSERT(BufferLength);
    ASSERT(*BufferLength > 0);

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
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx\n",
                    __FUNCTION__, Status);
        ExFreePoolWithTag(*Buffer, PL2303_TAG);
        *Buffer = NULL;
        ExFreePoolWithTag(Urb, PL2303_URB_TAG);
        *BufferLength = 0;
        return Status;
    }
    if (!NT_SUCCESS(Urb->UrbHeader.Status))
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
Pl2303UsbConfigureDevice(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PUSB_CONFIGURATION_DESCRIPTOR ConfigDescriptor,
    _In_ PUSB_INTERFACE_DESCRIPTOR InterfaceDescriptor)
{
    NTSTATUS Status;
    PURB Urb;
    USBD_INTERFACE_LIST_ENTRY InterfaceList[2];

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

    Pl2303Debug(         "%s. MaximumTransferSize=%u\n",
                __FUNCTION__, Urb->UrbSelectConfiguration.Interface.Pipes[0].MaximumTransferSize);

    Status = Pl2303UsbSubmitUrb(DeviceObject, Urb);
    if (!NT_SUCCESS(Status))
    {
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx\n",
                    __FUNCTION__, Status);
        ExFreePool(Urb);
        return Status;
    }

    Pl2303Debug(         "%s. NumberOfPipes=%u\n",
                    __FUNCTION__, Urb->UrbSelectConfiguration.Interface.NumberOfPipes);

    ExFreePool(Urb);

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
        Pl2303Error(         "%s. Pl2303UsbSubmitUrb failed with %08lx\n",
                    __FUNCTION__, Status);
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

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p\n",
                __FUNCTION__, DeviceObject);

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
    ASSERT(DescriptorLength == sizeof(USB_DEVICE_DESCRIPTOR));

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
    ASSERT(DescriptorLength == sizeof(USB_CONFIGURATION_DESCRIPTOR));

    ConfigDescriptor = Descriptor;
    ASSERT(ConfigDescriptor->wTotalLength != 0);
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
    ASSERT(DescriptorLength == ConfigDescriptor->wTotalLength);

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
                                                              0,
                                                              0,
                                                              USB_DEVICE_CLASS_VENDOR_SPECIFIC,
                                                              0,
                                                              0);
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
