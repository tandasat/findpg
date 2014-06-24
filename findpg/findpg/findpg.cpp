//
// This module implements exported debugger extension commands
//
#include "stdafx.h"

// C/C++ standard headers
// Other external headers
// Windows headers
// Original headers
#include "pte.h"
#include "Progress.h"
#include "PoolTagDescription.h"


////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//


////////////////////////////////////////////////////////////////////////////////
//
// constants and macros
//


////////////////////////////////////////////////////////////////////////////////
//
// types
//

typedef struct _POOL_TRACKER_BIG_PAGES
{
    PVOID Va;
    ULONG Key;
    ULONG PoolType;
    SIZE_T Size;    // InBytes
} POOL_TRACKER_BIG_PAGES, *PPOOL_TRACKER_BIG_PAGES;
C_ASSERT(sizeof(POOL_TRACKER_BIG_PAGES) == 0x18);


struct RandomnessInfo
{
    ULONG NumberOfDistinctiveNumbers;
    ULONG Ramdomness;
};


//----------------------------------------------------------------------------
//
// Base extension class.
// Extensions derive from the provided ExtExtension class.
//
// The standard class name is "Extension".  It can be
// overridden by providing an alternate definition of
// EXT_CLASS before including engextcpp.hpp.
//
//----------------------------------------------------------------------------
class EXT_CLASS : public ExtExtension
{
public:
    virtual HRESULT Initialize();
    EXT_COMMAND_METHOD(findpg);

private:
    void findpgInternal();

    std::vector <std::tuple<POOL_TRACKER_BIG_PAGES, RandomnessInfo>>
        FindPgPagesFromNonPagedPool();

    std::vector<std::tuple<ULONG64, SIZE_T, RandomnessInfo>>
        FindPgPagesFromIndependentPages();

    std::array<HARDWARE_PTE, 512> GetPtes(
        __in ULONG64 PteBase);

    bool IsPatchGuardPageAttribute(
        __in ULONG64 PageBase);

    bool IsPageValidReadWriteExecutable(
        __in ULONG64 PteAddr);

    // The number of bytes to examine to calculate the number of distinctive
    // bytes and randomness
    static const auto EXAMINATION_BYTES = 100;

    // It is not a PatchGuard page if the number of distinctive bytes are bigger
    // than this number
    static const auto MAXIMUM_DISTINCTIVE_NUMBER = 5;

    // It is not a PatchGuard page if randomness is smaller than this number
    static const auto MINIMUM_RANDOMNESS = 50;

    // It is not a PatchGuard page if the size of the page is smaller than this
    static const auto MINIMUM_REGION_SIZE = 0x004000;

    // It is not a PatchGuard page if the size of the page is larger than this
    static const auto MAXIMUM_REGION_SIZE = 0xf00000;
};


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//

namespace {

ULONG GetNumberOfDistinctiveNumbers(
    void* Addr,
    SIZE_T Size);

ULONG GetRamdomness(
    void* Addr,
    SIZE_T Size);

} // End of namespace {unnamed}


////////////////////////////////////////////////////////////////////////////////
//
// variables
//

// EXT_DECLARE_GLOBALS must be used to instantiate
// the framework's assumed globals.
EXT_DECLARE_GLOBALS();


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

HRESULT EXT_CLASS::Initialize()
{
    // Initialize ExtensionApis to use dprintf and so on
    // when this extension is loaded.
    PDEBUG_CLIENT debugClient = nullptr;
    auto result = DebugCreate(__uuidof(IDebugClient),
        reinterpret_cast<void**>(&debugClient));
    if (!SUCCEEDED(result))
    {
        return result;
    }
    auto debugClientScope = std::experimental::scope_guard(
        [debugClient]() { debugClient->Release(); });

    PDEBUG_CONTROL debugControl = nullptr;
    result = debugClient->QueryInterface(__uuidof(IDebugControl),
        reinterpret_cast<void**>(&debugControl));
    if (!SUCCEEDED(result))
    {
        return result;
    }
    auto debugControlScope = std::experimental::scope_guard(
        [debugControl]() { debugControl->Release(); });

    ExtensionApis.nSize = sizeof(ExtensionApis);
    result = debugControl->GetWindbgExtensionApis64(&ExtensionApis);
    if (!SUCCEEDED(result))
    {
        return result;
    }

    // Display guide messages
    dprintf("Use ");
    debugControl->ControlledOutput(
        DEBUG_OUTCTL_AMBIENT_DML, DEBUG_OUTPUT_NORMAL,
        "<exec cmd=\"!findpg\">!findpg</exec>");
    dprintf(" to find base addresses of the pages allocated for PatchGuard.\n");
    return result;
}


