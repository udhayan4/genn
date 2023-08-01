#pragma once

// Standard C++ includes
#include <array>
#include <numeric>
#include <unordered_set>

// GeNN includes
#include "gennExport.h"
#include "varAccess.h"

// GeNN code generator includes
#include "code_generator/backendBase.h"
#include "code_generator/codeStream.h"
#include "code_generator/environment.h"
#include "code_generator/presynapticUpdateStrategySIMT.h"

//--------------------------------------------------------------------------
// GeNN::CodeGenerator::Kernel
//--------------------------------------------------------------------------
namespace GeNN::CodeGenerator
{
//! Kernels generated by SIMT backends
enum Kernel
{
    KernelNeuronUpdate,
    KernelPresynapticUpdate,
    KernelPostsynapticUpdate,
    KernelSynapseDynamicsUpdate,
    KernelInitialize,
    KernelInitializeSparse,
    KernelNeuronSpikeQueueUpdate,
    KernelNeuronPrevSpikeTimeUpdate,
    KernelSynapseDendriticDelayUpdate,
    KernelCustomUpdate,
    KernelCustomTransposeUpdate,
    KernelMax
};

//--------------------------------------------------------------------------
// Type definitions
//--------------------------------------------------------------------------
//! Array of block sizes for each kernel
using KernelBlockSize = std::array<size_t, KernelMax>;

//--------------------------------------------------------------------------
// GeNN::CodeGenerator::BackendSIMT
//--------------------------------------------------------------------------
//! Base class for Single Instruction Multiple Thread style backends
/*! CUDA terminology is used throughout i.e. thread blocks and shared memory */
class GENN_EXPORT BackendSIMT : public BackendBase
{
public:
    BackendSIMT(const KernelBlockSize &kernelBlockSizes, const PreferencesBase &preferences)
    :   BackendBase(preferences), m_KernelBlockSizes(kernelBlockSizes)
    {}

    //------------------------------------------------------------------------
    // Enumerations
    //------------------------------------------------------------------------
    //! What atomic operation is required
    enum class AtomicOperation
    {
        ADD,
        OR,
    };

    //! What memory space atomic operation is required
    enum class AtomicMemSpace
    {
        GLOBAL,
        SHARED,
    };

    //------------------------------------------------------------------------
    // Declared virtuals
    //------------------------------------------------------------------------
    //! On some older devices, shared memory atomics are actually slower than global memory atomics so should be avoided
    virtual bool areSharedMemAtomicsSlow() const = 0;

    //! Get the prefix to use for shared memory variables
    virtual std::string getSharedPrefix() const = 0;

    //! Get the ID of the current thread within the threadblock
    virtual std::string getThreadID(unsigned int axis = 0) const = 0;

    //! Get the ID of the current thread block
    virtual std::string getBlockID(unsigned int axis = 0) const = 0;

    //! Get the name of the count-leading-zeros function
    virtual std::string getCLZ() const = 0;

    //! Get name of atomic operation
    virtual std::string getAtomic(const Type::ResolvedType &type,
                                  AtomicOperation op = AtomicOperation::ADD, 
                                  AtomicMemSpace memSpace = AtomicMemSpace::GLOBAL) const = 0;

    //! Generate a shared memory barrier
    virtual void genSharedMemBarrier(CodeStream &os) const = 0;

    //! For SIMT backends which initialize RNGs on device, initialize population RNG with specified seed and sequence
    virtual void genPopulationRNGInit(CodeStream &os, const std::string &globalRNG, const std::string &seed, const std::string &sequence) const = 0;

    //! Generate a preamble to add substitution name for population RNG
    virtual std::string genPopulationRNGPreamble(CodeStream &os, const std::string &globalRNG) const = 0;
    
    //! If required, generate a postamble for population RNG
    /*! For example, in OpenCL, this is used to write local RNG state back to global memory*/
    virtual void genPopulationRNGPostamble(CodeStream &os, const std::string &globalRNG) const = 0;

    //! Generate code to skip ahead local copy of global RNG
    virtual std::string genGlobalRNGSkipAhead(CodeStream &os, const std::string &sequence) const = 0;

    //! Get type of population RNG
    virtual Type::ResolvedType getPopulationRNGType() const = 0;

    //------------------------------------------------------------------------
    // BackendBase virtuals
    //------------------------------------------------------------------------
    //! Gets the stride used to access synaptic matrix rows, taking into account sparse data structure, padding etc
    virtual size_t getSynapticMatrixRowStride(const SynapseGroupInternal &sg) const final;

