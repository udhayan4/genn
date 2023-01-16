// Google test includes
#include "gtest/gtest.h"

// GeNN includes
#include "type.h"

// GeNN transpiler includes
#include "transpiler/errorHandler.h"
#include "transpiler/parser.h"
#include "transpiler/scanner.h"
#include "transpiler/typeChecker.h"

using namespace GeNN;
using namespace GeNN::Transpiler;

//--------------------------------------------------------------------------
// Anonymous namespace
//--------------------------------------------------------------------------
namespace
{
class TestErrorHandler : public ErrorHandlerBase
{
public:
    TestErrorHandler() : m_Error(false)
    {}

    bool hasError() const { return m_Error; }

    virtual void error(size_t line, std::string_view message) override
    {
        report(line, "", message);
    }

    virtual void error(const Token &token, std::string_view message) override
    {
        if(token.type == Token::Type::END_OF_FILE) {
            report(token.line, " at end", message);
        }
        else {
            report(token.line, " at '" + std::string{token.lexeme} + "'", message);
        }
    }

private:
    void report(size_t line, std::string_view where, std::string_view message)
    {
        std::cerr << "[line " << line << "] Error" << where << ": " << message << std::endl;
        m_Error = true;
    }

    bool m_Error;
};

class TestEnvironment : public TypeChecker::EnvironmentBase
{
public:
    //---------------------------------------------------------------------------
    // Public API
    //---------------------------------------------------------------------------
    void define(const Type::Base *type, std::string_view name, bool isConstValue = false, bool isConstPointer = false)
    {
        if(!m_Types.try_emplace(name, type, isConstValue, isConstPointer).second) {
            throw std::runtime_error("Redeclaration of '" + std::string{name} + "'");
        }
    }
    
    template<typename T>
    void define(std::string_view name, bool isConstValue = false, bool isConstPointer = false)
    {
        define(T::getInstance(), name, isConstValue, isConstPointer);
    }
    
    template<typename T>
    void definePointer(std::string_view name, bool isConstValue = false, bool isConstPointer = false)
    {
        define(Type::createPointer(T::getInstance()), name, isConstValue, isConstPointer);
    }
    

    //---------------------------------------------------------------------------
    // EnvironmentBase virtuals
    //---------------------------------------------------------------------------
    virtual void define(const Token &name, const Type::QualifiedType &qualifiedType, ErrorHandlerBase &errorHandler) final
    {
        errorHandler.error(name, "Cannot declare variable in external environment");
        throw TypeChecker::TypeCheckError();
    }
    
    virtual const Type::QualifiedType &assign(const Token &name, Token::Type op, const Type::QualifiedType &assignedType, 
                                              ErrorHandlerBase &errorHandler, bool initializer = false) final
    {
        // If type isn't found
        auto existingType = m_Types.find(name.lexeme);
        if(existingType == m_Types.end()) {
            errorHandler.error(name, "Undefined variable");
            throw TypeChecker::TypeCheckError();
        }
        
        // Perform standard type-checking logic
        return EnvironmentBase::assign(name, op, existingType->second, assignedType, errorHandler, initializer);    
    }
    
    virtual const Type::QualifiedType &incDec(const Token &name, Token::Type op, ErrorHandlerBase &errorHandler) final
    {
        auto existingType = m_Types.find(name.lexeme);
        if(existingType == m_Types.end()) {
            errorHandler.error(name, "Undefined variable");
            throw TypeChecker::TypeCheckError();
        }
        
        // Perform standard type-checking logic
        return EnvironmentBase::incDec(name, op, existingType->second, errorHandler);
    }
    
