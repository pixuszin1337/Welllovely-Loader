#include "mapper.h"
#include "pe.h"
#include <cstring>

struct ManualMapData {
    BYTE* pImageBase;
    HMODULE(WINAPI* fnLoadLibraryA)(LPCSTR);
    FARPROC(WINAPI* fnGetProcAddress)(HMODULE, LPCSTR);
    BOOL(WINAPI* fnRtlAddFunctionTable)(PRUNTIME_FUNCTION, DWORD, DWORD64);
    BOOL success;
};

#pragma runtime_checks("", off)
#pragma optimize("", off)

static DWORD WINAPI shellcode_entry(ManualMapData* pData) {
    if (!pData || !pData->pImageBase)
        return 0;

    BYTE* pBase = pData->pImageBase;
    auto* pDos = reinterpret_cast<IMAGE_DOS_HEADER*>(pBase);
    auto* pNt = reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + pDos->e_lfanew);
    auto* pOpt = &pNt->OptionalHeader;

    auto& importDir = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.Size) {
        auto* pImport = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pBase + importDir.VirtualAddress);
        while (pImport->Name) {
            char* szMod = reinterpret_cast<char*>(pBase + pImport->Name);
            HMODULE hMod = pData->fnLoadLibraryA(szMod);
            if (!hMod) {
                ++pImport;
                continue;
            }

            auto* pThunkRef = reinterpret_cast<ULONG_PTR*>(
                pBase + (pImport->OriginalFirstThunk ? pImport->OriginalFirstThunk : pImport->FirstThunk));
            auto* pFuncRef = reinterpret_cast<ULONG_PTR*>(pBase + pImport->FirstThunk);

            for (; *pThunkRef; ++pThunkRef, ++pFuncRef) {
                if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef)) {
                    *pFuncRef = reinterpret_cast<ULONG_PTR>(
                        pData->fnGetProcAddress(hMod, reinterpret_cast<LPCSTR>(*pThunkRef & 0xFFFF)));
                } else {
                    auto* pImportByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + (*pThunkRef));
                    *pFuncRef = reinterpret_cast<ULONG_PTR>(
                        pData->fnGetProcAddress(hMod, pImportByName->Name));
                }
            }
            ++pImport;
        }
    }

    auto& tlsDir = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (tlsDir.Size) {
        auto* pTLS = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(pBase + tlsDir.VirtualAddress);
        auto* pCallback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTLS->AddressOfCallBacks);
        if (pCallback) {
            while (*pCallback) {
                (*pCallback)(reinterpret_cast<PVOID>(pBase), DLL_PROCESS_ATTACH, nullptr);
                ++pCallback;
            }
        }
    }

    auto& exceptDir = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (exceptDir.Size && pData->fnRtlAddFunctionTable) {
        pData->fnRtlAddFunctionTable(
            reinterpret_cast<PRUNTIME_FUNCTION>(pBase + exceptDir.VirtualAddress),
            exceptDir.Size / sizeof(RUNTIME_FUNCTION),
            reinterpret_cast<DWORD64>(pBase));
    }

    if (pOpt->AddressOfEntryPoint) {
        auto pEntry = reinterpret_cast<BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID)>(
            pBase + pOpt->AddressOfEntryPoint);
        pEntry(reinterpret_cast<HINSTANCE>(pBase), DLL_PROCESS_ATTACH, nullptr);
    }

    pData->success = TRUE;
    return 0;
}

static void shellcode_end() {}

#pragma runtime_checks("", restore)
#pragma optimize("", on)

const char* mapper::to_string(Result r) {
    switch (r) {
    case Result::Success:         return "Success";
    case Result::InvalidPE:       return "Invalid PE file";
    case Result::AllocFailed:     return "Memory allocation failed";
    case Result::WriteFailed:     return "WriteProcessMemory failed";
    case Result::ShellcodeFailed: return "Shellcode execution failed";
    case Result::ThreadFailed:    return "Remote thread creation failed";
    case Result::Timeout:         return "Timed out waiting for entry point";
    }
    return "Unknown error";
}

