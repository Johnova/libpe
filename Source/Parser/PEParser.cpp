#include "stdafx.h"
#include "Parser/PEParser.h"
#include "Parser/PEParserImpl.h"
#include "PE/PEFile.h"
#include "PE/PEHeader.h"
#include "PE/PESection.h"
#include "PE/PEExportTable.h"
#include "PE/PEImportTable.h"
#include "PE/PEResourceTable.h"
#include "PE/PEExceptionTable.h"
#include "PE/PECertificateTable.h"
#include "PE/PERelocationTable.h"
#include "PE/PEDebugInfoTable.h"
#include "PE/PEGlobalPointerTable.h"
#include "PE/PETlsTable.h"
#include "PE/PELoadConfigTable.h"
#include "PE/PEBoundImportTable.h"
#include "PE/PEImportAddressTable.h"
#include "PE/PEClrTable.h"

LIBPE_NAMESPACE_BEGIN

template <class T>
LibPEPtr<PEParserT<T>>
PEParserT<T>::Create(PEParserType nType)
{
    switch(nType) {
    case PE_PARSER_TYPE_DISK_FILE:
        return new PEParserDiskFileT<T>;
    }

    return NULL;
}

template <class T>
PEAddress
PEParserT<T>::GetRVAFromVA(_In_ PEAddress nVA)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nVA, LIBPE_INVALID_ADDRESS);
    LIBPE_STRICTCHK(NULL != m_pFile);

    LibPERawOptionalHeaderT(T) *pOptinalHeader = (LibPERawOptionalHeaderT(T) *)m_pFile->GetRawOptionalHeader();
    LIBPE_CHK(NULL != pOptinalHeader, LIBPE_INVALID_ADDRESS);
    if(nVA < pOptinalHeader->ImageBase) {
        return LIBPE_INVALID_ADDRESS;
    }

    return nVA - pOptinalHeader->ImageBase;
}

template <class T>
PEAddress
PEParserT<T>::GetVAFromRVA(_In_ PEAddress nRVA)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nRVA, LIBPE_INVALID_ADDRESS);
    LIBPE_STRICTCHK(NULL != m_pFile);

    LibPERawOptionalHeaderT(T) *pOptinalHeader = (LibPERawOptionalHeaderT(T) *)m_pFile->GetRawOptionalHeader();
    LIBPE_CHK(NULL != pOptinalHeader, LIBPE_INVALID_ADDRESS);

    return pOptinalHeader->ImageBase + nRVA;
}

template <class T>
PEAddress
PEParserT<T>::GetRVAFromFOA(_In_ PEAddress nFOA)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nFOA, LIBPE_INVALID_ADDRESS);
    LIBPE_STRICTCHK(NULL != m_pFile);
    
    LibPEPtr<IPESection> pLastSection, pSection;
    UINT32 nSectionId = 0, nSectionCount = m_pFile->GetSectionCount();
    for(nSectionId = 0; nSectionId < nSectionCount; ++nSectionId) {
        if(FAILED(m_pFile->GetSection(nSectionId, &pSection)) || NULL == pSection) {
            return LIBPE_INVALID_ADDRESS;
        }

        if(pSection->GetFOA() > nFOA) {
            break;
        }

        pLastSection.Attach(pSection.Detach());
    }

    if (nSectionId == nSectionCount) {
        LibPEPtr<IPEOverlay> pOverlay;
        if (SUCCEEDED(m_pFile->GetOverlay(&pOverlay)) && NULL != pOverlay) {
            if (nFOA >= pOverlay->GetFOA()) {
                return pOverlay->GetRVA() + nFOA - pOverlay->GetFOA();
            }
        }
    }

    if(NULL == pLastSection) {
        return nFOA;
    }

    return pLastSection->GetRVA() + nFOA - pLastSection->GetFOA();
}

template <class T>
PEAddress
PEParserT<T>::GetFOAFromRVA(_In_ PEAddress nRVA)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nRVA, LIBPE_INVALID_ADDRESS);
    LIBPE_STRICTCHK(NULL != m_pFile);
    
    LibPEPtr<IPESection> pLastSection, pSection;
    UINT32 nSectionId = 0, nSectionCount = m_pFile->GetSectionCount();
    for(nSectionId = 0; nSectionId < nSectionCount; ++nSectionId) {
        if(FAILED(m_pFile->GetSection(nSectionId, &pSection)) || NULL == pSection) {
            return LIBPE_INVALID_ADDRESS;
        }

        if(pSection->GetRVA() > nRVA) {
            break;
        }

        pLastSection.Attach(pSection.Detach());
    }

    if (nSectionId == nSectionCount) {
        LibPEPtr<IPEOverlay> pOverlay;
        if (SUCCEEDED(m_pFile->GetOverlay(&pOverlay)) && NULL != pOverlay) {
            if (nRVA >= pOverlay->GetRVA()) {
                return pOverlay->GetFOA() + nRVA - pOverlay->GetRVA();
            }
        }
    }

    if(NULL == pLastSection) {
        return nRVA;
    }

    return pLastSection->GetFOA() + nRVA - pLastSection->GetRVA();
}

template <class T>
PEAddress
PEParserT<T>::GetVAFromFOA(_In_ PEAddress nFOA)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nFOA, LIBPE_INVALID_ADDRESS);
    return GetVAFromRVA(GetRVAFromFOA(nFOA));
}

template <class T>
PEAddress
PEParserT<T>::GetFOAFromVA(_In_ PEAddress nVA)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nVA, LIBPE_INVALID_ADDRESS);
    return GetFOAFromRVA(GetRVAFromVA(nVA));
}

template <class T>
void *
PEParserT<T>::GetRawMemory(_In_ UINT64 nOffset, _In_ UINT64 nSize)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(LIBPE_INVALID_SIZE != nSize);
    return m_pLoader->GetBuffer(nOffset, nSize);
}

template <class T>
const char *
PEParserT<T>::ParseAnsiString(_In_ PEAddress nRVA, _In_ PEAddress nFOA, UINT64 &nSize)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    PEAddress nOffset = GetRawOffset(nRVA, nFOA);
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nOffset, NULL);

    return m_pLoader->GetAnsiString(nOffset, nSize);
}

template <class T>
const wchar_t *
PEParserT<T>::ParseUnicodeString(_In_ PEAddress nRVA, _In_ PEAddress nFOA, UINT64 &nSize)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    PEAddress nOffset = GetRawOffset(nRVA, nFOA);
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nOffset, NULL);

    return m_pLoader->GetUnicodeString(nOffset, nSize);
}