    //! When backends require separate 'device' and 'host' versions of variables, they are identified with a prefix.
    //! This function returns the device prefix so it can be used in otherwise platform-independent code.
    virtual std::string getDeviceVarPrefix() const final { return getPreferences().automaticCopy ? "" : "d_"; }

    virtual void genPopVariableInit(EnvironmentExternalBase &env, HandlerEnv handler) const final;
    virtual void genVariableInit(EnvironmentExternalBase &env, const std::string &count, const std::string &indexVarName, HandlerEnv handler) const final;
    virtual void genSparseSynapseVariableRowInit(EnvironmentExternalBase &env, HandlerEnv handler) const final
    {
        genSynapseVariableRowInit(env, handler);
    }

    virtual void genDenseSynapseVariableRowInit(EnvironmentExternalBase &env, HandlerEnv handler) const final
    {
        genSynapseVariableRowInit(env, handler);
    }
    
    virtual void genKernelSynapseVariableInit(EnvironmentExternalBase &env, SynapseInitGroupMerged &sg, HandlerEnv handler) const final;
    virtual void genKernelCustomUpdateVariableInit(EnvironmentExternalBase &env, CustomWUUpdateInitGroupMerged &cu, HandlerEnv handler) const final;

    //! Should 'scalar' variables be implemented on device or can host variables be used directly?
    virtual bool isDeviceScalarRequired() const final { return true; }

    virtual bool isGlobalHostRNGRequired(const ModelSpecInternal &model) const final;
    virtual bool isGlobalDeviceRNGRequired(const ModelSpecInternal &model) const final;

    virtual bool isPostsynapticRemapRequired() const final { return true; }

    //------------------------------------------------------------------------
    // Public API
    //------------------------------------------------------------------------
    //! Get total number of RNG streams potentially used to initialise model
    /*! **NOTE** because RNG supports 2^64 streams, we are overly conservative */
    size_t getNumInitialisationRNGStreams(const ModelSpecMerged & modelMerged) const;

    size_t getKernelBlockSize(Kernel kernel) const { return m_KernelBlockSizes.at(kernel); }

    size_t getPaddedNumCustomUpdateThreads(const CustomUpdateInternal &cg, unsigned int batchSize) const;
    size_t getPaddedNumCustomUpdateWUThreads(const CustomUpdateWUInternal &cg, unsigned int batchSize) const;
    size_t getPaddedNumCustomUpdateTransposeWUThreads(const CustomUpdateWUInternal &cg, unsigned int batchSize) const;
    

    //--------------------------------------------------------------------------
    // Static API
    //--------------------------------------------------------------------------
    static size_t getNumPresynapticUpdateThreads(const SynapseGroupInternal &sg, const PreferencesBase &preferences);
    static size_t getNumPostsynapticUpdateThreads(const SynapseGroupInternal &sg);
    static size_t getNumSynapseDynamicsThreads(const SynapseGroupInternal &sg);
    static size_t getNumConnectivityInitThreads(const SynapseGroupInternal &sg);
    static size_t getNumInitThreads(const SynapseGroupInternal &sg);
    static size_t getNumInitThreads(const CustomUpdateWUInternal &cg);

    //! Register a new presynaptic update strategy
    /*! This function should be called with strategies in ascending order of preference */
    static void addPresynapticUpdateStrategy(PresynapticUpdateStrategySIMT::Base *strategy);

    //--------------------------------------------------------------------------
    // Constants
    //--------------------------------------------------------------------------
    static const char *KernelNames[KernelMax];

protected:
    //------------------------------------------------------------------------
    // Protected API
    //------------------------------------------------------------------------
    void genNeuronPrevSpikeTimeUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                            BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;
    void genNeuronSpikeQueueUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                         BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;
    void genNeuronUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                               BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;

    void genSynapseDendriticDelayUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                              BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;
    void genPresynapticUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                    BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;
    void genPostsynapticUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                     BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;
    void genSynapseDynamicsKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                                  BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;

    void genCustomUpdateKernel(EnvironmentExternal &env, ModelSpecMerged &modelMerged,
                               BackendBase::MemorySpaces &memorySpaces, const std::string &updateGroup, size_t &idStart) const;

    void genCustomUpdateWUKernel(EnvironmentExternal &env, ModelSpecMerged &modelMerged,
                                 BackendBase::MemorySpaces &memorySpaces, const std::string &updateGroup, size_t &idStart) const;
    
