#pragma once
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H

#include "Python3ParserBaseVisitor.h"
#include "BigInteger.h"
#include <any>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// Value types for Python interpreter
enum class ValueType {
    NONE,
    BOOL,
    INT,
    FLOAT,
    STRING,
    TUPLE
};

struct Value {
    ValueType type;
    bool boolVal;
    BigInteger intVal;
    double floatVal;
    std::string stringVal;
    std::vector<Value> tupleVal;

    Value() : type(ValueType::NONE) {}
    Value(bool b) : type(ValueType::BOOL), boolVal(b) {}
    Value(const BigInteger& i) : type(ValueType::INT), intVal(i) {}
    Value(double f) : type(ValueType::FLOAT), floatVal(f) {}
    Value(const std::string& s) : type(ValueType::STRING), stringVal(s) {}
    Value(const std::vector<Value>& t) : type(ValueType::TUPLE), tupleVal(t) {}

    bool toBool() const {
        switch (type) {
            case ValueType::NONE: return false;
            case ValueType::BOOL: return boolVal;
            case ValueType::INT: return !intVal.isZero();
            case ValueType::FLOAT: return floatVal != 0.0;
            case ValueType::STRING: return !stringVal.empty();
            case ValueType::TUPLE: return !tupleVal.empty();
        }
        return false;
    }

    std::string toString() const {
        switch (type) {
            case ValueType::NONE: return "None";
            case ValueType::BOOL: return boolVal ? "True" : "False";
            case ValueType::INT: return intVal.toString();
            case ValueType::FLOAT: {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6) << floatVal;
                return oss.str();
            }
            case ValueType::STRING: return stringVal;
            case ValueType::TUPLE: {
                std::string result = "(";
                for (size_t i = 0; i < tupleVal.size(); i++) {
                    if (i > 0) result += ", ";
                    result += tupleVal[i].toString();
                }
                if (tupleVal.size() == 1) result += ",";
                result += ")";
                return result;
            }
        }
        return "";
    }
};

// Function definition structure
struct FunctionDef {
    std::vector<std::string> params;
    std::vector<Value> defaultValues;
    Python3Parser::SuiteContext* body;
    std::map<std::string, Value>* capturedGlobals;
};

// Control flow exceptions
struct BreakException : public std::exception {};
struct ContinueException : public std::exception {};
struct ReturnException : public std::exception {
    Value value;
    ReturnException(const Value& v) : value(v) {}
};

class EvalVisitor : public Python3ParserBaseVisitor {
private:
    std::map<std::string, Value> globalVars;
    std::vector<std::map<std::string, Value>> scopeStack;
    std::map<std::string, FunctionDef> functions;

    void enterScope() {
        scopeStack.push_back(std::map<std::string, Value>());
    }

    void exitScope() {
        scopeStack.pop_back();
    }

    void setVariable(const std::string& name, const Value& value) {
        if (scopeStack.empty()) {
            globalVars[name] = value;
        } else {
            scopeStack.back()[name] = value;
        }
    }

    Value getVariable(const std::string& name) {
        if (!scopeStack.empty()) {
            auto& local = scopeStack.back();
            if (local.find(name) != local.end()) {
                return local[name];
            }
        }
        if (globalVars.find(name) != globalVars.end()) {
            return globalVars[name];
        }
        throw std::runtime_error("Variable not found: " + name);
    }

    bool hasVariable(const std::string& name) {
        if (!scopeStack.empty()) {
            auto& local = scopeStack.back();
            if (local.find(name) != local.end()) {
                return true;
            }
        }
        return globalVars.find(name) != globalVars.end();
    }

    Value convertType(const Value& v1, const Value& v2, bool& toFloat) {
        toFloat = (v1.type == ValueType::FLOAT || v2.type == ValueType::FLOAT);
        return Value();
    }

public:
    std::any visitFile_input(Python3Parser::File_inputContext *ctx) override;
    std::any visitFuncdef(Python3Parser::FuncdefContext *ctx) override;
    std::any visitParameters(Python3Parser::ParametersContext *ctx) override;
    std::any visitTypedargslist(Python3Parser::TypedargslistContext *ctx) override;
    std::any visitStmt(Python3Parser::StmtContext *ctx) override;
    std::any visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) override;
    std::any visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) override;
    std::any visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) override;
    std::any visitAugassign(Python3Parser::AugassignContext *ctx) override;
    std::any visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) override;
    std::any visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) override;
    std::any visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) override;
    std::any visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) override;
    std::any visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) override;
    std::any visitIf_stmt(Python3Parser::If_stmtContext *ctx) override;
    std::any visitWhile_stmt(Python3Parser::While_stmtContext *ctx) override;
    std::any visitSuite(Python3Parser::SuiteContext *ctx) override;
    std::any visitTest(Python3Parser::TestContext *ctx) override;
    std::any visitOr_test(Python3Parser::Or_testContext *ctx) override;
    std::any visitAnd_test(Python3Parser::And_testContext *ctx) override;
    std::any visitNot_test(Python3Parser::Not_testContext *ctx) override;
    std::any visitComparison(Python3Parser::ComparisonContext *ctx) override;
    std::any visitArith_expr(Python3Parser::Arith_exprContext *ctx) override;
    std::any visitTerm(Python3Parser::TermContext *ctx) override;
    std::any visitFactor(Python3Parser::FactorContext *ctx) override;
    std::any visitAtom_expr(Python3Parser::Atom_exprContext *ctx) override;
    std::any visitAtom(Python3Parser::AtomContext *ctx) override;
    std::any visitFormat_string(Python3Parser::Format_stringContext *ctx) override;
    std::any visitTrailer(Python3Parser::TrailerContext *ctx) override;
    std::any visitTestlist(Python3Parser::TestlistContext *ctx) override;
    std::any visitArglist(Python3Parser::ArglistContext *ctx) override;
    std::any visitArgument(Python3Parser::ArgumentContext *ctx) override;
};

#endif//PYTHON_INTERPRETER_EVALVISITOR_H