// Exported command !findpg (no options)
EXT_COMMAND(findpg,
    "Displays base addresses of PatchGuard pages",
    "")
{
    try
    {
        findpgInternal();
    }
    catch (std::exception& e)
    {
        // As an exception string does not appear on Windbg,
        // we need to handle it manually.
        Err("%s\n", e.what());
    }
}


// Does main stuff and throws an exception when
void EXT_CLASS::findpgInternal()
{
    Out("Wait until analysis is completed. It typically takes 2-5 minutes.\n");
    Out("Or press Ctrl+Break or [Debug] > [Break] to stop analysis.\n");

    // Collect PatchGuard pages from NonPagedPool and independent pages
    auto foundNonPaged = FindPgPagesFromNonPagedPool();
    Out("Phase 1 analysis has been done.\n");
    auto foundIndependent = FindPgPagesFromIndependentPages();
    Out("Phase 2 analysis has been done.\n");

    // Sort data according to its base addresses
    std::sort(foundNonPaged.begin(), foundNonPaged.end(), [](
        const decltype(foundNonPaged)::value_type& Lhs,
        const decltype(foundNonPaged)::value_type& Rhs)
    {
        return std::get<0>(Lhs).Va < std::get<0>(Rhs).Va;
    });
    std::sort(foundIndependent.begin(), foundIndependent.end(), [](
        const decltype(foundIndependent)::value_type& Lhs,
        const decltype(foundIndependent)::value_type& Rhs)
    {
        return std::get<0>(Lhs) < std::get<0>(Rhs);
    });

    // Display collected data
    PoolTagDescription pooltag(this);
    for (const auto& n : foundNonPaged)
    {
        const auto description = pooltag.get(std::get<0>(n).Key);
        Out("[BigPagePool] PatchGuard context page base: %y, size: 0x%08x,"
            " Randomness %3d:%3d,%s\n",
            std::get<0>(n).Va, std::get<0>(n).Size,
            std::get<1>(n).NumberOfDistinctiveNumbers,
            std::get<1>(n).Ramdomness,
            description.c_str());
    }
    for (const auto& n : foundIndependent)
    {
        Out("[Independent] PatchGuard context page base: %y, Size: 0x%08x,"
            " Randomness %3d:%3d,\n",
            std::get<0>(n), std::get<1>(n),
            std::get<2>(n).NumberOfDistinctiveNumbers,
            std::get<2>(n).Ramdomness);
    }
}