    void genCustomTransposeUpdateWUKernel(EnvironmentExternal &env, ModelSpecMerged &modelMerged,
                                          BackendBase::MemorySpaces &memorySpaces, const std::string &updateGroup, size_t &idStart) const;

    void genCustomConnectivityUpdateKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged,
                                           BackendBase::MemorySpaces &memorySpaces, const std::string &updateGroup, size_t &idStart) const;

    void genInitializeKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, 
                             BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;
   
    void genInitializeSparseKernel(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged,
                                   size_t numInitializeThreads, BackendBase::MemorySpaces &memorySpaces, size_t &idStart) const;

    //! Helper wrapper around padSize to pad size to a kernel size
    size_t padKernelSize(size_t size, Kernel kernel) const;

    //! Get kernel block size
    const KernelBlockSize &getKernelBlockSize() const { return m_KernelBlockSizes; }

private:
    //--------------------------------------------------------------------------
    // Type definitions
    //--------------------------------------------------------------------------
    template<typename G>
    using GenMergedGroupsFn = void (ModelSpecMerged::*)(const BackendBase&, BackendBase::MemorySpaces&, std::function<void(G&)>);

    template<typename G>
    using GenMergedCustomUpdateGroupsFn = void (ModelSpecMerged::*)(const BackendBase&, BackendBase::MemorySpaces&, const std::string &, std::function<void(G&)>);
    
    //--------------------------------------------------------------------------
    // Private methods
    //--------------------------------------------------------------------------
    template<typename T, typename S>
    void genGroup(EnvironmentExternalBase &env, T &gMerge, size_t &idStart,
                  S getPaddedSizeFn, GroupHandlerEnv<T> handler) const
    {
        // Sum padded sizes of each group within merged group
        const size_t paddedSize = std::accumulate(
            gMerge.getGroups().cbegin(), gMerge.getGroups().cend(), size_t{0},
            [getPaddedSizeFn](size_t acc, std::reference_wrapper<const typename T::GroupInternal> g)
            {
                return (acc + getPaddedSizeFn(g.get()));
            });

        env.getStream() << "// merged" << gMerge.getIndex() << std::endl;

        // If this is the first  group
        if(idStart == 0) {
            env.getStream() << "if(id < " << paddedSize << ")";
        }
        else {
            env.getStream() << "if(id >= " << idStart << " && id < " << idStart + paddedSize << ")";
        }
        {
            CodeStream::Scope b(env.getStream());

            if(gMerge.getGroups().size() == 1) {
                EnvironmentExternal groupEnv(env);
                groupEnv.getStream() << getPointerPrefix() << "struct Merged" << T::name << "Group" << gMerge.getIndex() << " *group";
                groupEnv.getStream() << " = &d_merged" << T::name << "Group" << gMerge.getIndex() << "[0]; " << std::endl;
                groupEnv.add(Type::Uint32.addConst(), "id", "lid",
                             {groupEnv.addInitialiser("const unsigned int lid = id - " + std::to_string(idStart) + ";")});

                // Use the starting thread ID of the whole merged group as group_start_id
                groupEnv.add(Type::Uint32.addConst(), "_group_start_id", std::to_string(idStart));
                
                // Launch handler
                handler(groupEnv, gMerge);
            }
            else {
                // Perform bisect operation to get index of merged struct
                env.getStream() << "unsigned int lo = 0;" << std::endl;
                env.getStream() << "unsigned int hi = " << gMerge.getGroups().size() << ";" << std::endl;
                env.getStream() << "while(lo < hi)" << std::endl;
                {
                    CodeStream::Scope b(env.getStream());
                    env.getStream() << "const unsigned int mid = (lo + hi) / 2;" << std::endl;

                    env.getStream() << "if(id < d_merged" << T::name << "GroupStartID" << gMerge.getIndex() << "[mid])";
                    {
                        CodeStream::Scope b(env.getStream());
                        env.getStream() << "hi = mid;" << std::endl;
                    }
                    env.getStream() << "else";
                    {
                        CodeStream::Scope b(env.getStream());
                        env.getStream() << "lo = mid + 1;" << std::endl;
                    }
                }

                // Use this to get reference to merged group structure
                env.getStream() << getPointerPrefix() << "struct Merged" << T::name << "Group" << gMerge.getIndex() << " *group";
                env.getStream() << " = &d_merged" << T::name << "Group" << gMerge.getIndex() << "[lo - 1]; " << std::endl;

                // Get group start thread ID and use as group_start_id
                EnvironmentExternal groupEnv(env);
                groupEnv.add(Type::Uint32.addConst(), "_group_start_id", "groupStartID",
                             {groupEnv.addInitialiser("const unsigned int groupStartID = d_merged" + T::name + "GroupStartID" + std::to_string(gMerge.getIndex()) + "[lo - 1];")});

                // Use this to calculate local id within group
                groupEnv.add(Type::Uint32.addConst(), "id", "lid",
                             {groupEnv.addInitialiser("const unsigned int lid = id - $(_group_start_id);")});
                
                // Launch handler
                handler(groupEnv, gMerge);
            }

           

            idStart += paddedSize;
        }
    }


