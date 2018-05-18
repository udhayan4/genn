/*--------------------------------------------------------------------------
   Author: Anton Komissarov
  
   Institute: Center for Computational Neuroscience and Robotics
              University of Sussex
	      Falmer, Brighton BN1 9QJ, UK 
  
   email to:  T.Nowotny@sussex.ac.uk
  
   initial version: 2018-05-14
  
--------------------------------------------------------------------------*/

%module libgenn
%{
#include "variableMode.h"
#include "global.h"

#include "codeStream.h"
#include "codeGenUtils.h"

#include "dpclass.h"

#include "neuronModels.h"

#include "standardSubstitutions.h"
#include "standardGeneratedSections.h"

#include "neuronGroup.h"
#include "postSynapseModels.h"

#include "synapseModels.h"
#include "synapseMatrixType.h"
#include "synapseGroup.h"
#include "utils.h"
#include "modelSpec.h"

#include "sparseProjection.h"
#include "sparseUtils.h"

#include "stringUtils.h"
#include "hr_time.h"

#include "generateCPU.h"
#include "generateInit.h"
#include "generateRunner.h"
#include "generateKernels.h"
#include "generateMPI.h"
#include "generateALL.h"
%}

%ignore GenericFunction;
%ignore FunctionTemplate;
%include <std_string.i>
%include <std_vector.i>

%include "include/variableMode.h"
%include "include/global.h"

%include "include/codeStream.h"
%include "include/codeGenUtils.h"

%include "include/dpclass.h"

%include "include/neuronModels.h"
%import "swig/newNeuronModels.i"

%include "include/standardSubstitutions.h"
%include "include/standardGeneratedSections.h"

%include "include/neuronGroup.h"
%include "include/postSynapseModels.h"
%import "swig/newPostsynapticModels.i"
%include "include/synapseModels.h"
%import "swig/newWeightUpdateModels.i"
%include "include/synapseMatrixType.h"
%include "include/synapseGroup.h"
%include "include/utils.h"
%include "include/modelSpec.h"

%include "include/sparseProjection.h"
%include "include/sparseUtils.h"

%include "include/generateCPU.h"
%include "include/generateInit.h"
%include "include/generateRunner.h"
%include "include/generateKernels.h"
%include "include/generateMPI.h"
%include "include/generateALL.h"


%include "include/stringUtils.h"
%include "include/hr_time.h"

%template(dvec) vector<double>;
