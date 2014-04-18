//
// This module implements a class responsible for displaying progress.
//
#pragma once

// C/C++ standard headers
#include <cstdint>

// Other external headers
// Windows headers
#include <engextcpp.hpp>


// Original headers


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

class Progress
{
public:
    Progress(
        __in ExtExtension* Ext)
        : m_Ext(Ext)
        , m_Progress(0)
    {
    }

    ~Progress()
    {
        m_Ext->Out("\n");
    }

    Progress& operator++()
    {
        if (m_Progress == 80)
        {
            m_Progress = 0;
            m_Ext->Out("\n");
        }
        ++m_Progress;
        m_Ext->Out(".");
        return *this;
    }

private:
    ExtExtension* m_Ext;
    std::uint64_t m_Progress;
};


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

