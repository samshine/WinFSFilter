/*++

Module Name:

    WinFSFilter.c

Abstract:

    This is the main module of the WinFSFilter miniFilter driver.

Environment:

    Kernel mode

--*/

#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

#include"MyHeader.h"

// TODO
// 还没有正确配置好动态链接库的项目导入设置

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")


PFLT_FILTER gFilterHandle;
ULONG_PTR OperationStatusCtx = 1;

#define PTDBG_TRACE_ROUTINES            0x00000001
#define PTDBG_TRACE_OPERATION_STATUS    0x00000002

ULONG gTraceFlags = 0;


#define PT_DBG_PRINT( _dbgLevel, _string )          \
    (FlagOn(gTraceFlags,(_dbgLevel)) ?              \
        DbgPrint _string :                          \
        ((int)0))

/*************************************************************************
    Prototypes
*************************************************************************/

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    );

NTSTATUS
WinFSFilterInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

VOID
WinFSFilterInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
WinFSFilterInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

NTSTATUS
WinFSFilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    );

NTSTATUS
WinFSFilterInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
WinFSFilterPreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

VOID
WinFSFilterOperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    );

FLT_POSTOP_CALLBACK_STATUS
WinFSFilterPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
WinFSFilterPreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    );

BOOLEAN
WinFSFilterDoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    );

EXTERN_C_END

//
// My Func Prototypes 
//
FLT_PREOP_FUNC(PreMyCreate);
FLT_POSTOP_FUNC(PostMyCreate);
FLT_PREOP_FUNC(PreMyWrite);
FLT_POSTOP_FUNC(PostMyWrite);
FLT_PREOP_FUNC(PreMyRead);
FLT_POSTOP_FUNC(PostMyRead);
FLT_PREOP_FUNC(PreMyDelete);
FLT_POSTOP_FUNC(PostMyDelete);

void LoadConfig();
void LoadTarget();


/*************************************************************************
	Prototypes End
*************************************************************************/

//
//  Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, WinFSFilterUnload)
#pragma alloc_text(PAGE, WinFSFilterInstanceQueryTeardown)
#pragma alloc_text(PAGE, WinFSFilterInstanceSetup)
#pragma alloc_text(PAGE, WinFSFilterInstanceTeardownStart)
#pragma alloc_text(PAGE, WinFSFilterInstanceTeardownComplete)
#endif