template <class T>
HRESULT
PEParserT<T>::ParseBasicInfo(_Outptr_ IPEDosHeader **ppDosHeader, _Outptr_ IPENtHeaders **ppNtHeaders, _Out_ SectionHeaderList *pSectionHeaders, _Outptr_ IPEOverlay **ppOverlay)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    // Parse dos header
    LibPEPtr<PEDosHeaderT<T>> pInnerDosHeader = new PEDosHeaderT<T>();
    {
        PEElementParsingScopeT<T> oDosHeaderParsingScope(pInnerDosHeader);
        pInnerDosHeader->InnerSetBase(m_pFile, this);
        pInnerDosHeader->InnerSetMemoryInfo(0, LIBPE_INVALID_ADDRESS, sizeof(LibPERawDosHeaderT(T)));
        pInnerDosHeader->InnerSetFileInfo(0, sizeof(LibPERawDosHeaderT(T)));
    }

    LibPERawDosHeaderT(T) *pRawDosHeader = pInnerDosHeader->GetRawStruct();
    LIBPE_CHK(NULL != pRawDosHeader, E_BOUNDS);
    LIBPE_CHK(IMAGE_DOS_SIGNATURE == pRawDosHeader->e_magic, E_INVALID_PROTOCOL_FORMAT);

    // Parse nt headers
    LibPEPtr<PENtHeadersT<T>> pInnerNtHeaders = new PENtHeadersT<T>();
    {
        PEElementParsingScopeT<T> oNtHeadersParsingScope(pInnerNtHeaders);
        pInnerNtHeaders->InnerSetBase(m_pFile, this);
        pInnerNtHeaders->InnerSetMemoryInfo(pRawDosHeader->e_lfanew, LIBPE_INVALID_ADDRESS, sizeof(LibPERawNtHeadersT(T)));
        pInnerNtHeaders->InnerSetFileInfo(pRawDosHeader->e_lfanew, sizeof(LibPERawNtHeadersT(T)));
    }

    LibPERawNtHeadersT(T) *pRawNtHeaders = pInnerNtHeaders->GetRawStruct();
    LIBPE_CHK(IMAGE_NT_SIGNATURE == pRawNtHeaders->Signature, E_UNEXPECTED);

    if (PETrait<T>::PointerSize == 4) {
        if (pRawNtHeaders->FileHeader.SizeOfOptionalHeader != sizeof(PERawOptionalHeader32)) {
            return E_UNEXPECTED;
        }
    }

    // Parse file header
    LibPEPtr<PEFileHeaderT<T>> pInnerFileHeader = new PEFileHeaderT<T>();
    {
        PEElementParsingScopeT<T> oFileHeaderParsingScope(pInnerFileHeader);
        pInnerFileHeader->InnerSetBase(m_pFile, this);
        pInnerFileHeader->InnerSetMemoryInfo(pRawDosHeader->e_lfanew + sizeof(UINT32), LIBPE_INVALID_ADDRESS, sizeof(LibPERawFileHeaderT(T)));
        pInnerFileHeader->InnerSetFileInfo(pRawDosHeader->e_lfanew + sizeof(UINT32), sizeof(LibPERawFileHeaderT(T)));
        pInnerNtHeaders->InnerSetFileHeader(pInnerFileHeader.p);
    }

    LibPEPtr<PEOptionalHeaderT<T>> pInnerOptionalHeader = new PEOptionalHeaderT<T>();
    {
        PEElementParsingScopeT<T> oOptionalHeaderParsingScope(pInnerOptionalHeader);
        pInnerOptionalHeader->InnerSetBase(m_pFile, this);
        pInnerOptionalHeader->InnerSetMemoryInfo(pRawDosHeader->e_lfanew + sizeof(UINT32) + sizeof(LibPERawFileHeaderT(T)), LIBPE_INVALID_ADDRESS, sizeof(LibPERawOptionalHeaderT(T)));
        pInnerOptionalHeader->InnerSetFileInfo(pRawDosHeader->e_lfanew + sizeof(UINT32) + sizeof(LibPERawFileHeaderT(T)), sizeof(LibPERawOptionalHeaderT(T)));
        pInnerNtHeaders->InnerSetOptionalHeader(pInnerOptionalHeader.p);
    }

    *ppDosHeader = pInnerDosHeader.Detach();
    *ppNtHeaders = pInnerNtHeaders.Detach();

    // Parse section headers
    UINT32 nStartSectionHeaderOffset = pRawDosHeader->e_lfanew + sizeof(DWORD) + sizeof(LibPERawFileHeaderT(T)) + pRawNtHeaders->FileHeader.SizeOfOptionalHeader;
    UINT32 nSectionHeaderOffset = nStartSectionHeaderOffset;
    for (UINT16 nSectionId = 0; nSectionId < pRawNtHeaders->FileHeader.NumberOfSections; ++nSectionId) {
        nSectionHeaderOffset = nStartSectionHeaderOffset + nSectionId * sizeof(LibPERawSectionHeaderT(T));

        LibPEPtr<PESectionHeaderT<T>> pSectionHeader = new PESectionHeaderT<T>();
        {
            PEElementParsingScopeT<T> oSectionHeaderParsingScope(pSectionHeader);
            pSectionHeader->InnerSetBase(m_pFile, this);
            pSectionHeader->InnerSetMemoryInfo(nSectionHeaderOffset, LIBPE_INVALID_ADDRESS, sizeof(LibPERawOptionalHeaderT(T)));
            pSectionHeader->InnerSetFileInfo(nSectionHeaderOffset, sizeof(LibPERawOptionalHeaderT(T)));
        }

        pSectionHeaders->push_back(pSectionHeader.p);
    }

    // Parse extra data
    PEAddress nOverlayBeginFOA = nStartSectionHeaderOffset;
    PEAddress nOverlayBeginRVA = nStartSectionHeaderOffset;
    if (pSectionHeaders->size() > 0) {
        LibPEPtr<IPESection> pSection;
        if (FAILED(pSectionHeaders->back()->GetSection(&pSection)) || NULL == pSection) {
            return E_FAIL;
        }

        nOverlayBeginFOA = pSection->GetFOA() + pSection->GetSizeInFile();
        nOverlayBeginRVA = pSection->GetRVA() + pSection->GetSizeInMemory();
    }

    PEAddress nFileSize = (PEAddress)(m_pLoader->GetSize());
    if (nOverlayBeginFOA < nFileSize) {
        PEAddress nOverlaySize = nFileSize - nOverlayBeginFOA;

        LibPEPtr<PEOverlayT<T>> pOverlay = new PEOverlayT<T>();

        PEElementParsingScopeT<T> oOverlayParsingScope(pOverlay);
        pOverlay->InnerSetBase(m_pFile, this);
        pOverlay->InnerSetMemoryInfo(nOverlayBeginRVA, LIBPE_INVALID_ADDRESS, nOverlaySize);
        pOverlay->InnerSetFileInfo(nOverlayBeginFOA, nOverlaySize);

        *ppOverlay = pOverlay.Detach();
    }

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseSection(_In_ LibPERawSectionHeaderT(T) *pSectionHeader, _Outptr_ IPESection **ppSection)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    LibPEPtr<PESectionT<T>> pInnerSection = new PESectionT<T>();
    PEElementParsingScopeT<T> oSectionParsingScope(pInnerSection);

    InitPEElement(
        pInnerSection,
        pSectionHeader->VirtualAddress,
        pSectionHeader->SizeOfRawData,
        pSectionHeader->PointerToRawData,
        pSectionHeader->SizeOfRawData
        );

    pInnerSection->InnerSetSectionHeader(pSectionHeader);

    *ppSection = pInnerSection.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseExportTable(_Outptr_ IPEExportTable **ppExportTable)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppExportTable = NULL;

    LibPEPtr<PEExportTableT<T>> pExportTable = new PEExportTableT<T>();
    PEElementParsingScopeT<T> oExportTableParsingScope(pExportTable);
    LIBPE_CHK_HR(ParseDataDirectory(pExportTable.p, IMAGE_DIRECTORY_ENTRY_EXPORT));

    LibPERawExportDirectory(T) *pExportDirectory = pExportTable->GetRawStruct();
    LIBPE_CHK(NULL != pExportDirectory, E_BOUNDS);

    PEAddress nFunctionListOffset = GetRawOffsetFromAddressField(pExportDirectory->AddressOfFunctions);
    PEAddress nNameListOffset = GetRawOffsetFromAddressField(pExportDirectory->AddressOfNames);
    PEAddress nNameOrdinalListOffset = GetRawOffsetFromAddressField(pExportDirectory->AddressOfNameOrdinals);

    UINT32 *pFunctionList = (UINT32 *)m_pLoader->GetBuffer(nFunctionListOffset, pExportDirectory->NumberOfFunctions * sizeof(PEAddress));
    UINT32 *pNameList = (UINT32 *)m_pLoader->GetBuffer(nNameListOffset, pExportDirectory->NumberOfNames * sizeof(PEAddress));
    UINT16 *pNameOrdinalList = (UINT16 *)m_pLoader->GetBuffer(nNameOrdinalListOffset, pExportDirectory->NumberOfFunctions * sizeof(UINT16));

    LIBPE_CHK(NULL != pFunctionList && NULL != pNameList && NULL != pNameOrdinalList, E_BOUNDS);

    pExportTable->InnerSetFunctionList(pFunctionList);
    pExportTable->InnerSetNameList(pNameList);
    pExportTable->InnerSetNameOrdinalList(pNameOrdinalList);

    LIBPE_CHK_HR(pExportTable->PrepareForUsing());

    *ppExportTable = pExportTable.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseExportFunction(_In_ IPEExportTable *pExportTable, UINT32 nIndex, _Outptr_ IPEExportFunction **ppFunction)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    PEExportTableT<T> *pRawExportTable = static_cast<PEExportTableT<T> *>(pExportTable);
    UINT32 *pFunctionList = pRawExportTable->GetRawFunctionList();
    UINT32 *pNameList = pRawExportTable->GetRawNameList();
    UINT16 *pNameOrdinalList = pRawExportTable->GetRawNameOrdinalList();

    LIBPE_CHK(NULL != pFunctionList && NULL != pNameList && NULL != pNameOrdinalList, E_BOUNDS);

    PEAddress nFunctionRVA = pFunctionList[nIndex];
    PEAddress nNameRVA = (nIndex < pRawExportTable->GetFunctionCount()) ? pNameList[nIndex] : LIBPE_INVALID_ADDRESS;
    UINT16 nNameOrdinal = pNameOrdinalList[nIndex];

    LibPEPtr<PEExportFunctionT<T>> pExportFunction = new PEExportFunctionT<T>();
    PEElementParsingScopeT<T> oExportFunctionParsingScope(pExportFunction);
    InitPEElement(pExportFunction, nFunctionRVA, LIBPE_INVALID_ADDRESS, LIBPE_INVALID_SIZE);     // We don't know the exact length of the function, so we always return invalid size.

    pExportFunction->InnerSetOrdinal(nNameOrdinal);

    if (nNameRVA != LIBPE_INVALID_ADDRESS) {
        PEAddress nNameFOA = GetFOAFromRVA(nNameRVA);
        UINT64 nNameBufferSize = 0;
        const char *pName = m_pLoader->GetAnsiString(nNameFOA, nNameBufferSize);
        pExportFunction->InnerSetName(pName);
    }

    *ppFunction = pExportFunction.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseImportTable(_Outptr_ IPEImportTable **ppImportTable)
{
    return ParseDataDirectoryToInterface<IPEImportTable, PEImportTableT<T>>(IMAGE_DIRECTORY_ENTRY_IMPORT, ppImportTable);
}

