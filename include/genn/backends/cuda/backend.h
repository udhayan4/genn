#pragma once

// Standard C++ includes
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <numeric>
#include <string>
#include <unordered_set>

// Standard C includes
#include <cassert>

// CUDA includes
#include <cuda.h>
#include <cuda_runtime.h>

// GeNN includes
#include "backendExport.h"

// GeNN code generator includes
#include "code_generator/backendBase.h"
#include "code_generator/codeStream.h"
#include "code_generator/substitutions.h"

// CUDA backend includes
#include "presynapticUpdateStrategy.h"

// Forward declarations
namespace filesystem
{
    class path;
}

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::DeviceSelectMethod
//--------------------------------------------------------------------------
namespace CodeGenerator
{
namespace CUDA
{
//! Methods for selecting CUDA device
enum class DeviceSelect
{
    OPTIMAL,        //!< Pick optimal device based on how well kernels can be simultaneously simulated and occupancy
    MOST_MEMORY,    //!< Pick device with most global memory
    MANUAL,         //!< Use device specified by user
};

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::BlockSizeSelect
//--------------------------------------------------------------------------
//! Methods for selecting CUDA kernel block size
enum class BlockSizeSelect
{
    OCCUPANCY,  //!< Pick optimal blocksize for each kernel based on occupancy
    MANUAL,     //!< Use block sizes specified by user
};

//--------------------------------------------------------------------------
// Kernel
//--------------------------------------------------------------------------
//! Kernels generated by CUDA backend
enum Kernel
{
    KernelNeuronUpdate,
    KernelPresynapticUpdate,
    KernelPostsynapticUpdate,
    KernelSynapseDynamicsUpdate,
    KernelInitialize,
    KernelInitializeSparse,
    KernelPreNeuronReset,
    KernelPreSynapseReset,
    KernelMax
};

//--------------------------------------------------------------------------
// Type definitions
//--------------------------------------------------------------------------
//! Array of block sizes for each kernel
using KernelBlockSize = std::array<size_t, KernelMax>;

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::Preferences
//--------------------------------------------------------------------------
//! Preferences for CUDA backend
struct Preferences : public PreferencesBase
{
    Preferences()
    {
        std::fill(manualBlockSizes.begin(), manualBlockSizes.end(), 32);
    }

    //! Should PTX assembler information be displayed for each CUDA kernel during compilation?
    bool showPtxInfo = false;

    //! Should line info be included in resultant executable for debugging/profiling purposes?
    bool generateLineInfo = false;

    //! GeNN normally identifies devices by PCI bus ID to ensure that the model is run on the same device
    //! it was optimized for. However if, for example, you are running on a cluser with NVML this is not desired behaviour.
    bool selectGPUByDeviceID = false;

    //! How to select GPU device
    DeviceSelect deviceSelectMethod = DeviceSelect::OPTIMAL;

    //! If device select method is set to DeviceSelect::MANUAL, id of device to use
    unsigned int manualDeviceID = 0;

    //! How to select CUDA blocksize
    BlockSizeSelect blockSizeSelectMethod = BlockSizeSelect::OCCUPANCY;

    //! If block size select method is set to BlockSizeSelect::MANUAL, block size to use for each kernel
    KernelBlockSize manualBlockSizes;

    //! NVCC compiler options for all GPU code
    std::string userNvccFlags = "";
};

//--------------------------------------------------------------------------
// CodeGenerator::CUDA::Backend
//--------------------------------------------------------------------------
class BACKEND_EXPORT Backend : public BackendBase
{
public:
    Backend(const KernelBlockSize &kernelBlockSizes, const Preferences &preferences,
            const std::string &scalarType, int device);

    //--------------------------------------------------------------------------
    // CodeGenerator::Backends:: virtuals
    //--------------------------------------------------------------------------
    virtual void genNeuronUpdate(CodeStream &os, const ModelSpecMerged &modelMerged, MemorySpaces &memorySpaces,
                                 HostHandler preambleHandler, NeuronGroupSimHandler simHandler, NeuronUpdateGroupMergedHandler wuVarUpdateHandler,
                                 HostHandler pushEGPHandler) const override;