//
//  operation registration
//

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {

    { IRP_MJ_CREATE,
      0,
      PreMyCreate,
      PostMyCreate },

    { IRP_MJ_CREATE_NAMED_PIPE,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_CLOSE,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_READ,
      0,
      PreMyRead,
      PostMyRead },

    { IRP_MJ_WRITE,
      0,
	  PreMyWrite,
	  PostMyWrite },

	{ IRP_MJ_SET_INFORMATION,
	  0,
	  PreMyDelete,
	  PostMyDelete },

#if 0 // TODO - List all of the requests to filter.
    { IRP_MJ_QUERY_INFORMATION,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_QUERY_EA,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_SET_EA,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_FLUSH_BUFFERS,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_QUERY_VOLUME_INFORMATION,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_SET_VOLUME_INFORMATION,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_DIRECTORY_CONTROL,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_FILE_SYSTEM_CONTROL,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_DEVICE_CONTROL,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_INTERNAL_DEVICE_CONTROL,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_SHUTDOWN,
      0,
      WinFSFilterPreOperationNoPostOperation,
      NULL },                               //post operations not supported

    { IRP_MJ_LOCK_CONTROL,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_CLEANUP,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_CREATE_MAILSLOT,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_QUERY_SECURITY,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_SET_SECURITY,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_QUERY_QUOTA,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_SET_QUOTA,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_PNP,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_RELEASE_FOR_SECTION_SYNCHRONIZATION,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_ACQUIRE_FOR_MOD_WRITE,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_RELEASE_FOR_MOD_WRITE,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_ACQUIRE_FOR_CC_FLUSH,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_RELEASE_FOR_CC_FLUSH,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_FAST_IO_CHECK_IF_POSSIBLE,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_NETWORK_QUERY_OPEN,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_MDL_READ,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_MDL_READ_COMPLETE,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_PREPARE_MDL_WRITE,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_MDL_WRITE_COMPLETE,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_VOLUME_MOUNT,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

    { IRP_MJ_VOLUME_DISMOUNT,
      0,
      WinFSFilterPreOperation,
      WinFSFilterPostOperation },

#endif // TODO

    { IRP_MJ_OPERATION_END }
};

//
//  This defines what we want to filter with FltMgr
//

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof( FLT_REGISTRATION ),         //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    Callbacks,                          //  Operation callbacks

    WinFSFilterUnload,                           //  MiniFilterUnload

    WinFSFilterInstanceSetup,                    //  InstanceSetup
    WinFSFilterInstanceQueryTeardown,            //  InstanceQueryTeardown
    WinFSFilterInstanceTeardownStart,            //  InstanceTeardownStart
    WinFSFilterInstanceTeardownComplete,         //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};



NTSTATUS
WinFSFilterInstanceSetup (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
/*++

Routine Description:

    This routine is called whenever a new instance is created on a volume. This
    gives us a chance to decide if we need to attach to this volume or not.

    If this routine is not defined in the registration structure, automatic
    instances are always created.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Flags describing the reason for this attach request.

Return Value:

    STATUS_SUCCESS - attach
    STATUS_FLT_DO_NOT_ATTACH - do not attach

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterInstanceSetup: Entered\n") );

    return STATUS_SUCCESS;
}


NTSTATUS
WinFSFilterInstanceQueryTeardown (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This is called when an instance is being manually deleted by a
    call to FltDetachVolume or FilterDetach thereby giving us a
    chance to fail that detach request.

    If this routine is not defined in the registration structure, explicit
    detach requests via FltDetachVolume or FilterDetach will always be
    failed.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Indicating where this detach request came from.

Return Value:

    Returns the status of this operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterInstanceQueryTeardown: Entered\n") );

    return STATUS_SUCCESS;
}


VOID
WinFSFilterInstanceTeardownStart (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the start of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterInstanceTeardownStart: Entered\n") );
}


VOID
WinFSFilterInstanceTeardownComplete (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
/*++

Routine Description:

    This routine is called at the end of instance teardown.

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance and its associated volume.

    Flags - Reason why this instance is being deleted.

Return Value:

    None.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterInstanceTeardownComplete: Entered\n") );
}


/*************************************************************************
    MiniFilter initialization and unload routines.
*************************************************************************/

NTSTATUS
DriverEntry (
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    This is the initialization routine for this miniFilter driver.  This
    registers with FltMgr and initializes all global data structures.

Arguments:

    DriverObject - Pointer to driver object created by the system to
        represent this driver.

    RegistryPath - Unicode string identifying where the parameters for this
        driver are located in the registry.

Return Value:

    Routine can return non success error codes.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( RegistryPath );

	DPRINT(CUR_LEVEL, "My Force Print  Get WinFSFilter DriverEntry Entered\n");

	LoadTarget();
	LoadConfig();
	if (TARGET == NULL) {
		DPRINT(CUR_LEVEL,"Driver Didn't Start\n");
	}

	
	//WriteError = (ExternWarningFunc)MmGetProcedureAddress()
	/*MmGetProcedureAddress*/
	//NtRaiseHard
	//IoRaiseHardError()

    //
    //  Register with FltMgr to tell it our callback routines
    //

    status = FltRegisterFilter( DriverObject,
                                &FilterRegistration,
                                &gFilterHandle );

    FLT_ASSERT( NT_SUCCESS( status ) );

    if (NT_SUCCESS( status )) {

        //
        //  Start filtering i/o
        //

        status = FltStartFiltering( gFilterHandle );

        if (!NT_SUCCESS( status )) {

            FltUnregisterFilter( gFilterHandle );
        }
    }

    return status;
}

NTSTATUS
WinFSFilterUnload (
    _In_ FLT_FILTER_UNLOAD_FLAGS Flags
    )
/*++

Routine Description:

    This is the unload routine for this miniFilter driver. This is called
    when the minifilter is about to be unloaded. We can fail this unload
    request if this is not a mandatory unload indicated by the Flags
    parameter.

Arguments:

    Flags - Indicating if this is a mandatory unload.

Return Value:

    Returns STATUS_SUCCESS.

--*/
{
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterUnload: Entered\n") );

    FltUnregisterFilter( gFilterHandle );

    return STATUS_SUCCESS;
}


/*************************************************************************
    MiniFilter callback routines.
*************************************************************************/
FLT_PREOP_CALLBACK_STATUS
WinFSFilterPreOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterPreOperation: Entered\n") );

    //
    //  See if this is an operation we would like the operation status
    //  for.  If so request it.
    //
    //  NOTE: most filters do NOT need to do this.  You only need to make
    //        this call if, for example, you need to know if the oplock was
    //        actually granted.
    //

    if (WinFSFilterDoRequestOperationStatus( Data )) {

        status = FltRequestOperationStatusCallback( Data,
                                                    WinFSFilterOperationStatusCallback,
                                                    (PVOID)(++OperationStatusCtx) );
        if (!NT_SUCCESS(status)) {

            PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                          ("WinFSFilter!WinFSFilterPreOperation: FltRequestOperationStatusCallback Failed, status=%08x\n",
                           status) );
        }
    }

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_WITH_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}