    template<typename T, typename S>
    void genParallelGroup(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, BackendBase::MemorySpaces &memorySpaces, size_t &idStart, 
                          GenMergedGroupsFn<T> generateGroupFn,  S getPaddedSizeFunc, GroupHandlerEnv<T> handler) const
    {
        std::invoke(generateGroupFn, modelMerged, *this, memorySpaces,
                    [this, getPaddedSizeFunc, handler, &env, &idStart](T &g)
                    {
                        genGroup(env, g, idStart, getPaddedSizeFunc, handler);
                    });
    }

    template<typename T, typename S>
    void genParallelGroup(EnvironmentExternalBase &env, ModelSpecMerged &modelMerged, BackendBase::MemorySpaces &memorySpaces, 
                          const std::string &updateGroupName, size_t &idStart, GenMergedCustomUpdateGroupsFn<T> generateGroupFn,  
                          S getPaddedSizeFunc, GroupHandlerEnv<T> handler) const
    {
        std::invoke(generateGroupFn, modelMerged, *this, memorySpaces, updateGroupName,
                    [this, getPaddedSizeFunc, handler, &env, &idStart](T &g)
                    {
                        genGroup(env, g, idStart, getPaddedSizeFunc, handler);
                    });
    }
    
    // Helper function to generate kernel code to initialise variables associated with synapse group or custom WU update with dense/kernel connectivity
    template<typename G>
    void genSynapseVarInit(EnvironmentExternalBase &env, unsigned int batchSize, G &g,
                           bool initRNGRequired, bool kernel, size_t kernelDimensions) const
    {
        env.print("if($(id) < ");
        
        // If synapse group has kernel weights, check ID against product of kernel dimensions
        if (kernel) {
            // Loop through kernel dimensions and multiply together
            env.getStream() << "(";
            for (size_t i = 0; i < kernelDimensions; i++) {
                env.print(getKernelSize(g, i));
                if (i != (kernelDimensions - 1)) {
                    env.getStream() << " * ";
                }
            }
            env.getStream() << ")";
        }
        // Otherwise, against number of postsynaptic neurons
        else {
            env.print("$(num_post)");
        }
        env.getStream() << ")";
        {
            CodeStream::Scope b(env.getStream());
            EnvironmentGroupMergedField<G> initEnv(env, g);

            // If an RNG is required for initialisation,
            // make copy of global phillox RNG and skip ahead by thread id
            // **NOTE** not LOCAL id
            if(initRNGRequired) {
                initEnv.add(Type::Void, "_rng", 
                            genGlobalRNGSkipAhead(initEnv.getStream(), "id"));
            }

            // If synapse group has kernel weights
            if (kernel) {
                // Loop through kernel dimensions to generate seperate indices
                for (size_t i = 0; i < kernelDimensions; i++) {
                    std::ostringstream kernelIDInit;
                    kernelIDInit << "const unsigned int kernelID" << i << " = ($(id)";

                    // If this isn't the last dimension
                    if (i < (kernelDimensions - 1)) {
                        // Loop backwards through other kernel and generate code to divide by product of subsequent dimensions
                        kernelIDInit << " / (";
                        for (size_t j = (kernelDimensions - 1); j > i; j--) {
                            kernelIDInit << getKernelSize(g, j);

                            if (j != (i + 1)) {
                                kernelIDInit << " * ";
                            }
                        }
                        kernelIDInit << ")";
                    }
                    kernelIDInit << ")";

                    // If this isn't the first dimension, take modulus of kernel size
                    if (i > 0) {
                        kernelIDInit << " % " << getKernelSize(g, i);
                    }

                    kernelIDInit << ";" << std::endl;

                    // Add substitution
                    initEnv.add(Type::Uint32.addConst(), "id_kernel_" + std::to_string(i), "kernelID" + std::to_string(i),
                                {initEnv.addInitialiser(kernelIDInit.str())});
                }
            }
            // Otherwise, just substitute postsynaptic index
            else {
                initEnv.add(Type::Uint32.addConst(), "id_post", "$(id)");
            }

            // Generate init code
            g.generateInit(*this, initEnv, batchSize);
        }
    }
    