// Collects PatchGuard pages reside in NonPagedPool
std::vector <std::tuple<POOL_TRACKER_BIG_PAGES, RandomnessInfo>>
EXT_CLASS::FindPgPagesFromNonPagedPool()
{
    ULONG64 offset = 0;

    // Read MmNonPagedPoolStart if it is possible. On Windows 8.1, this symbol
    // has been removed and this magic value is used instead.
    ULONG64 mmNonPagedPoolStart = 0xFFFFE00000000000;
    auto result = m_Symbols->GetOffsetByName("nt!MmNonPagedPoolStart", &offset);
    if (SUCCEEDED(result))
    {
        result = m_Data->ReadPointersVirtual(1, offset, &mmNonPagedPoolStart);
        if (!SUCCEEDED(result))
        {
            throw std::runtime_error("nt!MmNonPagedPoolStart could not be read.");
        }
    }

    // Read PoolBigPageTableSize
    result = m_Symbols->GetOffsetByName("nt!PoolBigPageTableSize", &offset);
    if (!SUCCEEDED(result))
    {
        throw std::runtime_error("nt!PoolBigPageTableSize could not be found.");
    }
    SIZE_T poolBigPageTableSize = 0;
    result = m_Data->ReadPointersVirtual(1, offset, &poolBigPageTableSize);
    if (!SUCCEEDED(result))
    {
        throw std::runtime_error("nt!PoolBigPageTableSize could not be read.");
    }

    // Read PoolBigPageTable
    result = m_Symbols->GetOffsetByName("nt!PoolBigPageTable", &offset);
    if (!SUCCEEDED(result))
    {
        throw std::runtime_error("nt!PoolBigPageTable could not be found.");
    }
    ULONG64 poolBigPageTable = 0;
    result = m_Data->ReadPointersVirtual(1, offset, &poolBigPageTable);
    if (!SUCCEEDED(result))
    {
        throw std::runtime_error("nt!PoolBigPageTable could not be read.");
    }

    // Read actual PoolBigPageTable contents
    ULONG readBytes = 0;
    std::vector<POOL_TRACKER_BIG_PAGES> table(poolBigPageTableSize);
    result = m_Data->ReadVirtual(poolBigPageTable, table.data(),
        static_cast<ULONG>(table.size() * sizeof(POOL_TRACKER_BIG_PAGES)),
        &readBytes);
    if (!SUCCEEDED(result))
    {
        throw std::runtime_error("nt!PoolBigPageTable could not be read.");
    }

    // Walk BigPageTable
    Progress progress(this);
    std::vector<std::tuple<POOL_TRACKER_BIG_PAGES, RandomnessInfo>> found;
    for (SIZE_T i = 0; i < poolBigPageTableSize; ++i)
    {
        if ((i % 0x1000) == 0)
        {
            ++progress;
        }

        const auto& entry = table[i];
        auto startAddr = reinterpret_cast<ULONG_PTR>(entry.Va);

        // Ignore unused entries
        if (!startAddr || (startAddr & 1))
        {
            continue;
        }

        // Filter by the size of region
        if (MINIMUM_REGION_SIZE > entry.Size || entry.Size > MAXIMUM_REGION_SIZE)
        {
            continue;
        }

        // Filter by the address
        if (startAddr < mmNonPagedPoolStart)
        {
            // This assertion seem reasonable but not always be true.
            //assert(entry.PoolType & 1 /*PagedPool*/);
            continue;
        }

        // Filter by the page protection
        if (!IsPatchGuardPageAttribute(startAddr))
        {
            continue;
        }

        // Read and check randomness of the contents
        std::array<std::uint8_t, EXAMINATION_BYTES> contents;
        result = m_Data->ReadVirtual(startAddr,
            contents.data(), static_cast<ULONG>(contents.size()), &readBytes);
        if (!SUCCEEDED(result))
        {
            continue;
        }
        const auto numberOfDistinctiveNumbers = GetNumberOfDistinctiveNumbers(
            contents.data(), EXAMINATION_BYTES);
        const auto randomness = GetRamdomness(
            contents.data(), EXAMINATION_BYTES);
        if (numberOfDistinctiveNumbers > MAXIMUM_DISTINCTIVE_NUMBER ||
            randomness < MINIMUM_RANDOMNESS)
        {
            continue;
        }

        // It seems to be a PatchGuard page
        found.emplace_back(entry,
            RandomnessInfo{ numberOfDistinctiveNumbers, randomness,});
    }
    return found;
}