template <class T>
HRESULT
PEParserT<T>::ParseAllImportModules(_In_ IPEImportTable *pImportTable)
{
    PEImportTableT<T> *pRawImportTable = static_cast<PEImportTableT<T> *>(pImportTable);
    LibPERawImportDescriptor(T) *pImportDesc = pRawImportTable->GetRawStruct();
    LIBPE_CHK(pImportDesc != NULL, E_BOUNDS);

    PEAddress nImportDescRVA = pRawImportTable->GetRVA(), nImportDescFOA = pRawImportTable->GetFOA();
    while (0 != pImportDesc->Characteristics && 0 != pImportDesc->Name) {
        LibPEPtr<IPEImportModule> pImportModule;
        if (SUCCEEDED(ParseImportModule(nImportDescRVA, nImportDescFOA, pImportDesc, &pImportModule))) {
            pRawImportTable->InnerAddImportModule(pImportModule);
        }

        ++pImportDesc;
        nImportDescRVA += sizeof(LibPERawImportDescriptor(T));
        nImportDescFOA += sizeof(LibPERawImportDescriptor(T));
    }

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseImportModule(_In_ PEAddress nImportDescRVA, _In_ PEAddress nImportDescFOA, LibPERawImportDescriptor(T) *pImportDescriptor, _Outptr_ IPEImportModule **ppImportModule)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nImportDescRVA && LIBPE_INVALID_ADDRESS != nImportDescFOA && NULL != pImportDescriptor, E_INVALIDARG);

    *ppImportModule = NULL;

    PEAddress nImportNameFOA = GetFOAFromRVA(pImportDescriptor->Name);
    if (LIBPE_INVALID_ADDRESS == nImportNameFOA) {
        return E_FAIL;
    }

    UINT64 nNameBufferSize = 0;
    const char *pImportName = m_pLoader->GetAnsiString(nImportNameFOA, nNameBufferSize);

    LibPEPtr<PEImportModuleT<T>> pImportModule = new PEImportModuleT<T>();
    PEElementParsingScopeT<T> oImportModuleParsingScope(pImportModule);

    pImportModule->InnerSetBase(m_pFile, this);
    pImportModule->InnerSetMemoryInfo(nImportDescRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawImportDescriptor(T)));
    pImportModule->InnerSetFileInfo(nImportDescFOA, sizeof(LibPERawImportDescriptor(T)));
    pImportModule->InnerSetName(pImportName);

    *ppImportModule = pImportModule.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseAllImportFunctions(_In_ IPEImportModule *pImportModule)
{
    PEImportModuleT<T> *pInnerImportModule = (PEImportModuleT<T> *)pImportModule;

    LibPERawImportDescriptor(T) *pImportDescriptor = pInnerImportModule->GetRawStruct();
    LIBPE_CHK(NULL != pImportDescriptor, E_BOUNDS);

    // By default, we use the first bridge to IMAGE_IMPORT_BY_NAME. But in some cases, the first bridge is NULL.
    // Compilers use the second bridge only. So we should fix the thunk entry at that time.
    PEAddress nThunkDataRVA = pImportDescriptor->OriginalFirstThunk;
    if (0 == pImportDescriptor->OriginalFirstThunk) {
        nThunkDataRVA = pImportDescriptor->FirstThunk;
    }
    LIBPE_CHK(0 != nThunkDataRVA, E_FAIL);

    PEAddress nThunkDataFOA = GetFOAFromRVA(nThunkDataRVA);
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nThunkDataFOA, E_FAIL);

    for (;;) {
        LibPERawThunkData(T) *pThunkData = (LibPERawThunkData(T) *)m_pLoader->GetBuffer(nThunkDataFOA, sizeof(LibPERawThunkData(T)));
        LIBPE_CHK(NULL != pThunkData, E_BOUNDS);

        if (0 == pThunkData->u1.AddressOfData) {
            break;
        }

        LibPEPtr<IPEImportFunction> pImportFunction;
        LIBPE_CHK_HR(ParseImportFunction(nThunkDataRVA, nThunkDataFOA, &pImportFunction));

        pInnerImportModule->InnerAddImportFunction(pImportFunction);

        nThunkDataRVA += sizeof(LibPERawThunkData(T));
        nThunkDataFOA += sizeof(LibPERawThunkData(T));
    }

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseImportFunction(_In_ PEAddress nThunkDataRVA, _In_ PEAddress nThunkDataFOA, _Outptr_ IPEImportFunction **ppImportFunction)
{
    LibPEPtr<PEImportFunctionT<T>> pImportFunction = new PEImportFunctionT<T>();
    PEElementParsingScopeT<T> oImportFunctionParsingScope(pImportFunction);

    pImportFunction->InnerSetBase(m_pFile, this);
    pImportFunction->InnerSetMemoryInfo(nThunkDataRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawThunkData(T)));
    pImportFunction->InnerSetFileInfo(nThunkDataFOA, sizeof(LibPERawThunkData(T)));

    *ppImportFunction = pImportFunction.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseImportFunction(_In_ IPEImportFunction *pFunction)
{
    PEImportFunctionT<T> *pInnerFunction = (PEImportFunctionT<T> *)pFunction;

    LibPERawThunkData(T) *pThunkData = pInnerFunction->GetRawStruct();
    LIBPE_CHK(NULL != pThunkData, E_BOUNDS);

    // No IMAGE_IMPORT_BY_NAME structure if the function is imported by ordinal
    if (0 != (pThunkData->u1.Ordinal & PETrait<T>::ImageOrdinalFlag)) {
        return S_OK;
    }

    // Parse the function imported by IMAGE_IMPORT_BY_NAME
    PEAddress nRawImportByNameRVA = pThunkData->u1.AddressOfData;
    if (0 == nRawImportByNameRVA) {
        return E_FAIL;
    }

    PEAddress nRawImportByNameFOA = GetFOAFromRVA(nRawImportByNameRVA);
    if (LIBPE_INVALID_ADDRESS == nRawImportByNameFOA) {
        return E_FAIL;
    }

    LibPERawImportByName(T) *pImportByName = (LibPERawImportByName(T) *)m_pLoader->GetBuffer(nRawImportByNameFOA, sizeof(LibPERawImportByName(T)));
    if (NULL == pImportByName) {
        return E_BOUNDS;
    }

    UINT64 nNameBufferSize = 0;
    if (NULL == m_pLoader->GetAnsiString(nRawImportByNameFOA + sizeof(UINT16), nNameBufferSize)) {
        return E_BOUNDS;
    }

    PEAddress nRawImportByNameSize = (PEAddress)(sizeof(UINT16) + nNameBufferSize);

    pInnerFunction->InnerSetRawImportByName(pImportByName, nRawImportByNameRVA, nRawImportByNameFOA, nRawImportByNameSize);

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseResourceTable(_Outptr_ IPEResourceTable **ppResourceTable)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppResourceTable = NULL;

    LibPEPtr<PEResourceTableT<T>> pResourceTable = new PEResourceTableT<T>();
    PEElementParsingScopeT<T> oResourceTableParsingScope(pResourceTable);
    LIBPE_CHK_HR(ParseDataDirectory(pResourceTable.p, IMAGE_DIRECTORY_ENTRY_RESOURCE));

    LibPEPtr<IPEResourceDirectory> pRootDirectory;
    LIBPE_CHK_HR(ParseResourceDirectory(pResourceTable->GetRVA(), pResourceTable->GetFOA(), &pRootDirectory));

    pResourceTable->InnerSetRootDirectory(pRootDirectory.p);

    *ppResourceTable = pResourceTable.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseResourceDirectory(_In_ PEAddress nRVA, _In_ PEAddress nFOA, _Outptr_ IPEResourceDirectory **ppDirectory)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nRVA && LIBPE_INVALID_ADDRESS != nFOA, E_INVALIDARG);
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppDirectory = NULL;

    LibPEPtr<PEResourceDirectoryT<T>> pResourceDirectory = new PEResourceDirectoryT<T>();
    PEElementParsingScopeT<T> oResourceDirectoryParsingScope(pResourceDirectory);

    pResourceDirectory->InnerSetBase(m_pFile, this);
    pResourceDirectory->InnerSetMemoryInfo(nRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawResourceDirectory(T)));
    pResourceDirectory->InnerSetFileInfo(nFOA, sizeof(LibPERawResourceDirectory(T)));

    LibPERawResourceDirectory(T) *pRawDirectory = pResourceDirectory->GetRawStruct();
    LIBPE_CHK(NULL != pRawDirectory, E_BOUNDS);

    pResourceDirectory->InnerReserveEntry(pRawDirectory->NumberOfNamedEntries + pRawDirectory->NumberOfIdEntries);

    *ppDirectory = pResourceDirectory.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseResourceDirectoryEntry(_In_ IPEResourceDirectory *pDirectory, UINT32 nEntryIndex, _Outptr_ IPEResourceDirectoryEntry **ppEntry)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppEntry = NULL;

    PEAddress nFirstEntryRVA = pDirectory->GetRVA() + pDirectory->GetSizeInMemory();
    PEAddress nFirstEntryFOA = pDirectory->GetFOA() + pDirectory->GetSizeInFile();

    PEAddress nEntryRVA = nFirstEntryRVA + nEntryIndex * sizeof(LibPERawResourceDirectoryEntry(T));
    PEAddress nEntryFOA = nFirstEntryFOA + nEntryIndex * sizeof(LibPERawResourceDirectoryEntry(T));

    LibPEPtr<PEResourceDirectoryEntryT<T>> pResourceDirectoryEntry = new PEResourceDirectoryEntryT<T>();
    PEElementParsingScopeT<T> oResourceDirectoryEntryParsingScope(pResourceDirectoryEntry);

    pResourceDirectoryEntry->InnerSetBase(m_pFile, this);
    pResourceDirectoryEntry->InnerSetMemoryInfo(nEntryRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawResourceDirectoryEntry(T)));
    pResourceDirectoryEntry->InnerSetFileInfo(nEntryFOA, sizeof(LibPERawResourceDirectoryEntry(T)));

    *ppEntry = pResourceDirectoryEntry.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseResourceDataEntry(_In_ PEAddress nRVA, _In_ PEAddress nFOA, _Outptr_ IPEResourceDataEntry **ppDataEntry)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nRVA && LIBPE_INVALID_ADDRESS != nFOA, E_INVALIDARG);
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppDataEntry = NULL;

    LibPEPtr<PEResourceDataEntryT<T>> pInnerDataEntry = new PEResourceDataEntryT<T>();
    PEElementParsingScopeT<T> oDataEntryParsingScope(pInnerDataEntry);

    pInnerDataEntry->InnerSetBase(m_pFile, this);
    pInnerDataEntry->InnerSetMemoryInfo(nRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawResourceDataEntry(T)));
    pInnerDataEntry->InnerSetFileInfo(nFOA, sizeof(LibPERawResourceDataEntry(T)));

    *ppDataEntry = pInnerDataEntry.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseResource(_In_ IPEResourceDataEntry *pDataEntry, _Outptr_ IPEResource **ppResource)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppResource = NULL;

    LibPERawResourceDataEntry(T) *pRawDataEntry = (LibPERawResourceDataEntry(T) *)pDataEntry->GetRawMemory();
    LIBPE_CHK(NULL != pRawDataEntry, E_BOUNDS);

    LibPEPtr<PEResourceT<T>> pInnerResource = new PEResourceT<T>();
    PEElementParsingScopeT<T> oResourceParsingScope(pInnerResource);

    pInnerResource->InnerSetBase(m_pFile, this);
    pInnerResource->InnerSetMemoryInfo(pRawDataEntry->OffsetToData, LIBPE_INVALID_ADDRESS, pRawDataEntry->Size);
    pInnerResource->InnerSetFileInfo(0, pRawDataEntry->Size);

    *ppResource = pInnerResource.Detach();

    return S_OK;
}

