#include "code_generator/codeGenUtils.h"

// Is C++ regex library operational?
// We assume it is for:
// 1) Compilers that don't define __GNUCC__
// 2) Clang
// 3) GCC 5.X.Y and future
// 4) Any future (4.10.Y?) GCC 4.X.Y releases
// 5) GCC 4.9.1 and subsequent patch releases (GCC fully implemented regex in 4.9.0
// BUT bug 61227 https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61227 prevented \w from working until 4.9.1)
#if !defined(__GNUC__) || \
    __clang__ || \
    __GNUC__ > 4 || \
    (__GNUC__ == 4 && (__GNUC_MINOR__ > 9 || \
                      (__GNUC_MINOR__ == 9 && __GNUC_PATCHLEVEL__ >= 1)))
    #include <regex>
#else
    #error "GeNN now requires a functioning std::regex implementation - please upgrade your version of GCC to at least 4.9.1"
#endif

// Standard C includes
#include <cstring>

// GeNN includes
#include "modelSpec.h"

// GeNN code generator includes
#include "code_generator/environment.h"
#include "code_generator/groupMerged.h"

// GeNN transpiler includes
#include "transpiler/parser.h"
#include "transpiler/prettyPrinter.h"

//----------------------------------------------------------------------------
// GeNN::CodeGenerator
//----------------------------------------------------------------------------
namespace GeNN::CodeGenerator
{
void genTypeRange(CodeStream &os, const Type::ResolvedType &type, const std::string &prefix)
{
    const auto &numeric = type.getNumeric();
    os << "#define " << prefix << "_MIN " << Utils::writePreciseString(numeric.min, numeric.maxDigits10) << numeric.literalSuffix << std::endl << std::endl;

    os << "#define " << prefix << "_MAX " << Utils::writePreciseString(numeric.max, numeric.maxDigits10) << numeric.literalSuffix << std::endl;
}
//----------------------------------------------------------------------------
std::string disambiguateNamespaceFunction(const std::string supportCode, const std::string code, std::string namespaceName) {
    // Regex for function call - looks for words with succeeding parentheses with or without any data inside the parentheses (arguments)
    std::regex funcCallRegex(R"(\w+(?=\(.*\)))");
    std::smatch matchedInCode;
    std::regex_search(code.begin(), code.end(), matchedInCode, funcCallRegex);
    std::string newCode = code;

    // Regex for function definition - looks for words with succeeding parentheses with or without any data inside the parentheses (arguments) followed by braces on the same or new line
    std::regex supportCodeRegex(R"(\w+(?=\(.*\)\s*\{))");
    std::smatch matchedInSupportCode;
    std::regex_search(supportCode.begin(), supportCode.end(), matchedInSupportCode, supportCodeRegex);

    // Iterating each function in code
    for (const auto& funcInCode : matchedInCode) {
        // Iterating over every function in support code to check if that function is indeed defined in support code (and not called - like fmod())
        for (const auto& funcInSupportCode : matchedInSupportCode) {
            if (funcInSupportCode.str() == funcInCode.str()) {
                newCode = std::regex_replace(newCode, std::regex(funcInCode.str()), namespaceName + "_$&");
                break;
            }
        }
    }
    return newCode;
}
//----------------------------------------------------------------------------
void prettyPrintExpression(const std::vector<Transpiler::Token> &tokens, const Type::TypeContext &typeContext, EnvironmentExternalBase &env, Transpiler::ErrorHandlerBase &errorHandler)
{
    using namespace Transpiler;

    // Parse tokens as expression
    auto expression = Parser::parseExpression(tokens, typeContext, errorHandler);

    // Resolve types
    auto resolvedTypes = TypeChecker::typeCheck(expression.get(), env, typeContext, errorHandler);

    // Pretty print
    PrettyPrinter::print(expression, env, typeContext, resolvedTypes);
}
 //--------------------------------------------------------------------------
void prettyPrintStatements(const std::vector<Transpiler::Token> &tokens, const Type::TypeContext &typeContext, EnvironmentExternalBase &env, 
                           Transpiler::ErrorHandlerBase &errorHandler, Transpiler::TypeChecker::StatementHandler forEachSynapseTypeCheckHandler,
                           Transpiler::PrettyPrinter::StatementHandler forEachSynapsePrettyPrintHandler)
{
    using namespace Transpiler;

    // Parse tokens as block item list (function body)
    auto updateStatements = Parser::parseBlockItemList(tokens, typeContext, errorHandler);

    // Resolve types
    auto resolvedTypes= TypeChecker::typeCheck(updateStatements, env, typeContext, errorHandler, forEachSynapseTypeCheckHandler);

    // Pretty print
    PrettyPrinter::print(updateStatements, env, typeContext, resolvedTypes, forEachSynapsePrettyPrintHandler);
}
//--------------------------------------------------------------------------
std::string printSubs(const std::string &format, Transpiler::PrettyPrinter::EnvironmentBase &env)
{
    // Create regex iterator to iterate over $(XXX) style varibles in format string
    // **NOTE** this doesn't match function argument $(0)
    std::regex regex("\\$\\(([a-zA-Z_][\\w]+)\\)");
    std::sregex_iterator matchesBegin(format.cbegin(), format.cend(), regex);
    std::sregex_iterator matchesEnd;
    
    // If there are no matches, leave format unmodified and return
    if(matchesBegin == matchesEnd) {
        return format;
    }
    // Otherwise
    else {
        // Loop through matches to build lazy string payload
        std::string output;
        for(std::sregex_iterator m = matchesBegin;;) {
            // Copy the non-matched subsequence (m->prefix()) onto output
            std::copy(m->prefix().first, m->prefix().second, std::back_inserter(output));

            // Add environment value of $(XXX) to output
            output += env[(*m)[1]];
    
            // If there are no subsequent matches, add the remaining 
            // non-matched characters onto output and return
            if(std::next(m) == matchesEnd) {
                 // Copy the non-matched subsequence (m->prefix()) onto output
                 std::copy(m->suffix().first, m->suffix().second, std::back_inserter(output));
                return output;
            }
            // Otherwise go onto next match
            else {
                m++;
            }
        }
    }
}
}   // namespace GeNN::CodeGenerator
