#pragma once

// GeNN includes
#include "customUpdate.h"

//------------------------------------------------------------------------
// CustomUpdateInternal
//------------------------------------------------------------------------
class CustomUpdateInternal : public CustomUpdate
{
public:
    CustomUpdateInternal(const std::string &name, const std::string &updateGroupName,
                         const CustomUpdateModels::Base *customUpdateModel, const std::vector<double> &params, 
                         const std::vector<Models::VarInit> &varInitialisers, const std::vector<VarReference> &varReferences, 
                         VarLocation defaultVarLocation, VarLocation defaultExtraGlobalParamLocation)
    :   CustomUpdate(name, updateGroupName, customUpdateModel, params, varInitialisers, varReferences, 
                     defaultVarLocation, defaultExtraGlobalParamLocation)
    {
    }

    using CustomUpdateBase::initDerivedParams;
    using CustomUpdateBase::getDerivedParams;
    using CustomUpdateBase::isInitRNGRequired;
    using CustomUpdateBase::isZeroCopyEnabled;
    using CustomUpdateBase::canBeMerged;
    using CustomUpdateBase::canInitBeMerged;
};

//------------------------------------------------------------------------
// CustomUpdateInternal
//------------------------------------------------------------------------
class CustomUpdateWUInternal : public CustomUpdateWU
{
public:
    CustomUpdateWUInternal(const std::string &name, const std::string &updateGroupName, Operation operation,
                           const CustomUpdateModels::Base *customUpdateModel, const std::vector<double> &params, 
                           const std::vector<Models::VarInit> &varInitialisers, const std::vector<WUVarReference> &varReferences, 
                           VarLocation defaultVarLocation, VarLocation defaultExtraGlobalParamLocation)
    :   CustomUpdateWU(name, updateGroupName, operation, customUpdateModel, params, varInitialisers, varReferences, 
                       defaultVarLocation, defaultExtraGlobalParamLocation)
    {
    }

    using CustomUpdateBase::initDerivedParams;
    using CustomUpdateBase::getDerivedParams;
    using CustomUpdateBase::isInitRNGRequired;
    using CustomUpdateBase::isZeroCopyEnabled;
    using CustomUpdateBase::canBeMerged;
    using CustomUpdateBase::canInitBeMerged;
    using CustomUpdateWU::getSynapseGroup;
};