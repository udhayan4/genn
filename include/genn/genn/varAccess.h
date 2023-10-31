#pragma once

// Standard C++ includes
#include <variant>

// GeNN includes
#include "gennExport.h"
#include "gennUtils.h"

//----------------------------------------------------------------------------
// Enumerations
//----------------------------------------------------------------------------
namespace GeNN
{
//! Flags defining attributes of var access models
//! **NOTE** Read-only and read-write are seperate flags rather than read and write so you can test mode & VarAccessMode::READ_ONLY
enum class VarAccessModeAttribute : unsigned int
{
    READ_ONLY   = (1 << 0), //! This variable is read only
    READ_WRITE  = (1 << 1), //! This variable is read-write
    REDUCE      = (1 << 2), //! This variable is a reduction target
    SUM         = (1 << 3), //! This variable's reduction operation is a summation
    MAX         = (1 << 4), //! This variable's reduction operation is a maximum
};

//! Supported combination of VarAccessModeAttribute
enum class VarAccessMode : unsigned int
{
    READ_WRITE  = static_cast<unsigned int>(VarAccessModeAttribute::READ_WRITE),
    READ_ONLY   = static_cast<unsigned int>(VarAccessModeAttribute::READ_ONLY),
    REDUCE_SUM  = static_cast<unsigned int>(VarAccessModeAttribute::REDUCE) | static_cast<unsigned int>(VarAccessModeAttribute::SUM),
    REDUCE_MAX  = static_cast<unsigned int>(VarAccessModeAttribute::REDUCE) | static_cast<unsigned int>(VarAccessModeAttribute::MAX),
};

//! Flags defining dimensions this variables has
enum class VarAccessDim : unsigned int
{
    ELEMENT     = (1 << 5),
    BATCH       = (1 << 6),
};

//! Supported combinations of access mode and dimension for neuron and synapse variables
enum class VarAccess : unsigned int
{
    READ_WRITE              = static_cast<unsigned int>(VarAccessMode::READ_WRITE) | static_cast<unsigned int>(VarAccessDim::ELEMENT) | static_cast<unsigned int>(VarAccessDim::BATCH),
    READ_ONLY               = static_cast<unsigned int>(VarAccessMode::READ_ONLY) | static_cast<unsigned int>(VarAccessDim::ELEMENT),
    READ_ONLY_DUPLICATE     = static_cast<unsigned int>(VarAccessMode::READ_ONLY) | static_cast<unsigned int>(VarAccessDim::ELEMENT) | static_cast<unsigned int>(VarAccessDim::BATCH),
    READ_ONLY_SHARED_NEURON = static_cast<unsigned int>(VarAccessMode::READ_ONLY) | static_cast<unsigned int>(VarAccessDim::BATCH),
};

//! Supported combinations of access mode and dimension for custom update variables
/*! The axes are defined 'subtractively' ie VarAccessDim::BATCH indicates that this axis should be removed */
enum class CustomUpdateVarAccess : unsigned int
{
    // Variables with same shape as groups custom update is attached to
    READ_WRITE                  = static_cast<unsigned int>(VarAccessMode::READ_WRITE),
    READ_ONLY                   = static_cast<unsigned int>(VarAccessMode::READ_ONLY),

    // Variables which will be shared across batches if custom update is batched
    READ_ONLY_SHARED            = static_cast<unsigned int>(VarAccessMode::READ_ONLY) | static_cast<unsigned int>(VarAccessDim::BATCH),

    // Variables which will be shared across neurons if per-element
    READ_ONLY_SHARED_NEURON    = static_cast<unsigned int>(VarAccessMode::READ_ONLY) | static_cast<unsigned int>(VarAccessDim::ELEMENT),

    // Reduction variables
    REDUCE_BATCH_SUM            = static_cast<unsigned int>(VarAccessMode::REDUCE_SUM) | static_cast<unsigned int>(VarAccessDim::BATCH),
    REDUCE_BATCH_MAX            = static_cast<unsigned int>(VarAccessMode::REDUCE_MAX) | static_cast<unsigned int>(VarAccessDim::BATCH),
    REDUCE_NEURON_SUM          = static_cast<unsigned int>(VarAccessMode::REDUCE_SUM) | static_cast<unsigned int>(VarAccessDim::ELEMENT),
    REDUCE_NEURON_MAX          = static_cast<unsigned int>(VarAccessMode::REDUCE_MAX) | static_cast<unsigned int>(VarAccessDim::ELEMENT),
};

//----------------------------------------------------------------------------
// Operators
//----------------------------------------------------------------------------
inline bool operator & (VarAccessMode mode, VarAccessModeAttribute modeAttribute)
{
    return (static_cast<unsigned int>(mode) & static_cast<unsigned int>(modeAttribute)) != 0;
}

inline bool operator & (VarAccess mode, VarAccessModeAttribute modeAttribute)
{
    return (static_cast<unsigned int>(mode) & static_cast<unsigned int>(modeAttribute)) != 0;
}

inline bool operator & (CustomUpdateVarAccess mode, VarAccessModeAttribute modeAttribute)
{
    return (static_cast<unsigned int>(mode) & static_cast<unsigned int>(modeAttribute)) != 0;
}

inline bool operator & (VarAccessDim a, VarAccessDim b)
{
    return (static_cast<unsigned int>(a) & static_cast<unsigned int>(b)) != 0;
}

inline VarAccessDim operator | (VarAccessDim a, VarAccessDim b)
{
    return static_cast<VarAccessDim>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
}

//----------------------------------------------------------------------------
// Free functions
//----------------------------------------------------------------------------
inline VarAccessDim clearVarAccessDim(VarAccessDim a, VarAccessDim b)
{
    return static_cast<VarAccessDim>(static_cast<unsigned int>(a) & ~static_cast<unsigned int>(b));
}

inline VarAccessDim getVarAccessDim(VarAccess v)
{
    return static_cast<VarAccessDim>(static_cast<unsigned int>(v) & ~0x1F);
}

inline VarAccessDim getVarAccessDim(CustomUpdateVarAccess v, VarAccessDim popDims)
{
    return clearVarAccessDim(popDims, static_cast<VarAccessDim>(static_cast<unsigned int>(v) & ~0x1F));
}

inline VarAccessMode getVarAccessMode(VarAccessMode v)
{
    return v;
}

inline VarAccessMode getVarAccessMode(VarAccess v)
{
    return static_cast<VarAccessMode>(static_cast<unsigned int>(v) & 0x1F);
}

inline VarAccessMode getVarAccessMode(CustomUpdateVarAccess v)
{
    return static_cast<VarAccessMode>(static_cast<unsigned int>(v) & 0x1F);
}
}   // namespace GeNN
