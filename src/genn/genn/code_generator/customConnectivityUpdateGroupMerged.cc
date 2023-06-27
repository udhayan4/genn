#include "code_generator/customConnectivityUpdateGroupMerged.h"

// Standard C++ includes
#include <list>

// GeNN code generator includes
#include "code_generator/modelSpecMerged.h"

// GeNN transpiler includes
#include "transpiler/errorHandler.h"

using namespace GeNN;
using namespace GeNN::CodeGenerator;

//----------------------------------------------------------------------------
// CodeGenerator::CustomConnectivityUpdateGroupMergedBase
//----------------------------------------------------------------------------
CustomConnectivityUpdateGroupMergedBase::CustomConnectivityUpdateGroupMergedBase(size_t index, const Type::TypeContext &typeContext,
                                                                                 const std::vector<std::reference_wrapper<const CustomConnectivityUpdateInternal>> &groups)
:   GroupMerged<CustomConnectivityUpdateInternal>(index, typeContext, groups)
{
    using namespace Type;

    addField(Uint32, "numSrcNeurons",
             [](const auto &cg, size_t) 
             {
                 const SynapseGroupInternal *sgInternal = static_cast<const SynapseGroupInternal*>(cg.getSynapseGroup());
                 return std::to_string(sgInternal->getSrcNeuronGroup()->getNumNeurons()); 
             });

    addField(Uint32, "numTrgNeurons",
             [](const auto &cg, size_t)
             { 
                 const SynapseGroupInternal *sgInternal = static_cast<const SynapseGroupInternal*>(cg.getSynapseGroup());
                 return std::to_string(sgInternal->getTrgNeuronGroup()->getNumNeurons()); 
             });

    // Add heterogeneous custom update model parameters
    addHeterogeneousParams<CustomConnectivityUpdateGroupMergedBase>(
        getArchetype().getCustomConnectivityUpdateModel()->getParamNames(), "",
        [](const auto &cg) { return cg.getParams(); },
        &CustomConnectivityUpdateGroupMergedBase::isParamHeterogeneous);

    // Add heterogeneous weight update model CustomConnectivityUpdateGroupMerged parameters
    addHeterogeneousDerivedParams<CustomConnectivityUpdateGroupMergedBase>(
        getArchetype().getCustomConnectivityUpdateModel()->getDerivedParams(), "",
        [](const auto &cg) { return cg.getDerivedParams(); },
        &CustomConnectivityUpdateGroupMergedBase::isDerivedParamHeterogeneous);
}
//----------------------------------------------------------------------------
bool CustomConnectivityUpdateGroupMergedBase::isParamHeterogeneous(const std::string &name) const
{
    return isParamValueHeterogeneous(name, [](const CustomConnectivityUpdateInternal &cg) { return cg.getParams(); });
}
//----------------------------------------------------------------------------
bool CustomConnectivityUpdateGroupMergedBase::isDerivedParamHeterogeneous(const std::string &name) const
{
    return isParamValueHeterogeneous(name, [](const CustomConnectivityUpdateInternal &cg) { return cg.getDerivedParams(); });
}

//----------------------------------------------------------------------------
// CodeGenerator::CustomConnectivityUpdateGroupMerged
//----------------------------------------------------------------------------
const std::string CustomConnectivityUpdateGroupMerged::name = "CustomConnectivityUpdate";
//----------------------------------------------------------------------------
CustomConnectivityUpdateGroupMerged::CustomConnectivityUpdateGroupMerged(size_t index, const Type::TypeContext &typeContext, const BackendBase &backend,
                                                                         const std::vector<std::reference_wrapper<const CustomConnectivityUpdateInternal>> &groups)