    virtual const Type::QualifiedType &getType(const Token &name, ErrorHandlerBase &errorHandler) final
    {
        auto type = m_Types.find(std::string{name.lexeme});
        if(type == m_Types.end()) {
            errorHandler.error(name, "Undefined variable");
            throw TypeChecker::TypeCheckError();
        }
        else {
            return type->second;
        }
    }
    
private:
    //---------------------------------------------------------------------------
    // Members
    //---------------------------------------------------------------------------
    std::unordered_map<std::string_view, Type::QualifiedType> m_Types;
};

template<typename T>
std::string getPointerTypeName()
{
    return createPointer(T::getInstance())->getTypeName();
}

void typeCheckStatements(std::string_view code, TestEnvironment &typeEnvironment)
{
    // Scan
    TestErrorHandler errorHandler;
    const auto tokens = Scanner::scanSource(code, errorHandler);
    ASSERT_FALSE(errorHandler.hasError());
 
    // Parse
    const auto statements = Parser::parseBlockItemList(tokens, errorHandler);
    ASSERT_FALSE(errorHandler.hasError());
     
    // Typecheck
    TypeChecker::typeCheck(statements, typeEnvironment, errorHandler);
    ASSERT_FALSE(errorHandler.hasError());
}

Type::QualifiedType typeCheckExpression(std::string_view code, TestEnvironment &typeEnvironment)
{
    // Scan
    TestErrorHandler errorHandler;
    const auto tokens = Scanner::scanSource(code, errorHandler);
    EXPECT_FALSE(errorHandler.hasError());
 
    // Parse
    const auto expression = Parser::parseExpression(tokens, errorHandler);
    EXPECT_FALSE(errorHandler.hasError());
     
    // Typecheck
    const auto type = TypeChecker::typeCheck(expression.get(), typeEnvironment, errorHandler);
    EXPECT_FALSE(errorHandler.hasError());
    return type;
}
}   // Anonymous namespace

