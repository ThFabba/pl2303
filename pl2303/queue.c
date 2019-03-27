/*
 * PL2303 Driver request queue routines
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

#include "pl2303.h"

_Function_class_(IO_CSQ_INSERT_IRP_EX)
_IRQL_requires_(DISPATCH_LEVEL)
_Requires_lock_held_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static NTSTATUS NTAPI Pl2303QueueInsertIrp(_In_ PIO_CSQ Csq,
                                           _In_ PIRP Irp,
                                           _In_ PVOID InsertContext);
_Function_class_(IO_CSQ_REMOVE_IRP)
_IRQL_requires_(DISPATCH_LEVEL)
_Requires_lock_held_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static VOID NTAPI Pl2303QueueRemoveIrp(_In_ PIO_CSQ Csq,
                                       _In_ PIRP Irp);
_Function_class_(IO_CSQ_PEEK_NEXT_IRP)
_IRQL_requires_(DISPATCH_LEVEL)
_Requires_lock_held_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static PIRP NTAPI Pl2303QueuePeekNextIrp(_In_ PIO_CSQ Csq,
                                         _In_opt_ PIRP Irp,
                                         _In_opt_ PVOID PeekContext);
_Function_class_(IO_CSQ_ACQUIRE_LOCK )
_IRQL_raises_(DISPATCH_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_Acquires_lock_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static VOID NTAPI Pl2303QueueAcquireLock(_In_ PIO_CSQ Csq,
                                         _Out_ _At_(*OldIrql, _Post_ _IRQL_saves_) PKIRQL OldIrql);
_Function_class_(IO_CSQ_RELEASE_LOCK)
_IRQL_requires_(DISPATCH_LEVEL)
_Releases_lock_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static VOID NTAPI Pl2303QueueReleaseLock(_In_ PIO_CSQ Csq,
                                         _In_ _IRQL_restores_ KIRQL OldIrql);
_Function_class_(IO_CSQ_COMPLETE_CANCELED_IRP)
_IRQL_requires_max_(DISPATCH_LEVEL)
static VOID NTAPI Pl2303QueueCompleteCanceledIrp(_In_ PIO_CSQ Csq,
                                                 _In_ PIRP Irp);

NTSTATUS Pl2303InitializeQueue(_In_ PQUEUE Queue);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, Pl2303InitializeQueue)
#endif /* defined ALLOC_PRAGMA */

NTSTATUS
Pl2303InitializeQueue(
    _In_ PQUEUE Queue)
{
    NTSTATUS Status;

    PAGED_CODE();

    Status = IoCsqInitializeEx(&Queue->Csq,
                               Pl2303QueueInsertIrp,
                               Pl2303QueueRemoveIrp,
                               Pl2303QueuePeekNextIrp,
                               Pl2303QueueAcquireLock,
                               Pl2303QueueReleaseLock,
                               Pl2303QueueCompleteCanceledIrp);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    KeInitializeSpinLock(&Queue->QueueSpinLock);
    InitializeListHead(&Queue->QueueHead);
    return STATUS_SUCCESS;
}

NTSTATUS
Pl2303QueueIrp(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    DBG_UNREFERENCED_PARAMETER(DeviceObject);
    DBG_UNREFERENCED_PARAMETER(Irp);
    return STATUS_NOT_IMPLEMENTED;
}

_Function_class_(IO_CSQ_INSERT_IRP_EX)
_IRQL_requires_(DISPATCH_LEVEL)
_Requires_lock_held_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static
NTSTATUS
NTAPI
Pl2303QueueInsertIrp(
    _In_ PIO_CSQ Csq,
    _In_ PIRP Irp,
    _In_ PVOID InsertContext)
{
    PQUEUE Queue = CONTAINING_RECORD(Csq, QUEUE, Csq);

    UNREFERENCED_PARAMETER(InsertContext);

    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
    InsertTailList(&Queue->QueueHead,
                   &Irp->Tail.Overlay.ListEntry);

    return STATUS_SUCCESS;
}

_Function_class_(IO_CSQ_REMOVE_IRP)
_IRQL_requires_(DISPATCH_LEVEL)
_Requires_lock_held_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static
VOID
NTAPI
Pl2303QueueRemoveIrp(
    _In_ PIO_CSQ Csq,
    _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(Csq);

    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}

_Function_class_(IO_CSQ_PEEK_NEXT_IRP)
_IRQL_requires_(DISPATCH_LEVEL)
_Requires_lock_held_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static
PIRP
NTAPI
Pl2303QueuePeekNextIrp(
    _In_ PIO_CSQ Csq,
    _In_opt_ PIRP Irp,
    _In_opt_ PVOID PeekContext)
{
    PQUEUE Queue = CONTAINING_RECORD(Csq, QUEUE, Csq);
    PLIST_ENTRY ListEntry;
    PIRP ListIrp;

    DBG_UNREFERENCED_PARAMETER(PeekContext);

    ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);
    if (Irp)
        ListEntry = Irp->Tail.Overlay.ListEntry.Flink;
    else
        ListEntry = Queue->QueueHead.Flink;

    while (ListEntry != &Queue->QueueHead)
    {
        ListIrp = CONTAINING_RECORD(ListEntry, IRP, Tail.Overlay.ListEntry);
        ListEntry = ListEntry->Flink;
        return ListIrp;
    }

    return NULL;
}

_Function_class_(IO_CSQ_ACQUIRE_LOCK)
_IRQL_raises_(DISPATCH_LEVEL)
_IRQL_requires_max_(DISPATCH_LEVEL)
_Acquires_lock_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static
VOID
NTAPI
Pl2303QueueAcquireLock(
    _In_ PIO_CSQ Csq,
    _Out_ _At_(*OldIrql, _Post_ _IRQL_saves_) PKIRQL OldIrql)
{
    PQUEUE Queue = CONTAINING_RECORD(Csq, QUEUE, Csq);

    KeAcquireSpinLock(&Queue->QueueSpinLock, OldIrql);
}

_Function_class_(IO_CSQ_RELEASE_LOCK)
_IRQL_requires_(DISPATCH_LEVEL)
_Releases_lock_(CONTAINING_RECORD(Csq, QUEUE, Csq)->QueueSpinLock)
static
VOID
NTAPI
Pl2303QueueReleaseLock(
    _In_ PIO_CSQ Csq,
    _In_ _IRQL_restores_ KIRQL OldIrql)
{
    PQUEUE Queue = CONTAINING_RECORD(Csq, QUEUE, Csq);

    KeReleaseSpinLock(&Queue->QueueSpinLock, OldIrql);
}

_Function_class_(IO_CSQ_COMPLETE_CANCELED_IRP)
_IRQL_requires_max_(DISPATCH_LEVEL)
static
VOID
NTAPI
Pl2303QueueCompleteCanceledIrp(
    _In_ PIO_CSQ Csq,
    _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(Csq);

    ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}
