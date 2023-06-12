#include "code_generator/environment.h"

// Standard C++ includes
#include <algorithm>

// Standard C includes
#include <cassert>

// GeNN includes
#include "gennUtils.h"

// Transpiler includes
#include "transpiler/errorHandler.h"

using namespace GeNN;
using namespace GeNN::CodeGenerator;
using namespace GeNN::Transpiler;

//----------------------------------------------------------------------------
// GeNN::CodeGenerator::EnvironmentExternal
//----------------------------------------------------------------------------
std::string EnvironmentExternal::define(const std::string&)
{
    throw std::runtime_error("Cannot declare variable in external environment");
}
//----------------------------------------------------------------------------    
std::string EnvironmentExternal::getName(const std::string &name, std::optional<Type::ResolvedType> type)
{
    // If name isn't found in environment
    auto env = m_Environment.find(name);
    if (env == m_Environment.end()) {
        // If there's a parent environment in context, lookup there
        if (std::holds_alternative<std::reference_wrapper<EnvironmentExternal>>(m_Context)) {
            return std::get<std::reference_wrapper<EnvironmentExternal>>(m_Context).get().getName(name, type);
        }
        // Otherwise, give error
        // **NOTE** this should never throw as type checking should happen first
        else {
            throw std::runtime_error("Undefined identifier '" + name + "'");
        }
    }
    // Otherwise, return it's value
    else {
        return env->second.second;
    }
}
//----------------------------------------------------------------------------    
void EnvironmentExternal::define(const Token&, const Type::ResolvedType&, ErrorHandlerBase&)
{
    throw std::runtime_error("Cannot declare variable in external environment");
}
//----------------------------------------------------------------------------    
std::vector<Type::ResolvedType> EnvironmentExternal::getTypes(const Token &name, ErrorHandlerBase &errorHandler)
{
     // If name isn't found in environment
    auto env = m_Environment.find(name.lexeme);
    if (env == m_Environment.end()) {
        // If there's a parent environment in context, lookup there
        if (std::holds_alternative<std::reference_wrapper<EnvironmentExternal>>(m_Context)) {
            return std::get<std::reference_wrapper<EnvironmentExternal>>(m_Context).get().getTypes(name, errorHandler);
        }
        // Otherwise, give error
        // **NOTE** this should never throw as type checking should happen first
        else {
            errorHandler.error(name, "Undefined identifier");
            throw TypeChecker::TypeCheckError();
        }
    }
    // Otherwise, return it's type
    else {
        return {env->second.first};
    }
}
//----------------------------------------------------------------------------
void EnvironmentExternal::add(const Type::ResolvedType &type, const std::string &name, const std::string &value)
{
    if(!m_Environment.try_emplace(name, type, value).second) {
        throw std::runtime_error("Redeclaration of '" + std::string{name} + "'");
    }
}
//----------------------------------------------------------------------------    
CodeStream &EnvironmentExternal::getContextStream() const
{
    return std::visit(
        Utils::Overload{
            [](std::reference_wrapper<EnvironmentExternal> enclosing)->CodeStream& { return enclosing.get().getStream(); },
            [](std::reference_wrapper<CodeStream> os)->CodeStream& { return os.get(); }},
        getContext());
}
//----------------------------------------------------------------------------
std::string EnvironmentExternal::getContextName(const std::string &name, std::optional<Type::ResolvedType> type) const
{
    return std::visit(
        Utils::Overload{
            [&name, type](std::reference_wrapper<EnvironmentExternal> enclosing)->std::string { return enclosing.get().getName(name, type); },
            [&name](std::reference_wrapper<CodeStream>)->std::string { throw std::runtime_error("Identifier '" + name + "' undefined"); }},
        getContext());
}

//----------------------------------------------------------------------------
// GeNN::CodeGenerator::EnvironmentSubstitute
//----------------------------------------------------------------------------
EnvironmentSubstitute::~EnvironmentSubstitute()
{
    // Loop through initialiser
    for(const auto &i : m_Initialisers) {
        // If variable requiring initialiser has been referenced, write out initialiser
        if (i.first) {
            getContextStream() << i.second << std::endl;
        }
    }
        
    // Write contents to context stream
    getContextStream() << m_ContentsStream.str();
}
//----------------------------------------------------------------------------
std::string EnvironmentSubstitute::getName(const std::string &name, std::optional<Type::ResolvedType> type)
{
    // If there isn't a substitution for this name, try and get name from context
    auto var = m_VarSubstitutions.find(name);
    if(var == m_VarSubstitutions.end()) {
        return getContextName(name, type);
    }
    // Otherwise, return substitution
    else {
        // If this variable relies on any initialiser statements, mark these initialisers as required
        for(const auto i : var->second.second) {
            m_Initialisers.at(i).first = true;
        }

        return var->second.first;
    }
}
//------------------------------------------------------------------------
void EnvironmentSubstitute::addSubstitution(const std::string &source, const std::string &destination, 
                                            std::vector<size_t> initialisers)
{
    assert(std::all_of(initialisers.cbegin(), initialisers.cend(), 
                       [this](size_t i) { return i < m_Initialisers.size(); }));

    if(!m_VarSubstitutions.try_emplace(source, destination, initialisers).second) {
        throw std::runtime_error("Redeclaration of substitution '" + source + "'");
    }
}
//------------------------------------------------------------------------
size_t EnvironmentSubstitute::addInitialiser(const std::string &initialiser)
{
    m_Initialisers.emplace_back(false, initialiser);
    return (m_Initialisers.size() - 1);
}