// Collects PatchGuard pages reside in independent pages
std::vector<std::tuple<ULONG64, SIZE_T, RandomnessInfo>>
EXT_CLASS::FindPgPagesFromIndependentPages()
{
    ULONG64 offset = 0;

    // MmSystemRangeStart
    auto result = m_Symbols->GetOffsetByName("nt!MmSystemRangeStart", &offset);
    if (!SUCCEEDED(result))
    {
        throw std::runtime_error("nt!MmSystemRangeStart could not be found.");
    }
    ULONG64 mmSystemRangeStart = 0;
    result = m_Data->ReadPointersVirtual(1, offset, &mmSystemRangeStart);
    if (!SUCCEEDED(result))
    {
        throw std::runtime_error("nt!MmSystemRangeStart could not be read.");
    }

    std::vector<std::tuple<ULONG64, SIZE_T, RandomnessInfo>> found;

    Progress progress(this);

    // Walk entire page table (PXE -> PPE -> PDE -> PTE)
    // Start parse PXE (PML4) which represents the beginning of kernel address
    const auto startPxe = reinterpret_cast<ULONG64>(
        MiAddressToPxe(reinterpret_cast<void*>(mmSystemRangeStart)));
    const auto endPxe = PXE_TOP;
    const auto pxes = GetPtes(PXE_BASE);
    for (auto currentPxe = startPxe; currentPxe < endPxe;
        currentPxe += sizeof(HARDWARE_PTE))
    {
        // Make sure that this PXE is valid
        const auto pxeIndex = (currentPxe - PXE_BASE) / sizeof(HARDWARE_PTE);
        const auto pxe = pxes[pxeIndex];
        if (!pxe.Valid)
        {
            continue;
        }

        // If the PXE is valid, analyze PPE belonging to this
        const auto startPpe = PPE_BASE + 0x1000 * pxeIndex;
        const auto endPpe   = PPE_BASE + 0x1000 * (pxeIndex + 1);
        const auto ppes = GetPtes(startPpe);
        for (auto currentPpe = startPpe; currentPpe < endPpe;
            currentPpe += sizeof(HARDWARE_PTE))
        {
            // Make sure that this PPE is valid
            const auto ppeIndex1 = (currentPpe - PPE_BASE) / sizeof(HARDWARE_PTE);
            const auto ppeIndex2 = (currentPpe - startPpe) / sizeof(HARDWARE_PTE);
            const auto ppe = ppes[ppeIndex2];
            if (!ppe.Valid)
            {
                continue;
            }

            // If the PPE is valid, analyze PDE belonging to this
            const auto startPde = PDE_BASE + 0x1000 * ppeIndex1;
            const auto endPde   = PDE_BASE + 0x1000 * (ppeIndex1 + 1);
            const auto pdes = GetPtes(startPde);
            for (auto currentPde = startPde; currentPde < endPde;
                currentPde += sizeof(HARDWARE_PTE))
            {
                // Make sure that this PDE is valid as well as is not handling
                // a large page as an independent page does not use a large page
                const auto pdeIndex1 = (currentPde - PDE_BASE) / sizeof(HARDWARE_PTE);
                const auto pdeIndex2 = (currentPde - startPde) / sizeof(HARDWARE_PTE);
                const auto pde = pdes[pdeIndex2];
                if (!pde.Valid || pde.LargePage)
                {
                    continue;
                }
                ++progress;

                // If the PDE is valid, analyze PTE belonging to this
                const auto startPte = PTE_BASE + 0x1000 * pdeIndex1;
                const auto endPte   = PTE_BASE + 0x1000 * (pdeIndex1 + 1);
                const auto ptes = GetPtes(startPte);
                for (auto currentPte = startPte; currentPte < endPte;
                    currentPte += sizeof(HARDWARE_PTE))
                {
                    // Make sure that this PPE is valid,
                    // Readable/Writable/Executable
                    const auto pteIndex2 = (currentPte - startPte)
                        / sizeof(HARDWARE_PTE);
                    const auto pte = ptes[pteIndex2];
                    if (!pte.Valid ||
                        !pte.Write ||
                        pte.NoExecute)
                    {
                        continue;
                    }

                    // This page might be PatchGuard page, so let's analyze it
                    const auto virtualAddress = reinterpret_cast<ULONG64>(
                        MiPteToAddress(
                            reinterpret_cast<HARDWARE_PTE*>(currentPte)))
                                | 0xffff000000000000;

                    // Read the contents of the address that is managed by the
                    // PTE
                    ULONG readBytes = 0;
                    std::array<std::uint8_t, EXAMINATION_BYTES + sizeof(ULONG64)>
                        contents;
                    result = m_Data->ReadVirtual(virtualAddress, contents.data(),
                        static_cast<ULONG>(contents.size()), &readBytes);
                    if (!SUCCEEDED(result))
                    {
                        continue;
                    }

                    // Check randomness of the contents
                    const auto numberOfDistinctiveNumbers =
                        GetNumberOfDistinctiveNumbers(
                            contents.data() + sizeof(ULONG64), EXAMINATION_BYTES);
                    const auto randomness = GetRamdomness(
                        contents.data() + sizeof(ULONG64), EXAMINATION_BYTES);
                    if (numberOfDistinctiveNumbers > MAXIMUM_DISTINCTIVE_NUMBER
                        || randomness < MINIMUM_RANDOMNESS)
                    {
                        continue;
                    }

                    // Also, check the size of the region. The first page of
                    // allocated pages as independent pages has its own page
                    // size in bytes at the first 8 bytes
                    const auto independentPageSize =
                        *reinterpret_cast<ULONG64*>(contents.data());
                    if (MINIMUM_REGION_SIZE > independentPageSize
                     || independentPageSize > MAXIMUM_REGION_SIZE)
                    {
                        continue;
                    }

                    // It seems to be a PatchGuard page
                    found.emplace_back(virtualAddress, independentPageSize,
                        RandomnessInfo{ numberOfDistinctiveNumbers, randomness, });
                }
            }
        }
    }
    return found;
}


