#pragma once

// Standard C++ includes
#include <string>

// GeNN includes
#include "type.h"

// Transpiler includes
#include "transpiler/statement.h"

// Forward declarations
namespace GeNN::CodeGenerator
{
class CodeStream;
}

//---------------------------------------------------------------------------
// GeNN::Transpiler::PrettyPrinter::EnvironmentBase
//---------------------------------------------------------------------------
namespace GeNN::Transpiler::PrettyPrinter
{
class EnvironmentBase
{
public:
    //------------------------------------------------------------------------
    // Declared virtuals
    //------------------------------------------------------------------------
    //! Define variable named by token and return the name as it should be used in code
    virtual std::string define(const Token &name) = 0;
    
    //! Get the name to use in code for the variable named by token
    virtual std::string getName(const Token &name) = 0;
    
    //! Get stream to write code within this environment to
    virtual CodeGenerator::CodeStream &getStream() = 0;
};

//---------------------------------------------------------------------------
// Free functions
//---------------------------------------------------------------------------
void print(const Statement::StatementList &statements, EnvironmentBase &environment, 
           const Type::TypeContext &context);
}
