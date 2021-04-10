/*
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>

/* Use GUID names 'gEfi...' that are already declared in Protocol headers. */
EFI_GUID gEfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID gEfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
EFI_GUID gEfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

/* Keep these variables global. */
static EFI_HANDLE ImageHandle;
static EFI_SYSTEM_TABLE *SystemTable;
static EFI_BOOT_SERVICES *BootServices;

typedef unsigned long long uint64_t;

struct boot_info_t {
    unsigned int* framebuffer;
    uint64_t* user_buffer;
    uint64_t kernel_code_size;
    uint64_t user_code_size;
    uint64_t* tss_buffer;
    uint64_t* page_table_buffer;
    void * memory_map;
    uint64_t memory_map_size;
    uint64_t memory_map_desc_size;
};

// Currently uses only EfiBootServicesData
// The function can be extended to accept any type as necessary
static VOID *AllocatePool(UINTN size, EFI_MEMORY_TYPE memType)
{
    VOID *ptr;
    EFI_STATUS ret = BootServices->AllocatePool(memType, size, &ptr);

    if (EFI_ERROR(ret)){
        // Print debug message, then stall in case
        if(ret == EFI_OUT_OF_RESOURCES)
            SystemTable->ConOut->OutputString(SystemTable->ConOut,
                L"ERROR: AllocatePool(): Out of Resources! \r\n");
        else if(ret == EFI_INVALID_PARAMETER)
            SystemTable->ConOut->OutputString(SystemTable->ConOut,
                L"ERROR: AllocatePool(): Invalid Parameter! \r\n");
        else if(ret == EFI_NOT_FOUND)
            SystemTable->ConOut->OutputString(SystemTable->ConOut,
                L"Error: AllocatePool(): Not Found!\r\n");
        BootServices->Stall(3 * 1000000); // stall so the message can be read
        return NULL;
    }
    return ptr;
}

static VOID FreePool(VOID *buf)
{
    BootServices->FreePool(buf);
}

/*
* Open user and kernel stuff
*/
static EFI_STATUS OpenKernel(EFI_FILE_PROTOCOL **pvh, EFI_FILE_PROTOCOL **pfh, EFI_FILE_PROTOCOL **puh)
{
    EFI_LOADED_IMAGE *li = NULL;
    EFI_FILE_IO_INTERFACE *fio = NULL;
    EFI_FILE_PROTOCOL *vh;
    EFI_STATUS efi_status;

    *pvh = NULL;
    *pfh = NULL;
    *puh = NULL;

    efi_status = BootServices->HandleProtocol(ImageHandle,
                    &gEfiLoadedImageProtocolGuid, (void **) &li);
    if (EFI_ERROR(efi_status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
            L"Cannot get LoadedImage for BOOTx64.EFI\r\n");
        BootServices->Stall(3 * 1000000); // 5 seconds
        return efi_status;
    }

    efi_status = BootServices->HandleProtocol(li->DeviceHandle,
                    &gEfiSimpleFileSystemProtocolGuid, (void **) &fio);
    if (EFI_ERROR(efi_status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
                        L"Cannot get fio\r\n");

        BootServices->Stall(3 * 1000000); // 5 seconds
        return efi_status;
    }

    efi_status = fio->OpenVolume(fio, pvh);
    if (EFI_ERROR(efi_status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
                        L"Cannot get the volume handle!\r\n");
        BootServices->Stall(3 * 1000000); // 5 seconds
        return efi_status;
    }
    vh = *pvh;

    efi_status = vh->Open(vh, pfh, L"\\EFI\\BOOT\\KERNEL",
                    EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(efi_status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
                        L"Cannot get the kernel file handle!\r\n");
        BootServices->Stall(3 * 1000000); // 5 seconds
        return efi_status;
    }

    efi_status = vh->Open(vh, puh, L"\\EFI\\BOOT\\USER",
                    EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(efi_status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
                        L"Cannot get the boot file handle!\r\n");
        BootServices->Stall(3 * 1000000); // 5 seconds
        return efi_status;
    }

    return EFI_SUCCESS;
}

static void CloseKernel(EFI_FILE_PROTOCOL *vh, EFI_FILE_PROTOCOL *fh,
    EFI_FILE_PROTOCOL *uh)
{
    vh->Close(vh);
    fh->Close(fh);
    uh->Close(uh);
}