VOID
WinFSFilterOperationStatusCallback (
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PFLT_IO_PARAMETER_BLOCK ParameterSnapshot,
    _In_ NTSTATUS OperationStatus,
    _In_ PVOID RequesterContext
    )
/*++

Routine Description:

    This routine is called when the given operation returns from the call
    to IoCallDriver.  This is useful for operations where STATUS_PENDING
    means the operation was successfully queued.  This is useful for OpLocks
    and directory change notification operations.

    This callback is called in the context of the originating thread and will
    never be called at DPC level.  The file object has been correctly
    referenced so that you can access it.  It will be automatically
    dereferenced upon return.

    This is non-pageable because it could be called on the paging path

Arguments:

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    RequesterContext - The context for the completion routine for this
        operation.

    OperationStatus -

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( FltObjects );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterOperationStatusCallback: Entered\n") );

    PT_DBG_PRINT( PTDBG_TRACE_OPERATION_STATUS,
                  ("WinFSFilter!WinFSFilterOperationStatusCallback: Status=%08x ctx=%p IrpMj=%02x.%02x \"%s\"\n",
                   OperationStatus,
                   RequesterContext,
                   ParameterSnapshot->MajorFunction,
                   ParameterSnapshot->MinorFunction,
                   FltGetIrpName(ParameterSnapshot->MajorFunction)) );
}


FLT_POSTOP_CALLBACK_STATUS
WinFSFilterPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
    )
/*++

Routine Description:

    This routine is the post-operation completion routine for this
    miniFilter.

    This is non-pageable because it may be called at DPC level.

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The completion context set in the pre-operation routine.

    Flags - Denotes whether the completion is successful or is being drained.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterPostOperation: Entered\n") );

    return FLT_POSTOP_FINISHED_PROCESSING;
}


FLT_PREOP_CALLBACK_STATUS
WinFSFilterPreOperationNoPostOperation (
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID *CompletionContext
    )
/*++

Routine Description:

    This routine is a pre-operation dispatch routine for this miniFilter.

    This is non-pageable because it could be called on the paging path

Arguments:

    Data - Pointer to the filter callbackData that is passed to us.

    FltObjects - Pointer to the FLT_RELATED_OBJECTS data structure containing
        opaque handles to this filter, instance, its associated volume and
        file object.

    CompletionContext - The context for the completion routine for this
        operation.

Return Value:

    The return value is the status of the operation.

--*/
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );

    PT_DBG_PRINT( PTDBG_TRACE_ROUTINES,
                  ("WinFSFilter!WinFSFilterPreOperationNoPostOperation: Entered\n") );

    // This template code does not do anything with the callbackData, but
    // rather returns FLT_PREOP_SUCCESS_NO_CALLBACK.
    // This passes the request down to the next miniFilter in the chain.

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}


