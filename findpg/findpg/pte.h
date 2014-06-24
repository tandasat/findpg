//
// This module implements page table related definitions.
//
#pragma once

// C/C++ standard headers
// Other external headers
// Windows headers
#include <Windows.h>

// Original headers


////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//

static const auto PXE_BASE    = 0xFFFFF6FB7DBED000UI64;
static const auto PXE_SELFMAP = 0xFFFFF6FB7DBEDF68UI64;
static const auto PPE_BASE    = 0xFFFFF6FB7DA00000UI64;
static const auto PDE_BASE    = 0xFFFFF6FB40000000UI64;
static const auto PTE_BASE    = 0xFFFFF68000000000UI64;

static const auto PXE_TOP     = 0xFFFFF6FB7DBEDFFFUI64;
static const auto PPE_TOP     = 0xFFFFF6FB7DBFFFFFUI64;
static const auto PDE_TOP     = 0xFFFFF6FB7FFFFFFFUI64;
static const auto PTE_TOP     = 0xFFFFF6FFFFFFFFFFUI64;

static const auto PTI_SHIFT = 12;
static const auto PDI_SHIFT = 21;
static const auto PPI_SHIFT = 30;
static const auto PXI_SHIFT = 39;


////////////////////////////////////////////////////////////////////////////////
//
// constants and macros
//


////////////////////////////////////////////////////////////////////////////////
//
// types
//

static const auto HARDWARE_PTE_WORKING_SET_BITS = 11;
typedef struct _HARDWARE_PTE
{
    ULONG64 Valid : 1;
    ULONG64 Write : 1;                // UP version
    ULONG64 Owner : 1;
    ULONG64 WriteThrough : 1;
    ULONG64 CacheDisable : 1;
    ULONG64 Accessed : 1;
    ULONG64 Dirty : 1;
    ULONG64 LargePage : 1;
    ULONG64 Global : 1;
    ULONG64 CopyOnWrite : 1;          // software field
    ULONG64 Prototype : 1;            // software field
    ULONG64 reserved0 : 1;            // software field
    ULONG64 PageFrameNumber : 28;
    ULONG64 reserved1 : 24 - (HARDWARE_PTE_WORKING_SET_BITS + 1);
    ULONG64 SoftwareWsIndex : HARDWARE_PTE_WORKING_SET_BITS;
    ULONG64 NoExecute : 1;
} HARDWARE_PTE, *PHARDWARE_PTE;
C_ASSERT(sizeof(HARDWARE_PTE) == 8);


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

inline
void* MiPxeToAddress(
    __in PHARDWARE_PTE PointerPxe)
{
    return reinterpret_cast<void*>(
        (reinterpret_cast<LONG64>(PointerPxe) << 52) >> 16);
}


inline
void* MiPpeToAddress(
    __in PHARDWARE_PTE PointerPpe)
{
    return reinterpret_cast<void*>(
        (reinterpret_cast<LONG64>(PointerPpe) << 43) >> 16);
}


inline
void* MiPdeToAddress(
    __in PHARDWARE_PTE PointerPde)
{
    return reinterpret_cast<void*>(
        (reinterpret_cast<LONG64>(PointerPde) << 34) >> 16);
}


inline
void* MiPteToAddress(
    __in PHARDWARE_PTE PointerPte)
{
    return reinterpret_cast<void*>(
        (reinterpret_cast<LONG64>(PointerPte) << 25) >> 16);
}


inline
PHARDWARE_PTE MiAddressToPxe(
    __in void* Address)
{
    auto Offset = reinterpret_cast<ULONG64>(Address) >> (PXI_SHIFT - 3);
    Offset &= (0x1FF << 3);
    return reinterpret_cast<PHARDWARE_PTE>(PXE_BASE + Offset);
}


inline
PHARDWARE_PTE MiAddressToPpe(
    __in void* Address)
{
    auto Offset = reinterpret_cast<ULONG64>(Address) >> (PPI_SHIFT - 3);
    Offset &= (0x3FFFF << 3);
    return reinterpret_cast<PHARDWARE_PTE>(PPE_BASE + Offset);
}


inline
PHARDWARE_PTE MiAddressToPde(
    __in void* Address)
{
    auto Offset = reinterpret_cast<ULONG64>(Address) >> (PDI_SHIFT - 3);
    Offset &= (0x7FFFFFF << 3);
    return reinterpret_cast<PHARDWARE_PTE>(PDE_BASE + Offset);
}


inline
PHARDWARE_PTE MiAddressToPte(
    __in void* Address)
{
    auto Offset = reinterpret_cast<ULONG64>(Address) >> (PTI_SHIFT - 3);
    Offset &= (0xFFFFFFFFFULL << 3);
    return reinterpret_cast<PHARDWARE_PTE>(PTE_BASE + Offset);
}