template <class T>
LibPERawResourceString(T) *
PEParserT<T>::ParseResourceString(_In_ PEAddress nRVA, _In_ PEAddress nFOA, UINT64 &nSize)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    PEAddress nRawOffset = GetRawOffset(nRVA, nFOA);
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nRawOffset, NULL);

    UINT16 *pRawStringSize = (UINT16 *)m_pLoader->GetBuffer(nRawOffset, sizeof(UINT16));
    if (NULL == pRawStringSize) {
        return NULL;
    }

    if (NULL == m_pLoader->GetBuffer(nRawOffset + sizeof(UINT16), *pRawStringSize)) {
        return NULL;
    }

    return (LibPERawResourceString(T) *)pRawStringSize;
}

template <class T>
LibPERawResourceStringU(T) *
PEParserT<T>::ParseResourceStringU(_In_ PEAddress nRVA, _In_ PEAddress nFOA, UINT64 &nSize)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    PEAddress nRawOffset = GetRawOffset(nRVA, nFOA);
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nRawOffset, NULL);

    UINT16 *pRawStringSize = (UINT16 *)m_pLoader->GetBuffer(nRawOffset, sizeof(UINT16));
    if (NULL == pRawStringSize) {
        return NULL;
    }

    if (NULL == m_pLoader->GetBuffer(nRawOffset + sizeof(UINT16), *pRawStringSize)) {
        return NULL;
    }

    return (LibPERawResourceStringU(T) *)pRawStringSize;
}