    virtual void genSynapseUpdate(CodeStream &os, const ModelSpecMerged &modelMerged, MemorySpaces &memorySpaces,
                                  HostHandler preambleHandler, PresynapticUpdateGroupMergedHandler wumThreshHandler, PresynapticUpdateGroupMergedHandler wumSimHandler,
                                  PresynapticUpdateGroupMergedHandler wumEventHandler, PresynapticUpdateGroupMergedHandler wumProceduralConnectHandler,
                                  PostsynapticUpdateGroupMergedHandler postLearnHandler, SynapseDynamicsGroupMergedHandler synapseDynamicsHandler,
                                  HostHandler pushEGPHandler) const override;

    virtual void genInit(CodeStream &os, const ModelSpecMerged &modelMerged, MemorySpaces &memorySpaces,
                         HostHandler preambleHandler, NeuronInitGroupMergedHandler localNGHandler, SynapseDenseInitGroupMergedHandler sgDenseInitHandler,
                         SynapseConnectivityInitMergedGroupHandler sgSparseConnectHandler, SynapseSparseInitGroupMergedHandler sgSparseInitHandler,
                         HostHandler initPushEGPHandler, HostHandler initSparsePushEGPHandler) const override;

    //! Gets the stride used to access synaptic matrix rows, taking into account sparse data structure, padding etc
    virtual size_t getSynapticMatrixRowStride(const SynapseGroupInternal &sg) const override;

    virtual void genDefinitionsPreamble(CodeStream &os, const ModelSpecMerged &modelMerged) const override;
    virtual void genDefinitionsInternalPreamble(CodeStream &os, const ModelSpecMerged &modelMerged) const override;
    virtual void genRunnerPreamble(CodeStream &os, const ModelSpecMerged &modelMerged) const override;
    virtual void genAllocateMemPreamble(CodeStream &os, const ModelSpecMerged &modelMerged) const override;
    virtual void genStepTimeFinalisePreamble(CodeStream &os, const ModelSpecMerged &modelMerged) const override;