//--------------------------------------------------------------------------
// Tests
//--------------------------------------------------------------------------
TEST(TypeChecker, ArraySubscript)
{
    // Integer array indexing
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        const auto type = typeCheckExpression("intArray[4]", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Int32::getInstance()->getTypeName());
        EXPECT_FALSE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }
    
    // Float array indexing
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        typeCheckExpression("intArray[4.0f]", typeEnvironment);}, 
        TypeChecker::TypeCheckError);

    // Pointer indexing
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        typeEnvironment.definePointer<Type::Int32>("indexArray");
        typeCheckExpression("intArray[indexArray]", typeEnvironment);}, 
        TypeChecker::TypeCheckError);
}
//--------------------------------------------------------------------------
TEST(TypeChecker, Assignment)
{
    // Numeric assignment
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal");
        typeEnvironment.define<Type::Float>("floatVal");
        typeEnvironment.define<Type::Int32>("intValConst", true);
        typeCheckStatements(
            "int w = intVal;\n"
            "float x = floatVal;\n"
            "int y = floatVal;\n"
            "float z = intVal;\n"
            "int wc = intValConst;\n"
            "const int cw = intVal;\n"
            "const int cwc = intValConst;\n",
            typeEnvironment);
    }

    // Pointer assignement
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        typeEnvironment.definePointer<Type::Int32>("intArrayConst", true);
        typeCheckStatements(
            "int *x = intArray;\n"
            "const int *y = intArray;\n"
            "const int *z = intArrayConst;\n", 
            typeEnvironment);
    }

    // Pointer assignement, attempt to remove const
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray", true);
        typeCheckStatements("int *x = intArray;", typeEnvironment);},
        TypeChecker::TypeCheckError);

    // Pointer assignement without explicit cast
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        typeCheckStatements("float *x = intArray;", typeEnvironment);},
        TypeChecker::TypeCheckError);

    // **TODO** other assignements i.e. += -= %=
}
//--------------------------------------------------------------------------
TEST(TypeChecker, Binary)
{
}
//--------------------------------------------------------------------------
TEST(TypeChecker, Call)
{
}
//--------------------------------------------------------------------------
TEST(TypeChecker, Cast)
{
    // Numeric cast
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal");
        const auto type = typeCheckExpression("(float)intVal", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Float::getInstance()->getTypeName());
        EXPECT_FALSE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Numeric cast to const
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal");
        const auto type = typeCheckExpression("(const int)intVal", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Int32::getInstance()->getTypeName());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Pointer cast to value const
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        const auto type = typeCheckExpression("(const int*)intArray", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), getPointerTypeName<Type::Int32>());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Pointer cast to pointer const
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        const auto type = typeCheckExpression("(int * const)intArray", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), getPointerTypeName<Type::Int32>());
        EXPECT_FALSE(type.constValue);
        EXPECT_TRUE(type.constPointer);
    }

    // Can't remove value const from numeric
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal", true);
        typeCheckExpression("(int)intVal", typeEnvironment);},
        TypeChecker::TypeCheckError);

    // Can't remove value const from pointer
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray", true);
        typeCheckExpression("(int*)intArray", typeEnvironment);},
        TypeChecker::TypeCheckError);

    // Can't remove pointer const from pointer
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray", false, true);
        typeCheckExpression("(int*)intArray", typeEnvironment);},
        TypeChecker::TypeCheckError);

   // Pointer cast can't reinterpret
   EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        typeCheckExpression("(float*)intArray", typeEnvironment);},
        TypeChecker::TypeCheckError);
    
   // Pointer can't be cast to numeric
   EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        typeCheckExpression("(int)intArray", typeEnvironment);},
        TypeChecker::TypeCheckError);
    
   // Numeric can't be cast to pointer
   EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal");
        typeCheckExpression("(int*)intVal", typeEnvironment);},
        TypeChecker::TypeCheckError);
}
//--------------------------------------------------------------------------
TEST(TypeChecker, Conditional)
{
}
//--------------------------------------------------------------------------
TEST(TypeChecker, IncDec)
{
    // Can increment numeric
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal");
        const auto type = typeCheckExpression("intVal++", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Int32::getInstance()->getTypeName());
        EXPECT_FALSE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Can increment pointer
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        const auto type = typeCheckExpression("intArray++", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), getPointerTypeName<Type::Int32>());
        EXPECT_FALSE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Can increment pointer to const
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray", true);
        const auto type = typeCheckExpression("intArray++", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), getPointerTypeName<Type::Int32>());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

   // Can't increment const number
   EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal", true);
        typeCheckExpression("intVal++", typeEnvironment);},
        TypeChecker::TypeCheckError);
   
   // Can't increment const pointer
   EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray", false, true);
        typeCheckExpression("intArray++", typeEnvironment);},
        TypeChecker::TypeCheckError);
}
//--------------------------------------------------------------------------
TEST(TypeChecker, Literal)
{
    // Float
    {
        TestEnvironment typeEnvironment;
        const auto type = typeCheckExpression("1.0f", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Float::getInstance()->getTypeName());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Double
    {
        TestEnvironment typeEnvironment;
        const auto type = typeCheckExpression("1.0", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Double::getInstance()->getTypeName());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Integer
    {
        TestEnvironment typeEnvironment;
        const auto type = typeCheckExpression("100", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Int32::getInstance()->getTypeName());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Unsigned integer
    {
        TestEnvironment typeEnvironment;
        const auto type = typeCheckExpression("100U", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Uint32::getInstance()->getTypeName());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }
}
//--------------------------------------------------------------------------
TEST(TypeChecker, Unary)
{
    // Dereference pointer
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        const auto type = typeCheckExpression("*intArray", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Int32::getInstance()->getTypeName());
        EXPECT_FALSE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Dereference pointer to const
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray", true);
        const auto type = typeCheckExpression("*intArray", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Int32::getInstance()->getTypeName());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Dereference const pointer
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray", false, true);
        const auto type = typeCheckExpression("*intArray", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Int32::getInstance()->getTypeName());
        EXPECT_FALSE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Dereference const pointer to const
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray", true, true);
        const auto type = typeCheckExpression("*intArray", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), Type::Int32::getInstance()->getTypeName());
        EXPECT_TRUE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Dereference numeric
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal");
        typeCheckExpression("*intVal", typeEnvironment); },
        TypeChecker::TypeCheckError);

    // Address of numeric
    {
        TestEnvironment typeEnvironment;
        typeEnvironment.define<Type::Int32>("intVal");
        const auto type = typeCheckExpression("&intVal", typeEnvironment);
        EXPECT_EQ(type.type->getTypeName(), getPointerTypeName<Type::Int32>());
        EXPECT_FALSE(type.constValue);
        EXPECT_FALSE(type.constPointer);
    }

    // Address of pointer
    EXPECT_THROW({
        TestEnvironment typeEnvironment;
        typeEnvironment.definePointer<Type::Int32>("intArray");
        typeCheckExpression("&intArray", typeEnvironment);},
        TypeChecker::TypeCheckError);
}