template <class T>
HRESULT
PEParserT<T>::ParseExceptionTable(_Outptr_ IPEExceptionTable **ppExceptionTable)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppExceptionTable = NULL;

    LibPEPtr<PEExceptionTableT<T>> pExceptionTable = new PEExceptionTableT<T>();
    PEElementParsingScopeT<T> oExceptionTableParsingScope(pExceptionTable);

    LIBPE_CHK_HR(ParseDataDirectory(pExceptionTable.p, IMAGE_DIRECTORY_ENTRY_EXCEPTION));

    pExceptionTable->InnerSetExceptionHandlerCount((UINT32)(pExceptionTable->GetSizeInFile() / sizeof(LibPERawRuntimeFunctionEntry(T))));

    *ppExceptionTable = pExceptionTable.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseExceptionHandlerEntry(_In_ IPEExceptionTable *pExceptionTable, _In_ UINT32 nHandlerIndex, _Outptr_ IPEExceptionHandlerEntry **ppExceptionHandlerEntry)
{
    *ppExceptionHandlerEntry = NULL;

    PEAddress nExceptionHandlerEntryRVA = pExceptionTable->GetRVA() + sizeof(LibPERawRuntimeFunctionEntry(T)) * nHandlerIndex;
    PEAddress nExceptionHandlerEntryFOA = pExceptionTable->GetFOA() + sizeof(LibPERawRuntimeFunctionEntry(T)) * nHandlerIndex;

    LibPEPtr<PEExceptionHandlerEntryT<T>> pExceptionHandlerEntry = new PEExceptionHandlerEntryT<T>();
    PEElementParsingScopeT<T> oExceptionHandlerEntryParsingScope(pExceptionHandlerEntry);

    pExceptionHandlerEntry->InnerSetBase(m_pFile, this);
    pExceptionHandlerEntry->InnerSetMemoryInfo(nExceptionHandlerEntryRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawRuntimeFunctionEntry(T)));
    pExceptionHandlerEntry->InnerSetFileInfo(nExceptionHandlerEntryFOA, sizeof(LibPERawRuntimeFunctionEntry(T)));

    *ppExceptionHandlerEntry = pExceptionHandlerEntry.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseExceptionHandler(_In_ IPEExceptionHandlerEntry *pExceptionHandlerEntry, _Outptr_ IPEExceptionHandler **ppExceptionHandler)
{
    PEExceptionHandlerEntryT<T> *pInnerExceptionHandlerEntry = (PEExceptionHandlerEntryT<T> *)pExceptionHandlerEntry;

    LibPERawRuntimeFunctionEntry(T) *pRawRuntimeFunctionEntry = pInnerExceptionHandlerEntry->GetRawStruct();
    PEAddress nExceptionHandlerRVA = pRawRuntimeFunctionEntry->BeginAddress;
    PEAddress nExceptionHandlerSize = pRawRuntimeFunctionEntry->EndAddress - pRawRuntimeFunctionEntry->BeginAddress;

    LibPEPtr<PEExceptionHandlerT<T>> pExceptionHandler = new PEExceptionHandlerT<T>();
    PEElementParsingScopeT<T> oExceptionHandlerParsingScope(pExceptionHandler);

    pExceptionHandler->InnerSetBase(m_pFile, this);
    pExceptionHandler->InnerSetMemoryInfo(nExceptionHandlerRVA, LIBPE_INVALID_ADDRESS, nExceptionHandlerSize);
    pExceptionHandler->InnerSetFileInfo(LIBPE_INVALID_ADDRESS, nExceptionHandlerSize);

    *ppExceptionHandler = pExceptionHandler.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseCertificateTable(_Outptr_ IPECertificateTable **ppCertificateTable)
{
    return ParseDataDirectoryToInterface<IPECertificateTable, PECertificateTableT<T>>(IMAGE_DIRECTORY_ENTRY_SECURITY, ppCertificateTable);
}

template <class T>
HRESULT
PEParserT<T>::ParseCertificates(_Inout_ IPECertificateTable *pCertificateTable)
{
    PECertificateTableT<T> *pInnerCertificateTable = (PECertificateTableT<T> *)pCertificateTable;

    LibPERawWinCertificate(T) *pRawWinCertificate = pInnerCertificateTable->GetRawStruct();
    LIBPE_CHK(NULL != pRawWinCertificate, E_BOUNDS);

    PEAddress nCertificateRVA = pInnerCertificateTable->GetRVA();
    PEAddress nCertificateFOA = pInnerCertificateTable->GetFOA();
    PEAddress nCertificateListEndFOA = nCertificateFOA + pInnerCertificateTable->GetSizeInFile();

    UINT32 nCertificateSize = 0;
    while (nCertificateFOA < nCertificateListEndFOA) {
        // Round certificate size to 8 types, according to the PECoff v83 standard
        nCertificateSize = pRawWinCertificate->dwLength;
        if ((nCertificateSize & 0xFF) != 0) {
            nCertificateSize |= 0xFF;
            ++nCertificateSize;
        }

        LibPEPtr<PECertificateT<T>> pCertificate = new PECertificateT<T>();
        PEElementParsingScopeT<T> oCertificateParsingScope(pCertificate);

        pCertificate->InnerSetBase(m_pFile, this);
        pCertificate->InnerSetMemoryInfo(nCertificateRVA, LIBPE_INVALID_ADDRESS, pRawWinCertificate->dwLength);
        pCertificate->InnerSetFileInfo(nCertificateFOA, pRawWinCertificate->dwLength);

        pInnerCertificateTable->InnerAddCertificate(pCertificate);

        nCertificateRVA += nCertificateSize;
        nCertificateFOA += nCertificateSize;
        pRawWinCertificate = (LibPERawWinCertificate(T) *)(((UINT8 *)pRawWinCertificate) + nCertificateSize);
    }

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseRelocationTable(_Outptr_ IPERelocationTable **ppRelocationTable)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppRelocationTable = NULL;

    LibPEPtr<PERelocationTableT<T>> pRelocationTable = new PERelocationTableT<T>();
    PEElementParsingScopeT<T> oRelocationTableParsingScope(pRelocationTable);

    LIBPE_CHK_HR(ParseDataDirectory(pRelocationTable.p, IMAGE_DIRECTORY_ENTRY_BASERELOC));

    LibPERawBaseRelocation(T) *pRawRelocationPage = (LibPERawBaseRelocation(T) *)pRelocationTable->GetRawStruct();
    LIBPE_CHK(NULL != pRawRelocationPage, E_BOUNDS);

    PEAddress nRelocationPageRVA = pRelocationTable->GetRVA();
    PEAddress nRelocationPageFOA = pRelocationTable->GetFOA();
    while (0 != pRawRelocationPage->VirtualAddress) {
        UINT16 *pRawItemList = (UINT16 *)(&pRawRelocationPage[1]);
        UINT32 nItemIndex = 0;
        UINT32 nItemCount = (pRawRelocationPage->SizeOfBlock - sizeof(LibPERawBaseRelocation(T))) / sizeof(UINT16);

        LibPEPtr<PERelocationPageT<T>> pRelocationPage = new PERelocationPageT<T>;
        PEElementParsingScopeT<T> oRelocationPageParsingScope(pRelocationPage);

        PEAddress nPageSize = sizeof(LibPERawBaseRelocation(T)) + nItemCount * sizeof(UINT16);

        pRelocationPage->InnerSetBase(m_pFile, this);
        pRelocationPage->InnerSetMemoryInfo(nRelocationPageRVA, LIBPE_INVALID_ADDRESS, nPageSize);
        pRelocationPage->InnerSetFileInfo(nRelocationPageFOA, nPageSize);

        pRelocationTable->InnerAddRelocationPage(pRelocationPage);

        PEAddress nRelocationItemRVA = nRelocationPageRVA + sizeof(LibPERawBaseRelocation(T));
        PEAddress nRelocationItemFOA = nRelocationPageFOA + sizeof(LibPERawBaseRelocation(T));
        while (nItemIndex < nItemCount) {
            LibPEPtr<PERelocationItemT<T>> pRelocationItem = new PERelocationItemT<T>();
            PEElementParsingScopeT<T> oRelocationItemParsingScope(pRelocationItem);

            pRelocationItem->InnerSetBase(m_pFile, this);
            pRelocationItem->InnerSetMemoryInfo(nRelocationItemRVA, LIBPE_INVALID_ADDRESS, sizeof(UINT16));
            pRelocationItem->InnerSetFileInfo(nRelocationItemFOA, sizeof(UINT16));
            pRelocationItem->InnerSetRelocateFlag(pRawItemList[nItemIndex] & 0xF000);
            pRelocationItem->InnerSetAddressRVA(pRawRelocationPage->VirtualAddress + (pRawItemList[nItemIndex] & 0x0FFF));
            pRelocationPage->InnerAddRelocationItem(pRelocationItem);

            nRelocationItemRVA += sizeof(UINT16);
            nRelocationItemFOA += sizeof(UINT16);
            ++nItemIndex;
        }

        nRelocationPageRVA = nRelocationItemRVA;
        nRelocationPageFOA = nRelocationItemFOA;
        pRawRelocationPage = (LibPERawBaseRelocation(T) *)(((UINT8 *)pRawRelocationPage) + sizeof(LibPERawBaseRelocation(T)) + sizeof(UINT16) * nItemCount);
    }

    *ppRelocationTable = pRelocationTable.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseDebugInfoTable(_Outptr_ IPEDebugInfoTable **ppDebugInfoTable)
{
    return ParseDataDirectoryToInterface<IPEDebugInfoTable, PEDebugInfoTableT<T>>(IMAGE_DIRECTORY_ENTRY_DEBUG, ppDebugInfoTable);
}

template <class T>
HRESULT
PEParserT<T>::ParseGlobalPointerTable(_Outptr_ IPEGlobalPointerTable **ppGlobalPointerTable)
{
    // The global pointer(GP) is stored in the VirtualAddress field in the data directory, so the data directory is returned,
    // according to PE and COFF specification 8.3.
    LIBPE_CHK(NULL != ppGlobalPointerTable, E_POINTER);
    LIBPE_STRICTCHK(NULL != m_pFile);

    LibPEPtr<IPEOptionalHeader> pOptionHeader;
    LIBPE_CHK_HR(m_pFile->GetOptionalHeader(&pOptionHeader));

    PEAddress nGlobalPointerTableDataDirectoryRVA = pOptionHeader->GetRVA() + FIELD_OFFSET(LibPERawOptionalHeaderT(T), DataDirectory[IMAGE_DIRECTORY_ENTRY_GLOBALPTR]);
    PEAddress nGlobalPointerTableDataDirectoryFOA = pOptionHeader->GetFOA() + FIELD_OFFSET(LibPERawOptionalHeaderT(T), DataDirectory[IMAGE_DIRECTORY_ENTRY_GLOBALPTR]);

    LibPEPtr<PEGlobalPointerTableT<T>> pGlobalPointerTable = new PEGlobalPointerTableT<T>();
    PEElementParsingScopeT<T> oGlobalPointerTableParsingScope(pGlobalPointerTable);

    pGlobalPointerTable->InnerSetBase(m_pFile, this);
    pGlobalPointerTable->InnerSetMemoryInfo(nGlobalPointerTableDataDirectoryRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawDataDirectoryT(T)));
    pGlobalPointerTable->InnerSetFileInfo(nGlobalPointerTableDataDirectoryFOA, sizeof(LibPERawDataDirectoryT(T)));

    *ppGlobalPointerTable = pGlobalPointerTable.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseTlsTable(_Outptr_ IPETlsTable **ppTlsTable)
{
    return ParseDataDirectoryToInterface<IPETlsTable, PETlsTableT<T>>(IMAGE_DIRECTORY_ENTRY_TLS, ppTlsTable);
}

template <class T>
HRESULT
PEParserT<T>::ParseTlsCallbacks(_Inout_ IPETlsTable *pTlsTable)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    PETlsTableT<T> *pInnerTlsTable = (PETlsTableT<T> *)pTlsTable;

    LibPERawTlsDirectory(T) *pRawTlsDirectory = pInnerTlsTable->GetRawStruct();
    LIBPE_CHK(NULL != pRawTlsDirectory, E_BOUNDS);
    LIBPE_CHK(NULL != pRawTlsDirectory->AddressOfCallBacks && pRawTlsDirectory->AddressOfCallBacks >= m_pFile->GetImageBase(), E_UNEXPECTED);

    PEAddress nCallbackEntryRVA = pRawTlsDirectory->AddressOfCallBacks - m_pFile->GetImageBase();
    PEAddress nCallbackEntryFOA = GetFOAFromRVA(nCallbackEntryRVA);
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nCallbackEntryFOA, E_UNEXPECTED);

    PEAddress nCallbackAddress = 0, nCallbackRVA = 0;
    for (;;) {
        void *pPointerToCallback = m_pLoader->GetBuffer(nCallbackEntryFOA, PETrait<T>::PointerSize);
        LIBPE_CHK(NULL != pPointerToCallback, E_BOUNDS);

        nCallbackAddress = 0;
        memcpy_s(&nCallbackAddress, sizeof(PEAddress), pPointerToCallback, PETrait<T>::PointerSize);

        if (0 == nCallbackAddress) {
            break;
        }

        nCallbackRVA = GetRVAFromAddressField(nCallbackAddress);
        LIBPE_CHK(LIBPE_INVALID_ADDRESS != nCallbackRVA, E_UNEXPECTED);
        pInnerTlsTable->InnerAddCallbackRVA(nCallbackRVA);

        nCallbackEntryFOA += PETrait<T>::PointerSize;
    }

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseLoadConfigTable(_Outptr_ IPELoadConfigTable **ppLoadConfigTable)
{
    return ParseDataDirectoryToInterface<IPELoadConfigTable, PELoadConfigTableT<T>>(IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, ppLoadConfigTable);
}

template <class T>
HRESULT
PEParserT<T>::ParseBoundImportTable(_Outptr_ IPEBoundImportTable **ppBoundImportTable)
{
    return ParseDataDirectoryToInterface<IPEBoundImportTable, PEBoundImportTableT<T>>(IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT, ppBoundImportTable);
}

template <class T>
HRESULT
PEParserT<T>::ParseBoundImportModules(_In_ IPEBoundImportTable *pBoundImportTable)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    PEBoundImportTableT<T> *pInnerBoundImportTable = (PEBoundImportTableT<T> *)pBoundImportTable;
    PEElementParsingScopeT<T> oBoundImportTableParsingScope(pInnerBoundImportTable);

    PEAddress nImportModuleRVA = pInnerBoundImportTable->GetRVA();
    PEAddress nImportModuleFOA = pInnerBoundImportTable->GetFOA();
    PEAddress nImportTableEndFOA = nImportModuleFOA + pInnerBoundImportTable->GetSizeInFile();
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nImportModuleRVA || LIBPE_INVALID_ADDRESS != nImportModuleFOA, E_UNEXPECTED);

    // Parse bound modules
    while (nImportModuleFOA < nImportTableEndFOA) {
        LibPEPtr<PEBoundImportModuleT<T>> pImportModule = new PEBoundImportModuleT<T>();
        PEElementParsingScopeT<T> oImportModuleParsingScope(pImportModule);

        pImportModule->InnerSetBase(m_pFile, this);
        pImportModule->InnerSetMemoryInfo(nImportModuleRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawBoundImportDescriptor(T)));
        pImportModule->InnerSetFileInfo(nImportModuleFOA, sizeof(LibPERawBoundImportDescriptor(T)));

        pInnerBoundImportTable->InnerAddBoundImportModule(pImportModule);

        // Parse bound forwarders
        LibPERawBoundImportDescriptor(T) *pRawBoundImportDescriptor = pInnerBoundImportTable->GetRawStruct();
        LIBPE_CHK(NULL != pRawBoundImportDescriptor, E_BOUNDS);

        UINT16 nForwarderCount = pRawBoundImportDescriptor->NumberOfModuleForwarderRefs;

        PEAddress nForwarderRVA = nImportModuleRVA + sizeof(LibPERawBoundImportDescriptor(T));
        PEAddress nForwarderFOA = nImportModuleFOA + sizeof(LibPERawBoundImportDescriptor(T));
        for (UINT16 nForwarderIndex = 0; nForwarderIndex < nForwarderCount; ++nForwarderIndex) {
            LibPEPtr<PEBoundForwarderT<T>> pForwarder = new PEBoundForwarderT<T>();
            PEElementParsingScopeT<T> oForwarderParsingScope(pForwarder);

            pForwarder->InnerSetBase(m_pFile, this);
            pForwarder->InnerSetMemoryInfo(nForwarderRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawBoundForwarderRef(T)));
            pForwarder->InnerSetFileInfo(nForwarderFOA, sizeof(LibPERawBoundForwarderRef(T)));

            pImportModule->InnerAddBoundForwarder(pForwarder);

            nForwarderRVA += sizeof(LibPERawBoundForwarderRef(T));
            nForwarderFOA += sizeof(LibPERawBoundForwarderRef(T));
        }

        nImportModuleRVA += sizeof(LibPERawBoundImportDescriptor(T)) + nForwarderCount * sizeof(LibPERawBoundForwarderRef(T));
        nImportModuleFOA += sizeof(LibPERawBoundImportDescriptor(T)) + nForwarderCount * sizeof(LibPERawBoundForwarderRef(T));
    }

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseImportAddressTable(_Outptr_ IPEImportAddressTable **ppImportAddressTable)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);

    *ppImportAddressTable = NULL;

    LibPEPtr<PEImportAddressTableT<T>> pImportAddressTable = new PEImportAddressTableT<T>();
    PEElementParsingScopeT<T> oImportAddressTableParsingScope(pImportAddressTable);

    LIBPE_CHK_HR(ParseDataDirectory(pImportAddressTable.p, IMAGE_DIRECTORY_ENTRY_IAT));

    if (FAILED(ParseImportAddressTableContent(pImportAddressTable))) {
        return E_FAIL;
    }

    *ppImportAddressTable = pImportAddressTable.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseImportAddressTableContent(_Inout_ IPEImportAddressTable *pImportAddressTable)
{
    LibPERawThunkData(T) *pRawTable = (LibPERawThunkData(T) *)pImportAddressTable->GetRawMemory();
    LIBPE_CHK(NULL != pRawTable, E_BOUNDS);

    PEImportAddressTableT<T> *pInnerImportAddressTable = static_cast<PEImportAddressTableT<T> *>(pImportAddressTable);
    PEAddress nTableRVA = pImportAddressTable->GetRVA();
    PEAddress nTableFOA = pImportAddressTable->GetFOA();

    LibPERawThunkData(T) *pRawBlock = pRawTable;
    PEAddress nParsedSize = 0, nTotalSize = pInnerImportAddressTable->GetSizeInFile();

    while (nParsedSize < nTotalSize && 0 != pRawBlock->u1.Function) {
        LibPEPtr<IPEImportAddressBlock> pBlock;
        if (FAILED(ParseImportAddressBlock(pRawBlock, nTableRVA + nParsedSize, nTableFOA + nParsedSize, &pBlock)) || NULL == pBlock) {
            return E_FAIL;
        }
        pInnerImportAddressTable->InnerAddImportAddressBlock(pBlock);

        nParsedSize += pBlock->GetSizeInFile();
        pRawBlock = (LibPERawThunkData(T) *)(((UINT8 *)pRawTable) + nParsedSize);
    }

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseImportAddressBlock(_In_ LibPERawThunkData(T) *pRawBlock, _In_ PEAddress nBlockRVA, _In_ PEAddress nBlockFOA, _Outptr_ IPEImportAddressBlock **ppBlock)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nBlockRVA, E_INVALIDARG);

    *ppBlock = NULL;

    LibPEPtr<PEImportAddressBlockT<T>> pBlock = new PEImportAddressBlockT<T>();
    PEElementParsingScopeT<T> oBlockParsingScope(pBlock);

    pBlock->InnerSetBase(m_pFile, this);
    pBlock->InnerSetRawMemory(pRawBlock);
    pBlock->InnerSetMemoryInfo(nBlockRVA, LIBPE_INVALID_ADDRESS, 0);
    pBlock->InnerSetFileInfo(nBlockFOA, 0);

    // Reload FOA to ensure it is avaliable. Because we need it right now.
    if (LIBPE_INVALID_ADDRESS == nBlockFOA) {
        nBlockFOA = GetFOAFromRVA(nBlockRVA);
    }

    // If the RawBlock is NULL, we should ready the memory ourself.
    LibPERawThunkData(T) *pRawItem = NULL;
    BOOL bNeedLoadMemory = false;
    if (NULL == pRawBlock) {
        LIBPE_STRICTCHK(NULL != m_pLoader);
        pRawItem = (LibPERawThunkData(T) *)m_pLoader->GetBuffer(nBlockFOA, sizeof(LibPERawThunkData(T)));
        LIBPE_CHK(NULL != pRawItem, E_BOUNDS);
        bNeedLoadMemory = true;
    }
    else {
        pRawItem = pRawBlock;
    }

    // Parse the child import address item. If you don't do this, we will never know the size of this block.
    PEAddress nBlockSize = 0;
    while (pRawItem->u1.Function != 0) {
        LibPEPtr<IPEImportAddressItem> pItem;
        if (FAILED(ParseImportAddressItem(pRawItem, nBlockRVA + nBlockSize, nBlockFOA + nBlockSize, &pItem))) {
            return E_FAIL;
        }
        pBlock->InnerAddImportAddressItem(pItem);

        nBlockSize += sizeof(LibPERawThunkData(T));

        if (bNeedLoadMemory) {
            pRawItem = (LibPERawThunkData(T) *)m_pLoader->GetBuffer(nBlockFOA + nBlockSize, sizeof(LibPERawThunkData(T)));
            LIBPE_CHK(NULL != pRawItem, E_BOUNDS);
        }
        else {
            ++pRawItem;
        }
    }

    nBlockSize += sizeof(LibPERawThunkData(T));

    pBlock->InnerSetMemoryInfo(nBlockRVA, LIBPE_INVALID_ADDRESS, nBlockSize);
    pBlock->InnerSetFileInfo(nBlockFOA, nBlockSize);

    *ppBlock = pBlock.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseImportAddressItem(_In_ LibPERawThunkData(T) *pRawItem, _In_ PEAddress nItemRVA, _In_ PEAddress nItemFOA, _Outptr_ IPEImportAddressItem **ppItem)
{
    LIBPE_CHK(LIBPE_INVALID_ADDRESS != nItemRVA && LIBPE_INVALID_ADDRESS != nItemFOA, E_INVALIDARG);

    *ppItem = NULL;

    LibPEPtr<PEImportAddressItemT<T>> pItem = new PEImportAddressItemT<T>();
    PEElementParsingScopeT<T> oItemParsingScope(pItem);

    pItem->InnerSetBase(m_pFile, this);
    pItem->InnerSetRawMemory(pRawItem);
    pItem->InnerSetMemoryInfo(nItemRVA, LIBPE_INVALID_ADDRESS, sizeof(LibPERawThunkData(T)));
    pItem->InnerSetFileInfo(nItemFOA, sizeof(LibPERawThunkData(T)));

    *ppItem = pItem.Detach();

    return S_OK;
}