BOOLEAN
WinFSFilterDoRequestOperationStatus(
    _In_ PFLT_CALLBACK_DATA Data
    )
/*++

Routine Description:

    This identifies those operations we want the operation status for.  These
    are typically operations that return STATUS_PENDING as a normal completion
    status.

Arguments:

Return Value:

    TRUE - If we want the operation status
    FALSE - If we don't

--*/
{
    PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;

    //
    //  return boolean state based on which operations we are interested in
    //

    return (BOOLEAN)

            //
            //  Check for oplock operations
            //

             (((iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
               ((iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_FILTER_OPLOCK)  ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_BATCH_OPLOCK)   ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_1) ||
                (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2)))

              ||

              //
              //    Check for directy change notification
              //

              ((iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL) &&
               (iopb->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY))
             );
}

// 
// My Functions
// 

// Open C:\WinFSFilter to get target path
void LoadTarget() {
	UNICODE_STRING     uniName;
	OBJECT_ATTRIBUTES  objAttr;

	RtlInitUnicodeString(&uniName, TARGET_CFG_FILE);
	InitializeObjectAttributes(&objAttr, &uniName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);

	HANDLE   handle;
	NTSTATUS ntstatus;
	IO_STATUS_BLOCK    ioStatusBlock;
	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		return;

	ntstatus = ZwCreateFile(&handle,
		GENERIC_READ,
		&objAttr, &ioStatusBlock, NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_WRITE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);
	if (!NT_SUCCESS(ntstatus)) {
		DPRINT(CUR_LEVEL,"Fail To Open Target Config File, %x\n", ntstatus);
	}
	else {
#define  BUFFER_SIZE 1024
		LARGE_INTEGER byteOffset;

		byteOffset.LowPart = byteOffset.HighPart = 0;
		ntstatus = ZwReadFile(handle, NULL, NULL, NULL, &ioStatusBlock,
			TARGET, BUFFER_SIZE, &byteOffset, NULL);
		if (NT_SUCCESS(ntstatus)) {
			TARGET[BUFFER_SIZE - 1] = '\0';
			DPRINT(CUR_LEVEL, "Get Target Config : %ls\n", TARGET);
		}
		ZwClose(handle);
		wchar_t *p = TARGET;
		while (*(p + 1))
			p++;
		if (*p == '\\') {
			IS_TARGET_FILE = 0;
			DPRINT(CUR_LEVEL,"Target is a directory\n");
		}
		else {
			DPRINT(CUR_LEVEL, "Target Is File\n");
		}
	}
}

// Open config file and get config at driver entry
void LoadConfig() {
	UNICODE_STRING     uniName;
	OBJECT_ATTRIBUTES  objAttr;

	RtlInitUnicodeString(&uniName, CONFIG_FILE);
	InitializeObjectAttributes(&objAttr, &uniName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL, NULL);

	HANDLE   handle;
	NTSTATUS ntstatus;
	IO_STATUS_BLOCK    ioStatusBlock;
	if (KeGetCurrentIrql() != PASSIVE_LEVEL)
		return;

	ntstatus = ZwCreateFile(&handle,
		GENERIC_READ,
		&objAttr, &ioStatusBlock, NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_WRITE,
		FILE_OPEN,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);
	if (!NT_SUCCESS(ntstatus)) {
		DPRINT(CUR_LEVEL, "Fail To Open Config,%x\n",ntstatus);
	}
	else {
#define  BUFFER_SIZE 1024
		CHAR buffer[BUFFER_SIZE] = { 0 };
		LARGE_INTEGER byteOffset;
		byteOffset.LowPart = byteOffset.HighPart = 0;
		ntstatus = ZwReadFile(handle, NULL, NULL, NULL, &ioStatusBlock,
			buffer, BUFFER_SIZE, &byteOffset, NULL);
		if (NT_SUCCESS(ntstatus)) {
			buffer[BUFFER_SIZE - 1] = '\0';
			DPRINT(CUR_LEVEL,"Get Config : %s\n", buffer);
		}
		ZwClose(handle);

		if (!strstr(buffer, "read_true"))
			READ_ACCESS = 0;
		if (!strstr(buffer, "write_true"))
			WRITE_ACCESS = 0;
		if (!strstr(buffer, "delete_true"))
			DELETE_ACCESS = 0;
	}
}