:   CustomConnectivityUpdateGroupMergedBase(index, typeContext, groups)
{
    using namespace Type;

    // Reserve vector of vectors to hold variables to update for all custom connectivity update groups, in archetype order
    m_SortedDependentVars.reserve(getGroups().size());

    // Loop through groups
    for(const auto &g : getGroups()) {
        // Get group's dependent variables
        const auto dependentVars = g.get().getDependentVariables();
        
        // Convert to list and sort
        // **NOTE** WUVarReferences are non-assignable so can't be sorted in a vector
        std::list<Models::WUVarReference> dependentVarsList(dependentVars.cbegin(), dependentVars.cend());
        dependentVarsList.sort([](const auto &a, const auto &b)
                               {  
                                   boost::uuids::detail::sha1 hashA;  
                                   Type::updateHash(a.getVar().type, hashA);
                                   Utils::updateHash(getVarAccessDuplication(a.getVar().access), hashA);

                                   boost::uuids::detail::sha1 hashB;
                                   Type::updateHash(b.getVar().type, hashB);
                                   Utils::updateHash(getVarAccessDuplication(b.getVar().access), hashB);

                                   return (hashA.get_digest() < hashB.get_digest());
                                });

        // Add vector for this groups update vars
        m_SortedDependentVars.emplace_back(dependentVarsList.cbegin(), dependentVarsList.cend());
    }
    
    // Check all vectors are the same size
    assert(std::all_of(m_SortedDependentVars.cbegin(), m_SortedDependentVars.cend(),
                       [this](const std::vector<Models::WUVarReference> &vars)
                       {
                           return (vars.size() == m_SortedDependentVars.front().size());
                       }));
    
    
    /*addField(Uint32, "rowStride",
            [&backend](const auto &cg, size_t) 
            { 
                const SynapseGroupInternal *sgInternal = static_cast<const SynapseGroupInternal*>(cg.getSynapseGroup());
                return std::to_string(backend.getSynapticMatrixRowStride(*sgInternal)); 
            });
    
    
    assert(getArchetype().getSynapseGroup()->getMatrixType() & SynapseMatrixConnectivity::SPARSE);
    addField(getArchetype().getSynapseGroup()->getSparseIndType().createPointer(), "ind", 
             [&backend](const auto &cg, size_t) 
             { 
                 return backend.getDeviceVarPrefix() + "ind" + cg.getSynapseGroup()->getName(); 
             });

    addField(Uint32.createPointer(), "rowLength",
             [&backend](const auto &cg, size_t) 
             { 
                 return backend.getDeviceVarPrefix() + "rowLength" + cg.getSynapseGroup()->getName(); 
             });

    // If some presynaptic variables are delayed, add delay pointer
    if (getArchetype().getPreDelayNeuronGroup() != nullptr) {
        addField(Uint32.createPointer(), "preSpkQuePtr", 
                 [&backend](const auto &cg, size_t) 
                 { 
                     return backend.getScalarAddressPrefix() + "spkQuePtr" + cg.getPreDelayNeuronGroup()->getName(); 
                 });
    }

    // If some postsynaptic variables are delayed, add delay pointer
    if (getArchetype().getPostDelayNeuronGroup() != nullptr) {
        addField(Uint32.createPointer(), "postSpkQuePtr", 
                 [&backend](const auto &cg, size_t) 
                 { 
                     return backend.getScalarAddressPrefix() + "spkQuePtr" + cg.getPostDelayNeuronGroup()->getName(); 
                 });
    }
    
    // If this backend requires per-population RNGs and this group requires one
    if(backend.isPopulationRNGRequired() && getArchetype().isRowSimRNGRequired()){
        addPointerField(*backend.getMergedGroupSimRNGType(), "rng", backend.getDeviceVarPrefix() + "rowRNG");
    }

    // Add variables to struct
    const auto *cm = getArchetype().getCustomConnectivityUpdateModel();
    addVars(cm->getVars(), backend.getDeviceVarPrefix());
    addVars(cm->getPreVars(), backend.getDeviceVarPrefix());
    addVars(cm->getPostVars(), backend.getDeviceVarPrefix());

    // Add variable references to struct
    addVarReferences(cm->getVarRefs(), backend.getDeviceVarPrefix(),
                     [](const auto &cg) { return cg.getVarReferences(); });
    addVarReferences(cm->getPreVarRefs(), backend.getDeviceVarPrefix(),
                     [](const auto &cg) { return cg.getPreVarReferences(); });
    addVarReferences(cm->getPostVarRefs(), backend.getDeviceVarPrefix(),
                     [](const auto &cg) { return cg.getPostVarReferences(); });

    // Add EGPs to struct
    this->addEGPs(cm->getExtraGlobalParams(), backend.getDeviceVarPrefix());

    
    // Loop through sorted dependent variables
    for(size_t i = 0; i < getSortedArchetypeDependentVars().size(); i++) {
        auto resolvedType = getSortedArchetypeDependentVars().at(i).getVar().type.resolve(getTypeContext());
        addField(resolvedType.createPointer(), "_dependentVar" + std::to_string(i), 
                 [i, &backend, this](const auto&, size_t g) 
                 { 
                     const auto &varRef = m_SortedDependentVars[g][i];
                     return backend.getDeviceVarPrefix() + varRef.getVar().name + varRef.getTargetName(); 
                 });
    }*/
}
//----------------------------------------------------------------------------
boost::uuids::detail::sha1::digest_type CustomConnectivityUpdateGroupMerged::getHashDigest() const
{
    boost::uuids::detail::sha1 hash;

    // Update hash with archetype's hash digest
    Utils::updateHash(getArchetype().getHashDigest(), hash);

    // Update hash with sizes of pre and postsynaptic neuron groups
    updateHash([](const auto &cg) 
               {
                   return static_cast<const SynapseGroupInternal*>(cg.getSynapseGroup())->getSrcNeuronGroup()->getNumNeurons();
               }, hash);

    updateHash([](const auto &cg) 
               {
                   return static_cast<const SynapseGroupInternal*>(cg.getSynapseGroup())->getTrgNeuronGroup()->getNumNeurons();
               }, hash);

    // Update hash with each group's parameters, derived parameters and variable references
    updateHash([](const auto &cg) { return cg.getParams(); }, hash);
    updateHash([](const auto &cg) { return cg.getDerivedParams(); }, hash);
    updateHash([](const auto &cg) { return cg.getVarReferences(); }, hash);
    updateHash([](const auto &cg) { return cg.getPreVarReferences(); }, hash);
    updateHash([](const auto &cg) { return cg.getPostVarReferences(); }, hash);

    return hash.get_digest();
}
//----------------------------------------------------------------------------
void CustomConnectivityUpdateGroupMerged::generateUpdate(const BackendBase &backend, EnvironmentExternalBase &env, const ModelSpecMerged &modelMerged) const
{
    // Create new environment to add current source fields to neuron update group 
    EnvironmentGroupMergedField<CustomConnectivityUpdateGroupMerged> updateEnv(env, *this);

    // Add fields for number of pre and postsynaptic neurons
    updateEnv.addField(Type::Uint32.addConst(), "num_pre",
                       Type::Uint32, "numSrcNeurons", 
                       [](const auto &cg, size_t) 
                       { 
                           const SynapseGroupInternal *sgInternal = static_cast<const SynapseGroupInternal*>(cg.getSynapseGroup());
                           return std::to_string(sgInternal->getSrcNeuronGroup()->getNumNeurons());
                       });
    updateEnv.addField(Type::Uint32.addConst(), "num_post",
                       Type::Uint32, "numTrgNeurons", 
                       [](const auto &cg, size_t) 
                       { 
                           const SynapseGroupInternal *sgInternal = static_cast<const SynapseGroupInternal*>(cg.getSynapseGroup());
                           return std::to_string(sgInternal->getSrcNeuronGroup()->getNumNeurons());
                       });

    // Substitute parameter and derived parameter names
    const auto *cm = getArchetype().getCustomConnectivityUpdateModel();
    updateEnv.addParams(cm->getParamNames(), "", &CustomConnectivityUpdateInternal::getParams, 
                        &CustomConnectivityUpdateGroupMerged::isParamHeterogeneous);
    updateEnv.addDerivedParams(cm->getDerivedParams(), "", &CustomConnectivityUpdateInternal::getDerivedParams, 
                               &CustomConnectivityUpdateGroupMerged::isDerivedParamHeterogeneous);
    updateEnv.addExtraGlobalParams(cm->getExtraGlobalParams(), backend.getDeviceVarPrefix());
    
    // Add presynaptic variables and variable references
    // **TODO** var references to batched variables should be private
    // **THINK** what about batched pre var references?
    updateEnv.addVars<CustomConnectivityUpdatePreVarAdapter>(backend.getDeviceVarPrefix(), LazyString{updateEnv, "id_pre"}, "");
    updateEnv.addVarRefs<CustomConnectivityUpdatePreVarRefAdapter>(backend.getDeviceVarPrefix(),
                                                                   [&updateEnv](VarAccessMode, const Models::VarReference &v)
                                                                   { 
                                                                      if(v.getDelayNeuronGroup() != nullptr) {
                                                                          return LazyString::print("$(_pre_delay_offset) + $(id_pre)", updateEnv); 
                                                                      }
                                                                      else {
                                                                          return LazyString{updateEnv, "id_pre"}; 
                                                                      }
                                                                   }, "");

    // Calculate index of start of row
    updateEnv.add(Type::Uint32.addConst(), "_row_start_idx", "rowStartIdx",
                  {updateEnv.addInitialiser("const unsigned int rowStartIdx = $(id_pre) * $(_row_stride);", updateEnv)});

    updateEnv.add(Type::Uint32.addConst(), "_syn_stride", "synStride",
                  {updateEnv.addInitialiser("const unsigned int synStride = $(num_pre) * $(_row_stride);", updateEnv)});

    // Get variables which will need to be manipulated when adding and removing synapses
    const auto ccuVars = cm->getVars();
    const auto ccuVarRefs = cm->getVarRefs();
    const auto &dependentVars = getSortedArchetypeDependentVars();
    std::vector<Type::ResolvedType> addSynapseTypes{Type::Uint32};
    addSynapseTypes.reserve(1 + ccuVars.size() + ccuVarRefs.size() + dependentVars.size());

    // Generate code to add a synapse to this row
    std::stringstream addSynapseStream;
    CodeStream addSynapse(addSynapseStream);
    {
        CodeStream::Scope b(addSynapse);

        // Assert that there is space to add synapse
        backend.genAssert(addSynapse, "$(_row_length)[$(id_pre)] < $(_row_stride)");

        // Calculate index to insert synapse
        addSynapse << "const unsigned newIdx = $(_row_start_idx) + $(_row_length)[$(id_pre)];" << std::endl;

        // Set postsynaptic target to parameter 0
        addSynapse << "$(_ind)[newIdx] = $(0);" << std::endl;
 
        // Use subsequent parameters to initialise new synapse's custom connectivity update model variables
        for (size_t i = 0; i < ccuVars.size(); i++) {
            addSynapse << "group->" << ccuVars[i].name << "[newIdx] = $(" << (1 + i) << ");" << std::endl;
            addSynapseTypes.push_back(ccuVars[i].type.resolve(getTypeContext()));
        }

        // Use subsequent parameters to initialise new synapse's variables referenced via the custom connectivity update
        for (size_t i = 0; i < ccuVarRefs.size(); i++) {
            // If model is batched and this variable is duplicated
            if ((modelMerged.getModel().getBatchSize() > 1) && getArchetype().getVarReferences().at(ccuVarRefs[i].name).isDuplicated()) 
            {
                // Copy parameter into a register (just incase it's e.g. a RNG call) and copy into all batches
                addSynapse << "const " << ccuVarRefs[i].type.resolve(getTypeContext()).getName() << " _" << ccuVarRefs[i].name << "Val = $(" << (1 + ccuVars.size() + i) << ");" << std::endl;
                addSynapse << "for(int b = 0; b < " << modelMerged.getModel().getBatchSize() << "; b++)";
                {
                    CodeStream::Scope b(addSynapse);
                    addSynapse << "group->" << ccuVarRefs[i].name << "[(b * $(_syn_stride)) + newIdx] = _" << ccuVarRefs[i].name << "Val;" << std::endl;
                }
            }
            // Otherwise, write parameter straight into var reference
            else {
                addSynapse << "group->" << ccuVarRefs[i].name << "[newIdx] = $(" << (1 + ccuVars.size() + i) << ");" << std::endl;
            }

            addSynapseTypes.push_back(ccuVarRefs[i].type.resolve(getTypeContext()));
        }
        
        // Loop through any other dependent variables
        for (size_t i = 0; i < dependentVars.size(); i++) {
            // If model is batched and this dependent variable is duplicated
            if ((modelMerged.getModel().getBatchSize() > 1) && dependentVars.at(i).isDuplicated())
            {
                // Loop through all batches and zero
                addSynapse << "for(int b = 0; b < " << modelMerged.getModel().getBatchSize() << "; b++)";
                {
                    CodeStream::Scope b(addSynapse);
                    addSynapse << "group->_dependentVar" << i << "[(b * $(_syn_stride)) + newIdx] = 0;" << std::endl;
                }
            }
            // Otherwise, zero var reference
            else {
                addSynapse << "group->_dependentVar" << i << "[newIdx] = 0;" << std::endl;
            }

            addSynapseTypes.push_back(dependentVars.at(i).getVar().type.resolve(getTypeContext()));
        }

        // Increment row length
        // **NOTE** this will also effect any forEachSynapse loop currently in operation
        addSynapse << "$(_row_length)[$(id_pre)]++;" << std::endl;
    }

    // Add function substitution with parameters to initialise custom connectivity update variables and variable references
    updateEnv.add(Type::ResolvedType::createFunction(Type::Void, addSynapseTypes), "add_synapse", LazyString(addSynapseStream.str(), updateEnv));

    // Generate code to remove a synapse from this row
    std::stringstream removeSynapseStream;
    CodeStream removeSynapse(removeSynapseStream);
    {
        CodeStream::Scope b(removeSynapse);

        // Calculate index we want to copy synapse from
        removeSynapse << "const unsigned lastIdx = $(_row_start_idx) + $(_row_length)[$(id_pre)] - 1;" << std::endl;

        // Copy postsynaptic target from end of row over synapse to be deleted
        removeSynapse << "$(_ind)[idx] = $(_ind)[lastIdx];" << std::endl;

        // Copy custom connectivity update variables from end of row over synapse to be deleted
        for (size_t i = 0; i < ccuVars.size(); i++) {
            removeSynapse << "group->" << ccuVars[i].name << "[idx] = group->" << ccuVars[i].name << "[lastIdx];" << std::endl;
        }
        
        // Loop through variable references
        for (size_t i = 0; i < ccuVarRefs.size(); i++) {
            // If model is batched and this variable is duplicated
            if ((modelMerged.getModel().getBatchSize() > 1) 
                && getArchetype().getVarReferences().at(ccuVarRefs[i].name).isDuplicated())
            {
                // Loop through all batches and copy custom connectivity update variable references from end of row over synapse to be deleted
                removeSynapse << "for(int b = 0; b < " << modelMerged.getModel().getBatchSize() << "; b++)";
                {
                    CodeStream::Scope b(addSynapse);
                    removeSynapse << "group->" << ccuVarRefs[i].name << "[(b * $(_syn_stride)) + idx] = ";
                    removeSynapse << "group->" << ccuVarRefs[i].name << "[(b * $(_syn_stride)) + lastIdx];" << std::endl;
                }
            }
            // Otherwise, copy custom connectivity update variable references from end of row over synapse to be deleted
            else {
                removeSynapse << "group->" << ccuVarRefs[i].name << "[idx] = group->" << ccuVarRefs[i].name << "[lastIdx];" << std::endl;
            }
        }
        
        // Loop through any other dependent variables
        for (size_t i = 0; i < dependentVars.size(); i++) {
            // If model is batched and this dependent variable is duplicated
            if ((modelMerged.getModel().getBatchSize() > 1) && dependentVars.at(i).isDuplicated()) {
                // Loop through all batches and copy dependent variable from end of row over synapse to be deleted
                removeSynapse << "for(int b = 0; b < " << modelMerged.getModel().getBatchSize() << "; b++)";
                {
                    CodeStream::Scope b(removeSynapse);
                    removeSynapse << "group->_dependentVar" << i << "[(b * $(_syn_stride)) + idx] = ";
                    removeSynapse << "group->_dependentVar" << i << "[(b * $(_syn_stride)) + lastIdx];" << std::endl;
                }
            }
            // Otherwise, copy dependent variable from end of row over synapse to be deleted
            else {
                removeSynapse << "group->_dependentVar" << i << "[idx] = group->_dependentVar" << i << "[lastIdx];" << std::endl;
            }
        }

        // Decrement row length
        // **NOTE** this will also effect any forEachSynapse loop currently in operation
        removeSynapse << "$(_row_length)[$(id_pre)]--;" << std::endl;

        // Decrement loop counter so synapse j will get processed
        removeSynapse << "j--;" << std::endl;
    }

    // Add function substitution with parameters to initialise custom connectivity update variables and variable references
    updateEnv.add(Type::ResolvedType::createFunction(Type::Void, {}), "remove_synapse", LazyString{updateEnv, removeSynapseStream.str()});

    // Pretty print code back to environment
    Transpiler::ErrorHandler errorHandler("Current source injection" + std::to_string(getIndex()));
    prettyPrintStatements(cm->getRowUpdateCode(), getTypeContext(), updateEnv, errorHandler, 
                          // Within for_each_synapse loops, define the following types
                          [](auto &env, auto &errorHandler)
                          {
                              env.define(Transpiler::Token{Transpiler::Token::Type::IDENTIFIER, "id_post", 0}, Type::Uint32.addConst(), errorHandler);
                              env.define(Transpiler::Token{Transpiler::Token::Type::IDENTIFIER, "id_syn", 0}, Type::Uint32.addConst(), errorHandler);

                              // **TODO** variable types
                          },
                          [&backend, &modelMerged, this](auto &env, auto generateBody)
                          {
                              EnvironmentGroupMergedField<CustomConnectivityUpdateGroupMerged> bodyEnv(env, *this);
                              bodyEnv.getStream() << printSubs("for(int j = 0; j < $(_row_length)[$(id_pre)]; j++)", bodyEnv);
                              {
                                  CodeStream::Scope b(bodyEnv.getStream());

                                  // Add postsynaptic and synaptic indices
                                  bodyEnv.add(Type::Uint32.addConst(), "id_post", LazyString::print("$(_ind)[$(_row_start_idx) + j]", bodyEnv);
                                  bodyEnv.add(Type::Uint32.addConst(), "id_syn", "idx",
                                              {bodyEnv.addInitialiser("const unsigned int idx = $(_row_start_idx) + j;", bodyEnv)});

                                  // Add postsynaptic and synaptic variables
                                  bodyEnv.addVars<CustomConnectivityUpdateVarAdapter>(backend.getDeviceVarPrefix(), LazyString{bodyEnv, "id_syn"}, "");
                                  bodyEnv.addVars<CustomConnectivityUpdatePostVarAdapter>(backend.getDeviceVarPrefix(), LazyString{bodyEnv, "id_post"}, "");

                                  // Add postsynaptic and synaptic var references
                                  // **TODO**
                                  bodyEnv.addVarRefs<CustomConnectivityUpdatePostVarRefAdapter>(backend.getDeviceVarPrefix(),
                                                                                                [modelMerged, this](const std::string &ma, const Models::VarReference &v) 
                                                                                                {
                                                                                                    return (modelMerged.getModel().getBatchSize() == 1) || !v.isDuplicated(); 
                                                                                                });
                                  bodyEnv.addVarRefs<CustomConnectivityUpdateVarRefAdapter>(backend.getDeviceVarPrefix(),
                                                                                            [modelMerged, this](const std::string &ma, const Models::WUVarReference &v) 
                                                                                            {
                                                                                                return (modelMerged.getModel().getBatchSize() == 1) || !v.isDuplicated(); 
                                                                                            });
                                   // Substitute in variable references, filtering out those which are duplicated
                                   const auto &variableRefs = getArchetype().getVarReferences();
                                   updateSubs.addVarNameSubstitution(cm->getVarRefs(), "", "group->", 
                                                                     [&updateSubs](VarAccessMode, const std::string&) { return "[" + updateSubs["id_syn"] + "]"; },
                                                                     [modelBatched, &variableRefs](VarAccessMode, const std::string &name) 
                                                                     {
                                                                         return !modelBatched || !variableRefs.at(name).isDuplicated(); 
                                                                     });
                                    
                                    // Substitute in (potentially delayed) postsynaptic variable references
                                    const auto &postVariableRefs = getArchetype().getPostVarReferences();
                                    updateSubs.addVarNameSubstitution(cm->getPostVarRefs(), "", "group->",
                                                                      [&postVariableRefs, &updateSubs](VarAccessMode, const std::string &name)
                                                                      { 
                                                                          if(postVariableRefs.at(name).getDelayNeuronGroup() != nullptr) {
                                                                              return "[postDelayOffset + " + updateSubs["id_post"] + "]"; 
                                                                          }
                                                                          else {
                                                                              return "[" + updateSubs["id_post"] + "]"; 
                                                                          }
                                                                      });
                                  generateBody(bodyEnv);
                              }
                          });
}

//----------------------------------------------------------------------------
// CodeGenerator::CustomConnectivityHostUpdateGroupMerged
//----------------------------------------------------------------------------
const std::string CustomConnectivityHostUpdateGroupMerged::name = "CustomConnectivityHostUpdate";
//----------------------------------------------------------------------------
CustomConnectivityHostUpdateGroupMerged::CustomConnectivityHostUpdateGroupMerged(size_t index, const Type::TypeContext &typeContext, const BackendBase &backend,
                                                                                 const std::vector<std::reference_wrapper<const CustomConnectivityUpdateInternal>> &groups)
:   CustomConnectivityUpdateGroupMergedBase(index, typeContext, groups)
{
    using namespace Type;

    // Add pre and postsynaptic variables
    const auto *cm = getArchetype().getCustomConnectivityUpdateModel();
    addVars(backend, cm->getPreVars(), &CustomConnectivityUpdateInternal::getPreVarLocation);
    addVars(backend, cm->getPostVars(), &CustomConnectivityUpdateInternal::getPostVarLocation);

    // Add host extra global parameters
    for(const auto &e : cm->getExtraGlobalParams()) {
        const auto resolvedType = e.type.resolve(getTypeContext());
        addField(resolvedType.createPointer(), e.name,
                 [e](const auto &g, size_t) { return e.name + g.getName(); },
                 GroupMergedFieldType::HOST_DYNAMIC);

        if(!backend.getDeviceVarPrefix().empty()) {
            addField(resolvedType.createPointer(), backend.getDeviceVarPrefix() + e.name,
                     [e, &backend](const auto &g, size_t)
                     {
                         return backend.getDeviceVarPrefix() + e.name + g.getName();
                     },
                     GroupMergedFieldType::DYNAMIC);
        }
    }
             
}
//----------------------------------------------------------------------------
void CustomConnectivityHostUpdateGroupMerged::generateUpdate(const BackendBase &backend, CodeStream &os) const
{
    CodeStream::Scope b(os);
    os << "// merged custom connectivity host update group " << getIndex() << std::endl;
    os << "for(unsigned int g = 0; g < " << getGroups().size() << "; g++)";
    {
        CodeStream::Scope b(os);

        // Get reference to group
        os << "const auto *group = &mergedCustomConnectivityHostUpdateGroup" << getIndex() << "[g]; " << std::endl;

        // Create substitutions
        const auto *cm = getArchetype().getCustomConnectivityUpdateModel();
        Substitutions subs;
        subs.addVarSubstitution("rng", "hostRNG");
        subs.addVarSubstitution("num_pre", "group->numSrcNeurons");
        subs.addVarSubstitution("num_post", "group->numTrgNeurons");
        subs.addVarNameSubstitution(cm->getExtraGlobalParams(), "", "group->");
        subs.addVarNameSubstitution(cm->getPreVars(), "", "group->");
        subs.addVarNameSubstitution(cm->getPostVars(), "", "group->");
        subs.addParamValueSubstitution(cm->getParamNames(), getArchetype().getParams(),
                                       [this](const std::string &p) { return isParamHeterogeneous(p); },
                                       "", "group->");
        subs.addVarValueSubstitution(cm->getDerivedParams(), getArchetype().getDerivedParams(),
                                     [this](const std::string & p) { return isDerivedParamHeterogeneous(p); },
                                     "", "group->");

        // Loop through EGPs
        for(const auto &egp : cm->getExtraGlobalParams()) {
            const auto resolvedType = egp.type.resolve(getTypeContext());

            // Generate code to push this EGP with count specified by $(0)
            std::stringstream pushStream;
            CodeStream push(pushStream);
            backend.genVariableDynamicPush(push, resolvedType, egp.name,
                                           VarLocation::HOST_DEVICE, "$(0)", "group->");

            // Add substitution
            subs.addFuncSubstitution("push" + egp.name + "ToDevice", 1, pushStream.str());

            // Generate code to pull this EGP with count specified by $(0)
            std::stringstream pullStream;
            CodeStream pull(pullStream);
            backend.genVariableDynamicPull(pull, resolvedType, egp.name,
                                           VarLocation::HOST_DEVICE, "$(0)", "group->");

            // Add substitution
            subs.addFuncSubstitution("pull" + egp.name + "FromDevice", 1, pullStream.str());
        }

        addVarPushPullFuncSubs(backend, subs, cm->getPreVars(), "group->numSrcNeurons",
                               &CustomConnectivityUpdateInternal::getPreVarLocation);
        addVarPushPullFuncSubs(backend, subs, cm->getPostVars(), "group->numTrgNeurons",
                               &CustomConnectivityUpdateInternal::getPostVarLocation);

        // Apply substitutons to row update code and write out
        std::string code = cm->getHostUpdateCode();
        subs.applyCheckUnreplaced(code, "custom connectivity host update : merged" + std::to_string(getIndex()));
        //code = ensureFtype(code, modelMerged.getModel().getPrecision());
        os << code;
    }
}
//----------------------------------------------------------------------------
void CustomConnectivityHostUpdateGroupMerged::addVarPushPullFuncSubs(const BackendBase &backend, Substitutions &subs, 
                                                                     const Models::Base::VarVec &vars, const std::string &count,
                                                                     VarLocation(CustomConnectivityUpdateInternal:: *getVarLocationFn)(const std::string&) const) const
{
    // Loop through variables
    for(const auto &v : vars) {
        const auto resolvedType = v.type.resolve(getTypeContext());

        // If var is located on the host
        const auto loc = std::invoke(getVarLocationFn, getArchetype(), v.name);
        if (loc & VarLocation::HOST) {
            // Generate code to push this variable
            std::stringstream pushStream;
            CodeStream push(pushStream);
            backend.genVariableDynamicPush(push, resolvedType, v.name,
                                           loc, count, "group->");

            // Add substitution
            subs.addFuncSubstitution("push" + v.name + "ToDevice", 0, pushStream.str());

            // Generate code to pull this variable
            // **YUCK** these EGP functions should probably just be called dynamic or something
            std::stringstream pullStream;
            CodeStream pull(pullStream);
            backend.genVariableDynamicPull(pull, resolvedType, v.name,
                                           loc, count, "group->");

            // Add substitution
            subs.addFuncSubstitution("pull" + v.name + "FromDevice", 0, pullStream.str());
        }
    }
}
//----------------------------------------------------------------------------
void CustomConnectivityHostUpdateGroupMerged::addVars(const BackendBase &backend, const Models::Base::VarVec &vars,
                                                      VarLocation(CustomConnectivityUpdateInternal:: *getVarLocationFn)(const std::string&) const)
{
    using namespace Type;

    // Loop through variables
    for(const auto &v : vars) {
        // If var is located on the host
        const auto resolvedType = v.type.resolve(getTypeContext());
        if (std::invoke(getVarLocationFn, getArchetype(), v.name) & VarLocation::HOST) {
            addField(resolvedType.createPointer(), v.name,
                    [v](const auto &g, size_t) { return v.name + g.getName(); },
                    GroupMergedFieldType::HOST);

            if(!backend.getDeviceVarPrefix().empty())  {
                // **TODO** I think could use addPointerField
                addField(resolvedType.createPointer(), backend.getDeviceVarPrefix() + v.name,
                         [v, &backend](const auto &g, size_t)
                         {
                             return backend.getDeviceVarPrefix() + v.name + g.getName();
                         });
            }
        }
    }
}
