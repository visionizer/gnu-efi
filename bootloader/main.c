#include <efi.h>
#include <efilib.h>
#include <stdbool.h>
#include <elf.h>

#ifndef __x86_64__
#define ELF_NATIVE_ARCH EM_X86_64
#elif defined(__aarch64__)
#define ELF_NATIVE_ARCH EM_AARCH64
#else
#error "No valid architecture defined"
#endif


EFI_SYSTEM_TABLE* TABLE;
EFI_BOOT_SERVICES* BOOT_SERVICES;
EFI_HANDLE HANDLE;

#define UNUSED(x) (void)x

#define NOTICE L"NOTICE"
#define WARN L"WARN"
#define OK L"OK"
#define ERROR L"ERROR"
#define FATAL L"FATAL"

#define log(lvl, fmt, ...) Print(L"[ %s ] ::-> ", lvl); Print(L##fmt, ##__VA_ARGS__); \
	Print(L"\r\n"); 

// The following will trigger once
// The do { ... } while (false) is neccessary as placing
// a ; at the end of a check_status(..) would cause an error
#define check_status(efi, what, retval) \
	do { \
		if (efi != EFI_SUCCESS) { \
			 log(ERROR, "An UEFI Error occured while trying to %a", what); \
			 log(ERROR, "%r", efi); \
			 return retval; \
		} else { \
			 { log(OK, "Successfully managed to %a", what); } \
		} \
	} while (false)

int memcmp(const void* ptra, const void* ptrb, size_t n)
{
	const unsigned char *a = aptr, *b = bptr;
	for (size_t i = 0; i < n; i++) {
		if (a[i] > b[i]) return -1;
		else if (a[i] < b[i]) return 1;
	}
	return 0;
}


EFI_FILE* load_file(EFI_FILE* directory, CHAR16* path)
{
	EFI_FILE* file;
	UNUSED(file);
	EFI_LOADED_IMAGE_PROTOCOL* image;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fileSystem;
	EFI_FILE* dir = directory;
	EFI_STATUS status;

	status = BOOT_SERVICES->HandleProtocol(HANDLE, &gEfiLoadedImageProtocolGuid, (void**)&image);
	check_status(status, "handle the loaded image protocol", NULL);
	
	status = BOOT_SERVICES->HandleProtocol(image->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&fileSystem);
	check_status(status, "handle the simple file system protocol", NULL);
	
	if (NULL == dir) {
		status = fileSystem->OpenVolume(fileSystem, &dir);
		check_status(status, "open the root volume", NULL);
	}

	status = dir->Open(dir, &file, path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
	check_status(status, "open the kernel file", NULL);
	check_status(EFI_NOT_FOUND, "check whether this works", NULL);
	if (status != EFI_SUCCESS) {
		return NULL;
	}

	return file;
}


EFI_STATUS efi_main (EFI_HANDLE handle, EFI_SYSTEM_TABLE* table) 
{
	InitializeLib(handle, table);
	
	TABLE = table;
	BOOT_SERVICES = table->BootServices;
	HANDLE = handle;
	
	log(NOTICE, "Launching Stiefelloader...");

	// As of STDv1, the kernel must be
	// (following an old unix tradition)
	// mounted at ESP:/esque
	// where:
	// 	ESP -> Your boot partition in Fat32
	// The Kernel must also be offset by 2M in order
	// to avoid overriding memory that does not belong to us
	// The kernel must be a valid Elf64 file and the main
	// method must be using the sysv-abi
	EFI_FILE* kernel = load_file(NULL, L"esque");
	if (kernel == NULL) {
		log(FATAL, "Failed to load the kernel file at /esque.");
		return EFI_NOT_FOUND;
	}

	Elf64_Ehdr header;
	{
		EFI_STATUS status;

		UINTN size;
		EFI_FILE_INFO* info;
		status = kernel->GetInfo(kernel, &gEfiFileInfoGuid, &size, NULL);
		check_status(status, "get kernel info", EFI_NOT_FOUND);
		UINTN headerSize = sizeof(header);
		kernel->Read(kernel, &headerSize, &header);
	}

	// Verify the ELF File
	if (
		memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 		||
		header.e_ident[EI_CLASS] != ELFCLASS64				||
		header.e_ident[EI_DATA] != ELFDATA2LSB				||
		header.e_type != ET_EXEC					||
		header.e_machine != ELF_NATIVE_ARCH				||
	) {
	}

	return EFI_SUCCESS;
}
