//
// This module implements a class responsible for retrieving pool tag
// description.
//
#include "stdafx.h"

// C/C++ standard headers
// Other external headers
// Windows headers
#include <strsafe.h>

// Original headers
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

PoolTagDescription::PoolTagDescription(
    __in ExtExtension* Ext)
    : m_GetPoolTagDescription(nullptr)
{
    Ext->m_Control->GetExtensionFunction(0,
        "GetPoolTagDescription",
        reinterpret_cast<FARPROC*>(&m_GetPoolTagDescription));
}


std::string PoolTagDescription::get(
    __in ULONG Key) const
{
    if (!m_GetPoolTagDescription)
    {
        return "";
    }

    DEBUG_POOLTAG_DESCRIPTION info = { sizeof(info) };
    auto result = m_GetPoolTagDescription(Key, &info);
    if (!SUCCEEDED(result))
    {
        return "";
    }

    char desc[400] = {};
    if (info.Description[0])
    {
        result = StringCchPrintfA(desc, _countof(desc),
            "  Pooltag %4.4s : %s", &Key, info.Description);
    }
    else
    {
        result = StringCchPrintfA(desc, _countof(desc),
            "  Pooltag %4.4s : Unknown", &Key);
    }

    char bin[100] = {};
    if (info.Binary[0])
    {
        result = StringCchPrintfA(bin, _countof(bin),
            ", Binary : %s", info.Binary);
    }

    char own[100] = {};
    if (info.Owner[0])
    {
        result = StringCchPrintfA(own, _countof(own),
            ", Owner : %s", info.Owner);
    }

    return std::string(desc) + bin + own;
}