    // Helper function to generate kernel code to initialise variables associated with synapse group or custom WU update with sparse connectivity
    template<typename G>
    void genSparseSynapseVarInit(EnvironmentExternalBase &env, unsigned int batchSize, G &g,
                                 bool varInitRequired, GroupHandlerEnv<G> handler) const
    {
        // Calculate how many blocks rows need to be processed in (in order to store row lengths in shared memory)
        const size_t blockSize = getKernelBlockSize(KernelInitializeSparse);
        const std::string blockSizeStr = std::to_string(blockSize);
        env.printLine("const unsigned int numBlocks = ($(num_pre) + " + blockSizeStr + " - 1) / " + blockSizeStr + ";");
        env.printLine("unsigned int idx = $(id);");

        // Loop through blocks
        env.getStream() << "for(unsigned int r = 0; r < numBlocks; r++)";
        {
            CodeStream::Scope b(env.getStream());

            // Calculate number of rows to process in this block
            env.getStream() << "const unsigned numRowsInBlock = (r == (numBlocks - 1))";
            env.getStream() << " ? ((" << env["num_pre"] << " - 1) % " << blockSize << ") + 1";
            env.getStream() << " : " << blockSize << ";" << std::endl;

            // Use threads to copy block of sparse structure into shared memory
            genSharedMemBarrier(env.getStream());
            env.getStream() << "if (" << getThreadID() << " < numRowsInBlock)";
            {
                CodeStream::Scope b(env.getStream());
                env.printLine("$(_sh_row_length)[" + getThreadID() + "] = $(_row_length)[(r * " + blockSizeStr + ") + " + getThreadID() + "];");
            }
            genSharedMemBarrier(env.getStream());

            // Loop through rows
            env.getStream() << "for(unsigned int i = 0; i < numRowsInBlock; i++)";
            {
                CodeStream::Scope b(env.getStream());

                // If there is a synapse for this thread to initialise
                env.print("if($(id) < $(_sh_row_length)[i])");
                {
                    CodeStream::Scope b(env.getStream());
                    
                    // Generate initialisation code
                    if(varInitRequired) {
                        EnvironmentExternal initEnv(env);
                        initEnv.add(Type::Uint32.addConst(), "id_pre", "((r * " + std::to_string(blockSize) + ") + i)");
                        initEnv.add(Type::Uint32.addConst(), "id_post", "$(_ind)[idx]");
                        g.generateInit(*this, initEnv, batchSize);
                    }
                    
                    // Call handler
                    handler(env, g);
                }

                // If matrix is ragged, advance index to next row by adding stride
                env.printLine("idx += $(_row_stride);");
            }
        }
    }

    void genEmitSpike(EnvironmentExternalBase &env, const std::string &suffix, bool recordingEnabled) const;

    void genRecordingSharedMemInit(CodeStream &os, const std::string &suffix) const;

    void genSynapseVariableRowInit(EnvironmentExternalBase &env, HandlerEnv handler) const;

    // Get appropriate presynaptic update strategy to use for this synapse group
    const PresynapticUpdateStrategySIMT::Base *getPresynapticUpdateStrategy(const SynapseGroupInternal &sg) const
    {
        return getPresynapticUpdateStrategy(sg, getPreferences());
    }

    //--------------------------------------------------------------------------
    // Private static methods
    //--------------------------------------------------------------------------
    // Get appropriate presynaptic update strategy to use for this synapse group
    static const PresynapticUpdateStrategySIMT::Base *getPresynapticUpdateStrategy(const SynapseGroupInternal &sg,
                                                                                   const PreferencesBase &preferences);

    //--------------------------------------------------------------------------
    // Members
    //--------------------------------------------------------------------------
    const KernelBlockSize m_KernelBlockSizes;

    //--------------------------------------------------------------------------
    // Static members
    //--------------------------------------------------------------------------
    static std::vector<PresynapticUpdateStrategySIMT::Base *> s_PresynapticUpdateStrategies;
};

}   // namespace GeNN::CodeGenerator