template <class T>
HRESULT
PEParserT<T>::ParseDelayImportTable(_Outptr_ IPEDelayImportTable **ppDelayImportTable)
{
    return E_NOTIMPL;
}

template <class T>
HRESULT
PEParserT<T>::ParseClrTable(_Outptr_ IPEClrTable **ppClrTable)
{
    // We don't parse the internal data strucure of CLR table for now.
    return ParseDataDirectoryToInterface<IPEClrTable, PEClrTableT<T>>(IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR, ppClrTable);
}

template <class T>
PEAddress
PEParserT<T>::GetRawOffset(_In_ PEAddress nRVA, _In_ PEAddress nFOA)
{
    LIBPE_STRICTCHK(NULL != m_pLoader);
    LIBPE_STRICTCHK(NULL != m_pFile);
    return (LIBPE_INVALID_ADDRESS != nRVA) ? GetRawOffsetFromRVA(nRVA) : GetRawOffsetFromFOA(nFOA);
}

template <class T>
HRESULT
PEParserT<T>::GetDataDirectoryEntry(_In_ INT32 nDataDirectoryEntryIndex, _In_ PEAddress &nRVA, _In_ PEAddress &nFOA, _In_ PEAddress &nSize)
{
    LIBPE_STRICTCHK(NULL != m_pFile);

    nRVA = LIBPE_INVALID_ADDRESS;
    nFOA = LIBPE_INVALID_ADDRESS;
    nSize = LIBPE_INVALID_SIZE;

    LibPERawOptionalHeaderT(T) *pOptionalHeader = (LibPERawOptionalHeaderT(T) *)m_pFile->GetRawOptionalHeader();
    if (NULL == pOptionalHeader) {
        return E_FAIL;
    }

    LibPERawDataDirectoryT(T) *pDataDirectory = &(pOptionalHeader->DataDirectory[nDataDirectoryEntryIndex]);
    if (NULL == pDataDirectory || 0 == pDataDirectory->VirtualAddress || 0 == pDataDirectory->Size) {
        return E_NOT_SET;
    }

    switch (nDataDirectoryEntryIndex) {
    case IMAGE_DIRECTORY_ENTRY_SECURITY:
        nFOA = pDataDirectory->VirtualAddress;
        nRVA = GetRVAFromFOA(nFOA);
        break;
    default:
        nRVA = GetRVAFromAddressField(pDataDirectory->VirtualAddress);
        if (LIBPE_INVALID_ADDRESS != nRVA) {
            nFOA = GetFOAFromRVA(nRVA);
        }
    }

    if (LIBPE_INVALID_ADDRESS == nRVA || LIBPE_INVALID_ADDRESS == nFOA) {
        return E_FAIL;
    }

    nSize = pDataDirectory->Size;

    return S_OK;
}

template <class T>
void
PEParserT<T>::InitPEElement(_Inout_ PEElementT<T> *pElement, _In_ PEAddress nRVA, _In_ PEAddress nFOA, _In_ PEAddress nSize)
{
    InitPEElement(pElement, nRVA, nSize, nFOA, nSize);
}

template <class T>
void
PEParserT<T>::InitPEElement(_Inout_ PEElementT<T> *pElement, _In_ PEAddress nRVA, _In_ PEAddress nMemSize, _In_ PEAddress nFOA, _In_ PEAddress nFileSize)
{
    pElement->InnerSetBase(m_pFile, this);
    pElement->InnerSetMemoryInfo(nRVA, LIBPE_INVALID_ADDRESS, nMemSize);
    pElement->InnerSetFileInfo(nFOA, nFileSize);
}

LIBPE_FORCE_TEMPLATE_REDUCTION_CLASS(PEParserT);
LIBPE_FORCE_TEMPLATE_REDUCTION_CLASS_FUNCTION(PEParserT, Create);

LIBPE_NAMESPACE_END