mapper::Result mapper::manual_map(HANDLE hProcess, const std::vector<uint8_t>& dll_data) {
    const uint8_t* raw = dll_data.data();
    size_t raw_size = dll_data.size();

    if (!pe::is_valid(raw, raw_size))
        return Result::InvalidPE;

    auto* pNt = pe::nt_headers(const_cast<uint8_t*>(raw));
    auto* pOpt = &pNt->OptionalHeader;
    auto* pSections = IMAGE_FIRST_SECTION(pNt);
    DWORD numSections = pNt->FileHeader.NumberOfSections;

    LPVOID pTargetBase = VirtualAllocEx(
        hProcess, nullptr, pOpt->SizeOfImage,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!pTargetBase)
        return Result::AllocFailed;

    std::vector<uint8_t> image(pOpt->SizeOfImage, 0);
    memcpy(image.data(), raw, pOpt->SizeOfHeaders);

    for (DWORD i = 0; i < numSections; i++) {
        auto& sec = pSections[i];
        if (sec.SizeOfRawData == 0) continue;
        memcpy(
            image.data() + sec.VirtualAddress,
            raw + sec.PointerToRawData,
            sec.SizeOfRawData);
    }

    auto& relocDir = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (relocDir.Size) {
        ULONGLONG delta = reinterpret_cast<ULONGLONG>(pTargetBase) - pOpt->ImageBase;
        auto* pReloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(image.data() + relocDir.VirtualAddress);
        const auto* pRelocEnd = reinterpret_cast<const uint8_t*>(pReloc) + relocDir.Size;

        while (reinterpret_cast<const uint8_t*>(pReloc) < pRelocEnd && pReloc->SizeOfBlock) {
            DWORD numEntries = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            auto* pEntries = reinterpret_cast<WORD*>(
                reinterpret_cast<uint8_t*>(pReloc) + sizeof(IMAGE_BASE_RELOCATION));

            for (DWORD i = 0; i < numEntries; i++) {
                WORD type = pEntries[i] >> 12;
                WORD offset = pEntries[i] & 0x0FFF;
                uint8_t* pPatch = image.data() + pReloc->VirtualAddress + offset;

                switch (type) {
                case IMAGE_REL_BASED_DIR64:
                    *reinterpret_cast<ULONGLONG*>(pPatch) += delta;
                    break;
                case IMAGE_REL_BASED_HIGHLOW:
                    *reinterpret_cast<DWORD*>(pPatch) += static_cast<DWORD>(delta);
                    break;
                case IMAGE_REL_BASED_HIGH:
                    *reinterpret_cast<WORD*>(pPatch) += static_cast<WORD>(delta >> 16);
                    break;
                case IMAGE_REL_BASED_LOW:
                    *reinterpret_cast<WORD*>(pPatch) += static_cast<WORD>(delta);
                    break;
                case IMAGE_REL_BASED_ABSOLUTE:
                    break;
                }
            }

            pReloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                reinterpret_cast<uint8_t*>(pReloc) + pReloc->SizeOfBlock);
        }
    }

    if (!WriteProcessMemory(hProcess, pTargetBase, image.data(), pOpt->SizeOfImage, nullptr)) {
        VirtualFreeEx(hProcess, pTargetBase, 0, MEM_RELEASE);
        return Result::WriteFailed;
    }

    size_t shellcode_size = reinterpret_cast<BYTE*>(shellcode_end) - reinterpret_cast<BYTE*>(shellcode_entry);
    if (shellcode_size == 0 || shellcode_size > 0x1000)
        shellcode_size = 0x1000;

    size_t alloc_size = sizeof(ManualMapData) + shellcode_size;

    LPVOID pShellcodeAlloc = VirtualAllocEx(
        hProcess, nullptr, alloc_size,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    if (!pShellcodeAlloc) {
        VirtualFreeEx(hProcess, pTargetBase, 0, MEM_RELEASE);
        return Result::AllocFailed;
    }

    BYTE* pDataAddr = reinterpret_cast<BYTE*>(pShellcodeAlloc);
    BYTE* pCodeAddr = pDataAddr + sizeof(ManualMapData);

    ManualMapData mapData{};
    mapData.pImageBase = reinterpret_cast<BYTE*>(pTargetBase);
    mapData.fnLoadLibraryA = LoadLibraryA;
    mapData.fnGetProcAddress = GetProcAddress;

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        mapData.fnRtlAddFunctionTable = reinterpret_cast<decltype(mapData.fnRtlAddFunctionTable)>(
            GetProcAddress(hNtdll, "RtlAddFunctionTable"));
    }
    mapData.success = FALSE;

    if (!WriteProcessMemory(hProcess, pDataAddr, &mapData, sizeof(mapData), nullptr)) {
        VirtualFreeEx(hProcess, pShellcodeAlloc, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, pTargetBase, 0, MEM_RELEASE);
        return Result::WriteFailed;
    }

    if (!WriteProcessMemory(hProcess, pCodeAddr, reinterpret_cast<LPVOID>(shellcode_entry), shellcode_size, nullptr)) {
        VirtualFreeEx(hProcess, pShellcodeAlloc, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, pTargetBase, 0, MEM_RELEASE);
        return Result::WriteFailed;
    }

    HANDLE hThread = CreateRemoteThread(
        hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pCodeAddr),
        pDataAddr, 0, nullptr);

    if (!hThread) {
        VirtualFreeEx(hProcess, pShellcodeAlloc, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, pTargetBase, 0, MEM_RELEASE);
        return Result::ThreadFailed;
    }

    DWORD waitResult = WaitForSingleObject(hThread, 15000);
    CloseHandle(hThread);

    if (waitResult == WAIT_TIMEOUT) {
        VirtualFreeEx(hProcess, pShellcodeAlloc, 0, MEM_RELEASE);
        return Result::Timeout;
    }

    ManualMapData resultData{};
    ReadProcessMemory(hProcess, pDataAddr, &resultData, sizeof(resultData), nullptr);

    VirtualFreeEx(hProcess, pShellcodeAlloc, 0, MEM_RELEASE);

    if (!resultData.success) {
        VirtualFreeEx(hProcess, pTargetBase, 0, MEM_RELEASE);
        return Result::ShellcodeFailed;
    }

    std::vector<uint8_t> zeroes(pOpt->SizeOfHeaders, 0);
    WriteProcessMemory(hProcess, pTargetBase, zeroes.data(), zeroes.size(), nullptr);

    return Result::Success;
}