    virtual void genVariableDefinition(CodeStream &definitions, CodeStream &definitionsInternal, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual void genVariableImplementation(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual MemAlloc genVariableAllocation(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc, size_t count) const override;
    virtual void genVariableFree(CodeStream &os, const std::string &name, VarLocation loc) const override;

    virtual void genExtraGlobalParamDefinition(CodeStream &definitions, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual void genExtraGlobalParamImplementation(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual void genExtraGlobalParamAllocation(CodeStream &os, const std::string &type, const std::string &name, 
                                               VarLocation loc, const std::string &countVarName = "count", const std::string &prefix = "") const override;
    virtual void genExtraGlobalParamPush(CodeStream &os, const std::string &type, const std::string &name, 
                                         VarLocation loc, const std::string &countVarName = "count", const std::string &prefix = "") const override;
    virtual void genExtraGlobalParamPull(CodeStream &os, const std::string &type, const std::string &name, 
                                         VarLocation loc, const std::string &countVarName = "count", const std::string &prefix = "") const override;

    //! Generate code for pushing an updated EGP value into the merged group structure on 'device'
    virtual void genMergedExtraGlobalParamPush(CodeStream &os, const std::string &suffix, size_t mergedGroupIdx, 
                                               const std::string &groupIdx, const std::string &fieldName,
                                               const std::string &egpName) const override;

    //! When generating function calls to push to merged groups, backend without equivalent of Unified Virtual Addressing e.g. OpenCL 1.2 may use different types on host
    virtual std::string getMergedGroupFieldHostType(const std::string &type) const override;

    virtual void genPopVariableInit(CodeStream &os, const Substitutions &kernelSubs, Handler handler) const override;
    virtual void genVariableInit(CodeStream &os, const std::string &count, const std::string &indexVarName,
                                 const Substitutions &kernelSubs, Handler handler) const override;
    virtual void genSynapseVariableRowInit(CodeStream &os, const SynapseGroupMergedBase &sg,
                                           const Substitutions &kernelSubs, Handler handler) const override;

    virtual void genVariablePush(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc, bool autoInitialized, size_t count) const override;
    virtual void genVariablePull(CodeStream &os, const std::string &type, const std::string &name, VarLocation loc, size_t count) const override;

    virtual void genCurrentVariablePush(CodeStream &os, const NeuronGroupInternal &ng, const std::string &type, const std::string &name, VarLocation loc) const override;
    virtual void genCurrentVariablePull(CodeStream &os, const NeuronGroupInternal &ng, const std::string &type, const std::string &name, VarLocation loc) const override;

    virtual void genCurrentTrueSpikePush(CodeStream &os, const NeuronGroupInternal &ng) const override
    {
        genCurrentSpikePush(os, ng, false);
    }
    virtual void genCurrentTrueSpikePull(CodeStream &os, const NeuronGroupInternal &ng) const override
    {
        genCurrentSpikePull(os, ng, false);
    }
    virtual void genCurrentSpikeLikeEventPush(CodeStream &os, const NeuronGroupInternal &ng) const override
    {
        genCurrentSpikePush(os, ng, true);
    }
    virtual void genCurrentSpikeLikeEventPull(CodeStream &os, const NeuronGroupInternal &ng) const override
    {
        genCurrentSpikePull(os, ng, true);
    }
    
    virtual MemAlloc genGlobalDeviceRNG(CodeStream &definitions, CodeStream &definitionsInternal, CodeStream &runner, CodeStream &allocations, CodeStream &free) const override;
    virtual MemAlloc genPopulationRNG(CodeStream &definitions, CodeStream &definitionsInternal, CodeStream &runner,
                                      CodeStream &allocations, CodeStream &free, const std::string &name, size_t count) const override;
    virtual void genTimer(CodeStream &definitions, CodeStream &definitionsInternal, CodeStream &runner,
                          CodeStream &allocations, CodeStream &free, CodeStream &stepTimeFinalise,
                          const std::string &name, bool updateInStepTime) const override;

    //! Generate code to return amount of free 'device' memory in bytes
    virtual void genReturnFreeDeviceMemoryBytes(CodeStream &os) const override;

    virtual void genMakefilePreamble(std::ostream &os) const override;
    virtual void genMakefileLinkRule(std::ostream &os) const override;
    virtual void genMakefileCompileRule(std::ostream &os) const override;

    virtual void genMSBuildConfigProperties(std::ostream &os) const override;
    virtual void genMSBuildImportProps(std::ostream &os) const override;
    virtual void genMSBuildItemDefinitions(std::ostream &os) const override;
    virtual void genMSBuildCompileModule(const std::string &moduleName, std::ostream &os) const override;
    virtual void genMSBuildImportTarget(std::ostream &os) const override;

    virtual std::string getArrayPrefix() const override{ return m_Preferences.automaticCopy ? "" : "d_"; }
    virtual std::string getScalarPrefix() const override{ return "d_"; }

    virtual bool isGlobalHostRNGRequired(const ModelSpecMerged &modelMerged) const override;
    virtual bool isGlobalDeviceRNGRequired(const ModelSpecMerged &modelMerged) const override;
    virtual bool isPopulationRNGRequired() const override{ return true; }
    virtual bool isSynRemapRequired() const override{ return true; }
    virtual bool isPostsynapticRemapRequired() const override{ return true; }

    //! Is automatic copy mode enabled in the preferences?
    virtual bool isAutomaticCopyEnabled() const override { return m_Preferences.automaticCopy; }

    //! Should GeNN generate empty state push and pull functions?
    virtual bool shouldGenerateEmptyStatePushPull() const override { return m_Preferences.generateEmptyStatePushPull; }

    //! Should GeNN generate pull functions for extra global parameters? These are very rarely used
    virtual bool shouldGenerateExtraGlobalParamPull() const override { return m_Preferences.generateExtraGlobalParamPull; }

    //! How many bytes of memory does 'device' have
    virtual size_t getDeviceMemoryBytes() const override{ return m_ChosenDevice.totalGlobalMem; }

    //! Some backends will have additional small, fast, memory spaces for read-only data which might
    //! Be well-suited to storing merged group structs. This method returns the prefix required to
    //! Place arrays in these and their size in preferential order
    virtual MemorySpaces getMergedGroupMemorySpaces(const ModelSpecMerged &modelMerged) const override;

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------
    const cudaDeviceProp &getChosenCUDADevice() const{ return m_ChosenDevice; }
    int getChosenDeviceID() const{ return m_ChosenDeviceID; }
    int getRuntimeVersion() const{ return m_RuntimeVersion; }
    std::string getNVCCFlags() const;

    std::string getFloatAtomicAdd(const std::string &ftype) const;

    //! Get total number of RNG streams potentially used to initialise model
    /*! **NOTE** because RNG supports 2^64 streams, we are overly conservative */
    size_t getNumInitialisationRNGStreams(const ModelSpecMerged &modelMerged) const;

    size_t getKernelBlockSize(Kernel kernel) const{ return m_KernelBlockSizes.at(kernel); }

    //--------------------------------------------------------------------------
    // Static API
    //--------------------------------------------------------------------------
    static size_t getNumPresynapticUpdateThreads(const SynapseGroupInternal &sg, const cudaDeviceProp &deviceProps,
                                                 const Preferences &preferences);
    static size_t getNumPostsynapticUpdateThreads(const SynapseGroupInternal &sg);
    static size_t getNumSynapseDynamicsThreads(const SynapseGroupInternal &sg);

    //! Register a new presynaptic update strategy
    /*! This function should be called with strategies in ascending order of preference */
    static void addPresynapticUpdateStrategy(PresynapticUpdateStrategy::Base* strategy);

    //--------------------------------------------------------------------------
    // Constants
    //--------------------------------------------------------------------------
    static const char *KernelNames[KernelMax];

private:
    //--------------------------------------------------------------------------
    // Type definitions
    //--------------------------------------------------------------------------
    template<typename T>
    using GetPaddedGroupSizeFunc = std::function<size_t(const T&)>;

    //--------------------------------------------------------------------------
    // Private methods
    //--------------------------------------------------------------------------
    template<typename T>
    void genParallelGroup(CodeStream &os, const Substitutions &kernelSubs, const std::vector<T> &groups, const std::string &mergedGroupPrefix, size_t &idStart,
                          GetPaddedGroupSizeFunc<typename T::GroupInternal> getPaddedSizeFunc,
                          GroupHandler<T> handler) const
    {
        // Loop through groups
        for(const auto &gMerge : groups) {
            // Sum padded sizes of each group within merged group
            const size_t paddedSize = std::accumulate(
                gMerge.getGroups().cbegin(), gMerge.getGroups().cend(), size_t{0},
                [gMerge, getPaddedSizeFunc](size_t acc, std::reference_wrapper<const typename T::GroupInternal> g)
                {
                    return (acc + getPaddedSizeFunc(g.get()));
                });

            os << "// merged" << gMerge.getIndex() << std::endl;

            // If this is the first  group
            if(idStart == 0) {
                os << "if(id < " << paddedSize << ")";
            }
            else {
                os << "if(id >= " << idStart << " && id < " << idStart + paddedSize << ")";
            }
            {
                CodeStream::Scope b(os);
                Substitutions popSubs(&kernelSubs);

                if(gMerge.getGroups().size() == 1) {
                    os << "const auto *group = &d_merged" << mergedGroupPrefix << "Group" << gMerge.getIndex() << "[0];" << std::endl;
                    os << "const unsigned int lid = id - " << idStart << ";" << std::endl;
                }
                else {
                    // Perform bisect operation to get index of merged struct
                    os << "unsigned int lo = 0;" << std::endl;
                    os << "unsigned int hi = " << gMerge.getGroups().size() << ";" << std::endl;
                    os << "while(lo < hi)" << std::endl;
                    {
                        CodeStream::Scope b(os);
                        os << "const unsigned int mid = (lo + hi) / 2;" << std::endl;

                        os << "if(id < d_merged" << mergedGroupPrefix << "GroupStartID" << gMerge.getIndex() << "[mid])";
                        {
                            CodeStream::Scope b(os);
                            os << "hi = mid;" << std::endl;
                        }
                        os << "else";
                        {
                            CodeStream::Scope b(os);
                            os << "lo = mid + 1;" << std::endl;
                        }
                    }

                    // Use this to get reference to merged group structure
                    os << "const auto *group = &d_merged" << mergedGroupPrefix << "Group" << gMerge.getIndex() << "[lo - 1]; " << std::endl;

                    // Use this and starting thread of merged group to calculate local id within neuron group
                    os << "const unsigned int lid = id - (d_merged" << mergedGroupPrefix << "GroupStartID" << gMerge.getIndex() << "[lo - 1]);" << std::endl;

                }
                popSubs.addVarSubstitution("id", "lid");
                handler(os, gMerge, popSubs);

                idStart += paddedSize;
            }
        }
    }


    template<typename T>
    void genMergedStructArrayPush(CodeStream &os, const std::vector<T> &groups, const std::string &name, MemorySpaces &memorySpaces) const
    {
        // Loop through groups
        for(const auto &g : groups) {
            // Get size of group in bytes
            const size_t groupBytes = g.getStructArraySize(*this);

            // Loop through memory spaces
            bool memorySpaceFound = false;
            for(auto &m : memorySpaces) {
                // If there is space in this memory space for group
                if(m.second > groupBytes) {
                    // Implement merged group array in this memory space
                    os << m.first << " Merged" << name << "Group" << g.getIndex() << " d_merged" << name << "Group" << g.getIndex() << "[" << g.getGroups().size() << "];" << std::endl;

                    // Set flag
                    memorySpaceFound = true;

                    // Subtract
                    m.second -= groupBytes;

                    // Stop searching
                    break;
                }
            }

            assert(memorySpaceFound);

            // Write function to update
            os << "void pushMerged" << name << "Group" << g.getIndex() << "ToDevice(unsigned int idx, ";
            g.generateStructFieldArgumentDefinitions(os, *this);
            os << ")";
            {
                CodeStream::Scope b(os);

                // Loop through sorted fields and build struct on the stack
                os << "Merged" << name << "Group" << g.getIndex() << " group = {";
                const auto sortedFields = g.getSortedFields(*this);
                for(const auto &f : sortedFields) {
                    os << std::get<1>(f) << ", ";
                }
                os << "};" << std::endl;

                // Push to device
                os << "CHECK_CUDA_ERRORS(cudaMemcpyToSymbolAsync(d_merged" << name << "Group" << g.getIndex() << ", &group, ";
                os << "sizeof(Merged" << name << "Group" << g.getIndex() << "), idx * sizeof(Merged" << name << "Group" << g.getIndex() << ")));" << std::endl;
            }
        }
    }

    void genEmitSpike(CodeStream &os, const Substitutions &subs, const std::string &suffix) const;

    void genCurrentSpikePush(CodeStream &os, const NeuronGroupInternal &ng, bool spikeEvent) const;
    void genCurrentSpikePull(CodeStream &os, const NeuronGroupInternal &ng, bool spikeEvent) const;

    void genKernelDimensions(CodeStream &os, Kernel kernel, size_t numThreads) const;

    //! Adds a type - both to backend base's list of sized types but also to device types set
    void addDeviceType(const std::string &type, size_t size);

    //! Is type a a device only type?
    bool isDeviceType(const std::string &type) const;

    // Get appropriate presynaptic update strategy to use for this synapse group
    const PresynapticUpdateStrategy::Base *getPresynapticUpdateStrategy(const SynapseGroupInternal &sg) const
    {
        return getPresynapticUpdateStrategy(sg, m_ChosenDevice, m_Preferences);
    }

    //--------------------------------------------------------------------------
    // Private static methods
    //--------------------------------------------------------------------------
    // Get appropriate presynaptic update strategy to use for this synapse group
    static const PresynapticUpdateStrategy::Base *getPresynapticUpdateStrategy(const SynapseGroupInternal &sg,
                                                                               const cudaDeviceProp &deviceProps,
                                                                               const Preferences &preferences);

    //--------------------------------------------------------------------------
    // Members
    //--------------------------------------------------------------------------
    const KernelBlockSize m_KernelBlockSizes;
    const Preferences m_Preferences;
    
    const int m_ChosenDeviceID;
    cudaDeviceProp m_ChosenDevice;

    int m_RuntimeVersion;

    //! Types that are only supported on device i.e. should never be exposed to user code
    std::unordered_set<std::string> m_DeviceTypes;

    //--------------------------------------------------------------------------
    // Static members
    //--------------------------------------------------------------------------
    static std::vector<PresynapticUpdateStrategy::Base*> s_PresynapticUpdateStrategies;
};
}   // CUDA
}   // CodeGenerator