// Returns PTEs in one page
std::array<HARDWARE_PTE, 512> EXT_CLASS::GetPtes(
    __in ULONG64 PteBase)
{
    ULONG readBytes = 0;
    std::array<HARDWARE_PTE, 512> ptes;
    auto result = m_Data->ReadVirtual(PteBase, ptes.data(),
        static_cast<ULONG>(ptes.size() * sizeof(HARDWARE_PTE)), &readBytes);
    if (!SUCCEEDED(result))
    {
        throw std::runtime_error("The given address could not be read.");
    }
    return ptes;
}


// Returns true when page protection of the given page or a parant page
// of the given page is Valid and Readable/Writable/Executable.
bool EXT_CLASS::IsPatchGuardPageAttribute(
    __in ULONG64 PageBase)
{
    const auto pteAddr = MiAddressToPte(reinterpret_cast<void*>(PageBase));
    if (IsPageValidReadWriteExecutable(reinterpret_cast<ULONG64>(pteAddr)))
    {
        return true;
    }
    const auto pdeAddr = MiAddressToPde(reinterpret_cast<void*>(PageBase));
    if (IsPageValidReadWriteExecutable(reinterpret_cast<ULONG64>(pdeAddr)))
    {
        return true;
    }
    return false;
}


// Returns true when page protection of the given page is
// Readable/Writable/Executable.
bool EXT_CLASS::IsPageValidReadWriteExecutable(
    __in ULONG64 PteAddr)
{
    ULONG readBytes = 0;
    HARDWARE_PTE pte = {};
    auto result = m_Data->ReadVirtual(PteAddr,
        &pte, sizeof(pte), &readBytes);
    if (!SUCCEEDED(result))
    {
        return false;
    }
    return pte.Valid
        && pte.Write
        && !pte.NoExecute;
}


namespace {


// Returns the number of 0x00 and 0xff in the given range
ULONG GetNumberOfDistinctiveNumbers(
    __in void* Addr,
    __in SIZE_T Size)
{
    const auto p = static_cast<UCHAR*>(Addr);
    ULONG count = 0;
    for (SIZE_T i = 0; i < Size; ++i)
    {
        if (p[i] == 0xff || p[i] == 0x00)
        {
            count++;
        }
    }
    return count;
}


// Returns the number of unique bytes in the given range.
// For example, it returns 3 for the following bytes
// 00 01 01 02 02 00 02
ULONG GetRamdomness(
    __in void* Addr,
    __in SIZE_T Size)
{
    const auto p = static_cast<UCHAR*>(Addr);
    std::set<UCHAR> dic;
    for (SIZE_T i = 0; i < Size; ++i)
    {
        dic.insert(p[i]);
    }
    return static_cast<ULONG>(dic.size());
}


} // End of namespace {unnamed}
