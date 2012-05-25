#include "pl2303.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Pl2303UsbStart)
#pragma alloc_text(PAGE, Pl2303UsbStop)
#endif /* defined ALLOC_PRAGMA */

NTSTATUS
Pl2303UsbStart(
    _In_ PDEVICE_OBJECT DeviceObject)
{
    NTSTATUS Status;

    PAGED_CODE();

    Pl2303Debug(         "%s. DeviceObject=%p\n",
                __FUNCTION__, DeviceObject);

    Status = STATUS_SUCCESS;

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

    Status = STATUS_SUCCESS;

    return Status;
}