static UINT32 *SetGraphicsMode(UINT32 width, UINT32 height)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics;
    EFI_STATUS efi_status;
    UINT32 mode;

    efi_status = BootServices->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,
                    NULL, (VOID **) &graphics);
    if (EFI_ERROR(efi_status)) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
            L"Cannot get the GOP handle!\r\n");
        BootServices->Stall(3 * 1000000); // 5 seconds
        return NULL;
    }

    if (!graphics->Mode || graphics->Mode->MaxMode == 0) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
            L"Incorrect GOP mode information!\r\n");
        BootServices->Stall(3 * 1000000); // 5 seconds
        return NULL;
    }

    for (mode = 0; mode < graphics->Mode->MaxMode; mode++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN size;

        // TODO: Locate BlueGreenRedReserved, aka BGRA (8-bit per color)
        // Resolution width x height (800x600 in our code)
        graphics->QueryMode(graphics, mode, &size, &info);

        if(info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor
            && info->HorizontalResolution == width
            && info->VerticalResolution == height){
            graphics->SetMode(graphics, mode);
            return (UINT32 *) graphics->Mode->FrameBufferBase;
        }
    }
    return NULL;
}

/* Use System V ABI rather than EFI/Microsoft ABI. */
typedef void (*kernel_entry_t) (unsigned long long *, struct boot_info_t*) __attribute__((sysv_abi));

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE *systemTable)
{
    EFI_FILE_PROTOCOL *vh, *fh, *uh;
    EFI_STATUS efi_status;
    UINT32 *fb;

    ImageHandle = imageHandle;
    SystemTable = systemTable;
    BootServices = systemTable->BootServices;

    efi_status = OpenKernel(&vh, &fh, &uh);
    if (EFI_ERROR(efi_status)) {
        BootServices->Stall(3 * 1000000); // 5 seconds
        return efi_status;
    }

    UINTN size = 4096 *  256;
    UINTN kernel_size = 4096 *  256;
    EFI_PHYSICAL_ADDRESS kernel_buffer = 0; // kernel program code and stack
    EFI_PHYSICAL_ADDRESS user_buffer = 0; //user program code and stack
    EFI_PHYSICAL_ADDRESS tss_buffer = 0; // tss segment and stack

    //allocate pages (1MB + 1 page for stack)
    efi_status = BootServices->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 257, &user_buffer);
    efi_status = BootServices->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 257, &kernel_buffer);
    efi_status = BootServices->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 3, &tss_buffer);

    if (EFI_ERROR(efi_status)) { //Check for errors
        if(efi_status == EFI_OUT_OF_RESOURCES)
            SystemTable->ConOut->OutputString(SystemTable->ConOut,
                L"Error: AllocatePages(): Out of Resources! \r\n");
        else if(efi_status == EFI_INVALID_PARAMETER)
            SystemTable->ConOut->OutputString(SystemTable->ConOut,
                L"Error: AllocatePages(): Invalid Parameter! \r\n");
        else if(efi_status == EFI_NOT_FOUND)
            SystemTable->ConOut->OutputString(SystemTable->ConOut,
                L"Error: AllocatePages(): Not Found! \r\n");
        BootServices->Stall(3 * 1000000); // stall so I can read the errors
        return efi_status;
    }

    // pointers to hold buffers
    // they are moved up a page since the first apge will be used for stacks
    VOID* kernel_code_baseptr = (void*) (kernel_buffer + 4096);
    VOID* user_code_baseptr = (void*) (user_buffer + 4096);
    VOID* tss_baseptr = (void*) (tss_buffer + 4096);

    //check pointers
    if(!kernel_code_baseptr || !user_code_baseptr) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Error: efi_main(): kernel buffer allocation failed! \r\n");
        BootServices->Stall(3 * 1000000); // 5 seconds
    }

    //read programs
    efi_status = fh->Read(fh,&kernel_size, kernel_code_baseptr);
     if(EFI_ERROR(efi_status)){
            if(efi_status == EFI_BUFFER_TOO_SMALL) {
                    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Error: GetMemoryMap(): Failed to retrieve map again! \r\n");
                    BootServices->Stall(3 * 1000000); // stall so I can read the errors
                    return efi_status;
                }
        }




    size = 4096 *  256;
    efi_status = uh->Read(uh,&size, user_code_baseptr);
    if(EFI_ERROR(efi_status)){
        if(efi_status == EFI_BUFFER_TOO_SMALL) {
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Error: GetMemoryMap(): Failed to retrieve map again! \r\n");
                BootServices->Stall(3 * 1000000); // stall so I can read the errors
                return efi_status;
            }
    }

    CloseKernel(vh, fh, uh);

    fb = SetGraphicsMode(800, 600);

    // // Allocate space for the page table.
    // EFI_PHYSICAL_ADDRESS base_physical_addr = 0;
    // efi_status = BootServices->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 2064, &base_physical_addr);

    // if (EFI_ERROR(efi_status)) { //Check for errors
    //     if(efi_status == EFI_OUT_OF_RESOURCES)
    //         SystemTable->ConOut->OutputString(SystemTable->ConOut,
    //             L"Error: AllocatePages(): Out of Resources! \r\n");
    //     else if(efi_status == EFI_INVALID_PARAMETER)
    //         SystemTable->ConOut->OutputString(SystemTable->ConOut,
    //             L"Error: AllocatePages(): Invalid Parameter! \r\n");
    //     else if(efi_status == EFI_NOT_FOUND)
    //         SystemTable->ConOut->OutputString(SystemTable->ConOut,
    //             L"Error: AllocatePages(): Not Found! \r\n");
    //     BootServices->Stall(3 * 1000000); // stall so I can read the errors
    //     return efi_status;
    // }

    // Cast pointer to allocated space
    // uint64_t *base = (unsigned long long *) base_physical_addr;
    uint64_t *page_table_baseptr = NULL;

    UINTN memMapSize = 0;
    EFI_MEMORY_DESCRIPTOR* memoryMap = NULL;
    UINTN mapKey = 0;
    UINTN descriptorSize;
    UINT32 descriptorVersion;
    //fill boot info with what we can
    struct boot_info_t boot_info = {(unsigned int*)fb, (uint64_t*)user_code_baseptr, (uint64_t) kernel_size, (uint64_t)size, (uint64_t*)tss_baseptr, (uint64_t*)page_table_baseptr};



    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Loading Clear! Exiting Boot Loader!! \r\n");

    while (1) {
        memMapSize = 0;

        // free memory used by BootServices
        efi_status = BootServices->GetMemoryMap(&memMapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);

        // check for errors, and redo getMemoryMap if possible
        if(EFI_ERROR(efi_status)){
            if(efi_status == EFI_BUFFER_TOO_SMALL) {
                memoryMap = AllocatePool(memMapSize, EfiBootServicesData);
                efi_status = BootServices->GetMemoryMap(&memMapSize, memoryMap, &mapKey, &descriptorSize, &descriptorVersion);

                if(EFI_ERROR(efi_status)){
                    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Error: GetMemoryMap(): Failed to retrieve map again! \r\n");
                    BootServices->Stall(3 * 1000000); // stall so I can read the errors
                    return efi_status;
                }
            }
            else {
                // Real error! try to print an error message and bail out
                SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Error: GetMemoryMap(): Error getting memory map! \r\n");
                BootServices->Stall(3 * 1000000); // stall so I can read the errors
                //return efi_status;
            }

        }

        boot_info.memory_map = memoryMap;
        boot_info.memory_map_size = memMapSize;
        boot_info.memory_map_desc_size = descriptorSize;

        efi_status = BootServices->ExitBootServices(ImageHandle, mapKey);
        if (efi_status == EFI_SUCCESS) {
            break; // Success, we can jump to the kernel now!
        }
        if (efi_status != EFI_INVALID_PARAMETER) {
            SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Error: ExitBootServices(): Unknown error! \r\n");
            BootServices->Stall(3 * 1000000); // stall so I can read the errors
            return efi_status;
        }
        // efi_status = EFI_INVALID_PARAMETER, need to repeat the loop again
        // if you need to deallocate any buffers, do not forget to do it here
        FreePool(memoryMap);
    }



    // kernel's _start() is at base #0 (pure binary format)
    // cast the function pointer appropriately and call the function
    kernel_entry_t func = (kernel_entry_t) kernel_code_baseptr;
    func(kernel_code_baseptr, &boot_info);

    return EFI_SUCCESS;
}