//
// My Callbacks 
//

// Intercept opening target files with writing, reading and direct deletion(del) rights 
// or opening config files with writing and direct deletion(del) rights 
FLT_PREOP_CALLBACK_STATUS
PreMyCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(FltObjects);
	PAGED_CODE();

	PFLT_FILE_NAME_INFORMATION n, o;
	PFLT_FILE_NAME_INFORMATION i;
	NTSTATUS status;
	int isTarget = 0, isConfig = 0,isTargetConfig = 0;

	status = FltGetFileNameInformation(Data, (FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT), &n);
	if (NT_SUCCESS(status)) {
		o = InterlockedExchangePointer(&i, n);
		if (NULL != i) {
			FltReleaseFileNameInformation(i);
		}

		if(IS_TARGET_FILE){
			if(!wcscmp(n->Name.Buffer, TARGET))
				isTarget = 1;
			if (!wcscmp(n->Name.Buffer, CONFIG_FILE))
				isConfig = 1;
			if (!wcscmp(n->Name.Buffer, TARGET_CFG_FILE))
				isTargetConfig = 1;
		}
		else {
			if (wcsstr(n->Name.Buffer, TARGET))
				isTarget = 1;
			if (wcsstr(n->Name.Buffer, CONFIG_FILE))
				isConfig = 1;
			if (wcsstr(n->Name.Buffer, TARGET_CFG_FILE))
				isTargetConfig = 1;
		}
	}

	// If is config file or target config file
	if (isConfig || isTargetConfig) {
		if ((Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA) ||
			(Data->Iopb->Parameters.Create.Options & FILE_DELETE_ON_CLOSE)) {
			DPRINT(CUR_LEVEL, "Abandon Config File Create%s%s IO \n",
				Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA ? " Write" : "",
				Data->Iopb->Parameters.Create.Options & FILE_DELETE_ON_CLOSE ? " Delete" : "");
			if (Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA)
				WriteError();
			else
				DeleteError();
			return FLT_PREOP_COMPLETE;
		}
	}
	// Or is target file
	else if (isTarget) {
		if ((!READ_ACCESS && Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_READ_DATA) ||
			(!WRITE_ACCESS && Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA) ||
			(Data->Iopb->Parameters.Create.Options & FILE_DELETE_ON_CLOSE)) {
			DPRINT(CUR_LEVEL, "Abandon A File Create%s%s%s IO ,%wZ\n",
				Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_READ_DATA ? " Read" : "",
				Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA ? " Write" : "",
				Data->Iopb->Parameters.Create.Options & FILE_DELETE_ON_CLOSE ? " Delete" : "",
				n->Name);
			if (Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA)
				WriteError();
			else if (Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess & FILE_WRITE_DATA)
				ReadError();
			else
				DeleteError();
			return FLT_PREOP_COMPLETE;
		}
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
PostMyCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

// Intercept writing to target files
FLT_PREOP_CALLBACK_STATUS
PreMyWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(FltObjects);
	PAGED_CODE();

	PFLT_FILE_NAME_INFORMATION n, o;
	PFLT_FILE_NAME_INFORMATION i;
	NTSTATUS status;
	int isTarget = 0, isConfig = 0, isTargetConfig = 0;

	status = FltGetFileNameInformation(Data, (FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT), &n);
	if (NT_SUCCESS(status)) {
		o = InterlockedExchangePointer(&i, n);
		if (NULL != i) {
			FltReleaseFileNameInformation(i);
		}

		if (IS_TARGET_FILE) {
			if (!wcscmp(n->Name.Buffer, TARGET))
				isTarget = 1;
			if (!wcscmp(n->Name.Buffer, CONFIG_FILE))
				isConfig = 1;
			if (!wcscmp(n->Name.Buffer, TARGET_CFG_FILE))
				isTargetConfig = 1;
		}
		else {
			if (wcsstr(n->Name.Buffer, TARGET))
				isTarget = 1;
			if (wcsstr(n->Name.Buffer, CONFIG_FILE))
				isConfig = 1;
			if (wcsstr(n->Name.Buffer, TARGET_CFG_FILE))
				isTargetConfig = 1;
		}
	}

	if (isTarget && !WRITE_ACCESS) {
		// Disallow Target File IO
		DPRINT(CUR_LEVEL, "Abandon A File Write IO ,%wZ\n", n->Name);
		WriteError();
		return FLT_PREOP_COMPLETE;
	}
	else if (isTargetConfig || isConfig) {
		// Disallow Config File IO
		DPRINT(CUR_LEVEL, "Abandon A Config File Write IO\n");
		WriteError();
		return FLT_PREOP_COMPLETE;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
PostMyWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

// Intercept reading from target files
FLT_PREOP_CALLBACK_STATUS
PreMyRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(FltObjects);
	PAGED_CODE();

	PFLT_FILE_NAME_INFORMATION n, o;
	PFLT_FILE_NAME_INFORMATION i;
	NTSTATUS status;
	int isTarget = 0;
	status = FltGetFileNameInformation(Data, (FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT), &n);
	if (NT_SUCCESS(status)) {
		o = InterlockedExchangePointer(&i, n);
		if (NULL != i) {
			FltReleaseFileNameInformation(i);
		}

		if (IS_TARGET_FILE) {
			if (!wcscmp(n->Name.Buffer, TARGET))
				isTarget = 1;
		}
		else {
			if (wcsstr(n->Name.Buffer, TARGET))
				isTarget = 1;
		}
	}

	if (!READ_ACCESS && isTarget) {
		// Disallow
		DPRINT(CUR_LEVEL, "Abandon A File Read IO ,%wZ\n", n->Name);
		ReadError();
		return FLT_PREOP_COMPLETE;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
PostMyRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	return FLT_POSTOP_FINISHED_PROCESSING;

}

// Intercept moving operation about target files,including moving to recycled
FLT_PREOP_CALLBACK_STATUS
PreMyDelete(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(FltObjects);
	PAGED_CODE();

	PFLT_FILE_NAME_INFORMATION n, o;
	PFLT_FILE_NAME_INFORMATION i;
	NTSTATUS status;
	int isTarget = 0, isConfig = 0, isTargetConfig = 0;

	status = FltGetFileNameInformation(Data, (FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT), &n);
	if (NT_SUCCESS(status)) {
		o = InterlockedExchangePointer(&i, n);
		if (NULL != i) {
			FltReleaseFileNameInformation(i);
		}

		if (IS_TARGET_FILE) {
			if (!wcscmp(n->Name.Buffer, TARGET))
				isTarget = 1;
			if (!wcscmp(n->Name.Buffer, CONFIG_FILE))
				isConfig = 1;
			if (!wcscmp(n->Name.Buffer, TARGET_CFG_FILE))
				isTargetConfig = 1;
		}
		else {
			if (wcsstr(n->Name.Buffer, TARGET))
				isTarget = 1;
			if (wcsstr(n->Name.Buffer, CONFIG_FILE))
				isConfig = 1;
			if (wcsstr(n->Name.Buffer, TARGET_CFG_FILE))
				isTargetConfig = 1;
		}
	}

	if (isTarget && !DELETE_ACCESS) {
		// Disallow Target File IO
		DPRINT(CUR_LEVEL, "Abandon A File Deletion (Recycle) IO ,%wZ\n", n->Name);
		DeleteError();
		return FLT_PREOP_COMPLETE;
	}
	else if (isConfig || isTargetConfig) {
		// Disallow Config File IO
		DPRINT(CUR_LEVEL, "Abandon A Config File Deletion (Recycle) IO\n");
		DeleteError();
		return FLT_PREOP_COMPLETE;
	}

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS
PostMyDelete(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
) {
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(FltObjects);
	UNREFERENCED_PARAMETER(CompletionContext);
	UNREFERENCED_PARAMETER(Flags);

	return FLT_POSTOP_FINISHED_PROCESSING;
}
