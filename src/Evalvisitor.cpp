#include "Evalvisitor.h"
#include "Python3Lexer.h"
#include <cmath>

std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
    for (auto stmt : ctx->stmt()) {
        visit(stmt);
    }
    return nullptr;
}

std::any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx) {
    std::string funcName = ctx->NAME()->getText();
    FunctionDef funcDef;
    funcDef.body = ctx->suite();
    funcDef.capturedGlobals = &globalVars;

    if (ctx->parameters()) {
        auto paramInfo = visit(ctx->parameters());
        if (paramInfo.has_value()) {
            auto params = std::any_cast<std::pair<std::vector<std::string>, std::vector<Value>>>(paramInfo);
            funcDef.params = params.first;
            funcDef.defaultValues = params.second;
        }
    }

    functions[funcName] = funcDef;
    return nullptr;
}

std::any EvalVisitor::visitParameters(Python3Parser::ParametersContext *ctx) {
    if (ctx->typedargslist()) {
        return visit(ctx->typedargslist());
    }
    return std::make_pair(std::vector<std::string>(), std::vector<Value>());
}

std::any EvalVisitor::visitTypedargslist(Python3Parser::TypedargslistContext *ctx) {
    std::vector<std::string> params;
    std::vector<Value> defaults;

    auto tfpdefs = ctx->tfpdef();
    auto tests = ctx->test();

    size_t defaultStartIdx = tfpdefs.size() - tests.size();

    for (size_t i = 0; i < tfpdefs.size(); i++) {
        params.push_back(tfpdefs[i]->NAME()->getText());
        if (i >= defaultStartIdx) {
            defaults.push_back(std::any_cast<Value>(visit(tests[i - defaultStartIdx])));
        }
    }

    return std::make_pair(params, defaults);
}

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx) {
    if (ctx->simple_stmt()) {
        return visit(ctx->simple_stmt());
    }
    if (ctx->compound_stmt()) {
        return visit(ctx->compound_stmt());
    }
    return nullptr;
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) {
    visit(ctx->small_stmt());
    return nullptr;
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) {
    if (ctx->expr_stmt()) {
        return visit(ctx->expr_stmt());
    }
    if (ctx->flow_stmt()) {
        return visit(ctx->flow_stmt());
    }
    return nullptr;
}

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) {
    auto tests = ctx->testlist();

    if (tests.size() == 1) {
        // Just expression evaluation
        visit(tests[0]);
        return nullptr;
    }

    if (ctx->augassign()) {
        // Augmented assignment
        std::string varName = tests[0]->test(0)->getText();
        Value currentVal = getVariable(varName);
        Value rhsVal = std::any_cast<Value>(visit(tests[1]));
        std::string op = ctx->augassign()->getText();

        Value result;
        bool isFloat = (currentVal.type == ValueType::FLOAT || rhsVal.type == ValueType::FLOAT);

        if (op == "+=") {
            if (isFloat) {
                double left = (currentVal.type == ValueType::FLOAT) ? currentVal.floatVal : currentVal.intVal.toDouble();
                double right = (rhsVal.type == ValueType::FLOAT) ? rhsVal.floatVal : rhsVal.intVal.toDouble();
                result = Value(left + right);
            } else if (currentVal.type == ValueType::STRING && rhsVal.type == ValueType::STRING) {
                result = Value(currentVal.stringVal + rhsVal.stringVal);
            } else {
                result = Value(currentVal.intVal + rhsVal.intVal);
            }
        } else if (op == "-=") {
            if (isFloat) {
                double left = (currentVal.type == ValueType::FLOAT) ? currentVal.floatVal : currentVal.intVal.toDouble();
                double right = (rhsVal.type == ValueType::FLOAT) ? rhsVal.floatVal : rhsVal.intVal.toDouble();
                result = Value(left - right);
            } else {
                result = Value(currentVal.intVal - rhsVal.intVal);
            }
        } else if (op == "*=") {
            if (isFloat) {
                double left = (currentVal.type == ValueType::FLOAT) ? currentVal.floatVal : currentVal.intVal.toDouble();
                double right = (rhsVal.type == ValueType::FLOAT) ? rhsVal.floatVal : rhsVal.intVal.toDouble();
                result = Value(left * right);
            } else if (currentVal.type == ValueType::STRING && rhsVal.type == ValueType::INT) {
                std::string repeated;
                long long times = std::stoll(rhsVal.intVal.toString());
                for (long long i = 0; i < times; i++) repeated += currentVal.stringVal;
                result = Value(repeated);
            } else {
                result = Value(currentVal.intVal * rhsVal.intVal);
            }
        } else if (op == "/=") {
            double left = (currentVal.type == ValueType::FLOAT) ? currentVal.floatVal : currentVal.intVal.toDouble();
            double right = (rhsVal.type == ValueType::FLOAT) ? rhsVal.floatVal : rhsVal.intVal.toDouble();
            result = Value(left / right);
        } else if (op == "//=") {
            if (isFloat) {
                double left = (currentVal.type == ValueType::FLOAT) ? currentVal.floatVal : currentVal.intVal.toDouble();
                double right = (rhsVal.type == ValueType::FLOAT) ? rhsVal.floatVal : rhsVal.intVal.toDouble();
                result = Value(std::floor(left / right));
            } else {
                result = Value(currentVal.intVal / rhsVal.intVal);
            }
        } else if (op == "%=") {
            if (isFloat) {
                double left = (currentVal.type == ValueType::FLOAT) ? currentVal.floatVal : currentVal.intVal.toDouble();
                double right = (rhsVal.type == ValueType::FLOAT) ? rhsVal.floatVal : rhsVal.intVal.toDouble();
                result = Value(left - std::floor(left / right) * right);
            } else {
                result = Value(currentVal.intVal % rhsVal.intVal);
            }
        }

        setVariable(varName, result);
        return nullptr;
    }

    // Regular assignment or chained assignment
    Value rhsValue = std::any_cast<Value>(visit(tests.back()));

    // Handle tuple unpacking or simple assignment
    for (int i = tests.size() - 2; i >= 0; i--) {
        auto testCtxs = tests[i]->test();
        if (testCtxs.size() == 1) {
            // Single variable assignment
            std::string varName = testCtxs[0]->getText();
            setVariable(varName, rhsValue);
        } else {
            // Multiple assignment (tuple unpacking)
            if (rhsValue.type == ValueType::TUPLE) {
                for (size_t j = 0; j < testCtxs.size() && j < rhsValue.tupleVal.size(); j++) {
                    std::string varName = testCtxs[j]->getText();
                    setVariable(varName, rhsValue.tupleVal[j]);
                }
            } else {
                // Should be a testlist on RHS too
                auto rhsTests = tests[i+1]->test();
                for (size_t j = 0; j < testCtxs.size() && j < rhsTests.size(); j++) {
                    std::string varName = testCtxs[j]->getText();
                    Value val = std::any_cast<Value>(visit(rhsTests[j]));
                    setVariable(varName, val);
                }
            }
        }
    }

    return nullptr;
}

std::any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx) {
    return nullptr;
}

std::any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) {
    if (ctx->break_stmt()) return visit(ctx->break_stmt());
    if (ctx->continue_stmt()) return visit(ctx->continue_stmt());
    if (ctx->return_stmt()) return visit(ctx->return_stmt());
    return nullptr;
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) {
    throw BreakException();
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) {
    throw ContinueException();
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) {
    if (ctx->testlist()) {
        auto tests = ctx->testlist()->test();
        if (tests.size() == 1) {
            Value val = std::any_cast<Value>(visit(tests[0]));
            throw ReturnException(val);
        } else {
            std::vector<Value> values;
            for (auto test : tests) {
                values.push_back(std::any_cast<Value>(visit(test)));
            }
            throw ReturnException(Value(values));
        }
    }
    throw ReturnException(Value());
}

std::any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) {
    if (ctx->if_stmt()) return visit(ctx->if_stmt());
    if (ctx->while_stmt()) return visit(ctx->while_stmt());
    if (ctx->funcdef()) return visit(ctx->funcdef());
    return nullptr;
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx) {
    auto tests = ctx->test();
    auto suites = ctx->suite();

    for (size_t i = 0; i < tests.size(); i++) {
        Value condition = std::any_cast<Value>(visit(tests[i]));
        if (condition.toBool()) {
            visit(suites[i]);
            return nullptr;
        }
    }

    if (suites.size() > tests.size()) {
        visit(suites.back());
    }

    return nullptr;
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx) {
    while (true) {
        Value condition = std::any_cast<Value>(visit(ctx->test()));
        if (!condition.toBool()) break;

        try {
            visit(ctx->suite());
        } catch (const BreakException&) {
            break;
        } catch (const ContinueException&) {
            continue;
        }
    }
    return nullptr;
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx) {
    if (ctx->simple_stmt()) {
        return visit(ctx->simple_stmt());
    }
    for (auto stmt : ctx->stmt()) {
        visit(stmt);
    }
    return nullptr;
}

std::any EvalVisitor::visitTest(Python3Parser::TestContext *ctx) {
    return visit(ctx->or_test());
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx) {
    auto andTests = ctx->and_test();
    Value result = std::any_cast<Value>(visit(andTests[0]));

    for (size_t i = 1; i < andTests.size(); i++) {
        if (result.toBool()) {
            return result;
        }
        result = std::any_cast<Value>(visit(andTests[i]));
    }

    return result;
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx) {
    auto notTests = ctx->not_test();
    Value result = std::any_cast<Value>(visit(notTests[0]));

    for (size_t i = 1; i < notTests.size(); i++) {
        if (!result.toBool()) {
            return result;
        }
        result = std::any_cast<Value>(visit(notTests[i]));
    }

    return result;
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx) {
    if (ctx->not_test()) {
        Value val = std::any_cast<Value>(visit(ctx->not_test()));
        return Value(!val.toBool());
    }
    return visit(ctx->comparison());
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx) {
    auto exprs = ctx->arith_expr();
    auto ops = ctx->comp_op();

    if (ops.empty()) {
        return visit(exprs[0]);
    }

    std::vector<Value> values;
    for (auto expr : exprs) {
        values.push_back(std::any_cast<Value>(visit(expr)));
    }

    for (size_t i = 0; i < ops.size(); i++) {
        Value left = values[i];
        Value right = values[i + 1];
        std::string op = ops[i]->getText();

        bool result = false;

        // Type conversion for comparison
        if (left.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
            double l = (left.type == ValueType::FLOAT) ? left.floatVal : left.intVal.toDouble();
            double r = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();

            if (op == "<") result = l < r;
            else if (op == ">") result = l > r;
            else if (op == "<=") result = l <= r;
            else if (op == ">=") result = l >= r;
            else if (op == "==") result = l == r;
            else if (op == "!=") result = l != r;
        } else if (left.type == ValueType::INT && right.type == ValueType::INT) {
            if (op == "<") result = left.intVal < right.intVal;
            else if (op == ">") result = left.intVal > right.intVal;
            else if (op == "<=") result = left.intVal <= right.intVal;
            else if (op == ">=") result = left.intVal >= right.intVal;
            else if (op == "==") result = left.intVal == right.intVal;
            else if (op == "!=") result = left.intVal != right.intVal;
        } else if (left.type == ValueType::STRING && right.type == ValueType::STRING) {
            if (op == "<") result = left.stringVal < right.stringVal;
            else if (op == ">") result = left.stringVal > right.stringVal;
            else if (op == "<=") result = left.stringVal <= right.stringVal;
            else if (op == ">=") result = left.stringVal >= right.stringVal;
            else if (op == "==") result = left.stringVal == right.stringVal;
            else if (op == "!=") result = left.stringVal != right.stringVal;
        } else if (op == "==" || op == "!=") {
            // Try to convert for equality comparison
            bool equal = false;
            if (left.type == right.type) {
                if (left.type == ValueType::BOOL) equal = left.boolVal == right.boolVal;
                else if (left.type == ValueType::NONE) equal = true;
            }
            result = (op == "==") ? equal : !equal;
        }

        if (!result) {
            return Value(false);
        }
    }

    return Value(true);
}

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx) {
    auto terms = ctx->term();
    Value result = std::any_cast<Value>(visit(terms[0]));

    auto ops = ctx->addorsub_op();
    for (size_t i = 0; i < ops.size(); i++) {
        Value right = std::any_cast<Value>(visit(terms[i + 1]));
        std::string op = ops[i]->getText();

        if (result.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
            double left = (result.type == ValueType::FLOAT) ? result.floatVal : result.intVal.toDouble();
            double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();

            if (op == "+") {
                result = Value(left + rightVal);
            } else {
                result = Value(left - rightVal);
            }
        } else if (op == "+" && result.type == ValueType::STRING && right.type == ValueType::STRING) {
            result = Value(result.stringVal + right.stringVal);
        } else if (result.type == ValueType::INT && right.type == ValueType::INT) {
            if (op == "+") {
                result = Value(result.intVal + right.intVal);
            } else {
                result = Value(result.intVal - right.intVal);
            }
        }
    }

    return result;
}

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx) {
    auto factors = ctx->factor();
    Value result = std::any_cast<Value>(visit(factors[0]));

    auto ops = ctx->muldivmod_op();
    for (size_t i = 0; i < ops.size(); i++) {
        Value right = std::any_cast<Value>(visit(factors[i + 1]));
        std::string op = ops[i]->getText();

        if (result.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
            double left = (result.type == ValueType::FLOAT) ? result.floatVal : result.intVal.toDouble();
            double rightVal = (right.type == ValueType::FLOAT) ? right.floatVal : right.intVal.toDouble();

            if (op == "*") {
                result = Value(left * rightVal);
            } else if (op == "/") {
                result = Value(left / rightVal);
            } else if (op == "//") {
                result = Value(std::floor(left / rightVal));
            } else if (op == "%") {
                result = Value(left - std::floor(left / rightVal) * rightVal);
            }
        } else if (op == "*" && result.type == ValueType::STRING && right.type == ValueType::INT) {
            std::string repeated;
            long long times = std::stoll(right.intVal.toString());
            for (long long i = 0; i < times; i++) repeated += result.stringVal;
            result = Value(repeated);
        } else if (op == "*" && result.type == ValueType::INT && right.type == ValueType::STRING) {
            std::string repeated;
            long long times = std::stoll(result.intVal.toString());
            for (long long i = 0; i < times; i++) repeated += right.stringVal;
            result = Value(repeated);
        } else if (result.type == ValueType::INT && right.type == ValueType::INT) {
            if (op == "*") {
                result = Value(result.intVal * right.intVal);
            } else if (op == "/") {
                result = Value(result.intVal.toDouble() / right.intVal.toDouble());
            } else if (op == "//") {
                result = Value(result.intVal / right.intVal);
            } else if (op == "%") {
                result = Value(result.intVal % right.intVal);
            }
        }
    }

    return result;
}

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx) {
    if (ctx->factor()) {
        Value val = std::any_cast<Value>(visit(ctx->factor()));

        if (ctx->ADD()) {
            return val;
        } else if (ctx->MINUS()) {
            if (val.type == ValueType::INT) {
                return Value(-val.intVal);
            } else if (val.type == ValueType::FLOAT) {
                return Value(-val.floatVal);
            }
        }
    }
    return visit(ctx->atom_expr());
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx) {
    Value result = std::any_cast<Value>(visit(ctx->atom()));

    if (ctx->trailer()) {
        auto trailerResult = visit(ctx->trailer());

        // Function call
        if (trailerResult.has_value()) {
            auto args = std::any_cast<std::pair<std::vector<Value>, std::map<std::string, Value>>>(trailerResult);

            std::string funcName = result.stringVal; // Function name from atom

            // Built-in functions
            if (funcName == "print") {
                for (size_t i = 0; i < args.first.size(); i++) {
                    if (i > 0) std::cout << " ";
                    if (args.first[i].type == ValueType::STRING) {
                        std::cout << args.first[i].stringVal;
                    } else {
                        std::cout << args.first[i].toString();
                    }
                }
                std::cout << std::endl;
                result = Value();
            } else if (funcName == "int") {
                Value arg = args.first[0];
                if (arg.type == ValueType::INT) result = arg;
                else if (arg.type == ValueType::FLOAT) result = Value(BigInteger((long long)arg.floatVal));
                else if (arg.type == ValueType::BOOL) result = Value(BigInteger(arg.boolVal ? 1 : 0));
                else if (arg.type == ValueType::STRING) result = Value(BigInteger(arg.stringVal));
            } else if (funcName == "float") {
                Value arg = args.first[0];
                if (arg.type == ValueType::FLOAT) result = arg;
                else if (arg.type == ValueType::INT) result = Value(arg.intVal.toDouble());
                else if (arg.type == ValueType::BOOL) result = Value(arg.boolVal ? 1.0 : 0.0);
                else if (arg.type == ValueType::STRING) result = Value(std::stod(arg.stringVal));
            } else if (funcName == "str") {
                Value arg = args.first[0];
                result = Value(arg.toString());
            } else if (funcName == "bool") {
                Value arg = args.first[0];
                result = Value(arg.toBool());
            } else {
                // User-defined function
                FunctionDef& func = functions[funcName];

                enterScope();

                // Handle default parameters
                size_t numRequiredParams = func.params.size() - func.defaultValues.size();

                // Positional arguments
                for (size_t i = 0; i < args.first.size(); i++) {
                    setVariable(func.params[i], args.first[i]);
                }

                // Keyword arguments
                for (auto& kv : args.second) {
                    setVariable(kv.first, kv.second);
                }

                // Default values
                for (size_t i = args.first.size(); i < func.params.size(); i++) {
                    if (!hasVariable(func.params[i])) {
                        size_t defaultIdx = i - numRequiredParams;
                        if (defaultIdx < func.defaultValues.size()) {
                            setVariable(func.params[i], func.defaultValues[defaultIdx]);
                        }
                    }
                }

                try {
                    visit(func.body);
                    result = Value();
                } catch (const ReturnException& e) {
                    result = e.value;
                }

                exitScope();
            }
        }
    }

    return result;
}

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx) {
    if (ctx->NUMBER()) {
        std::string numStr = ctx->NUMBER()->getText();
        if (numStr.find('.') != std::string::npos) {
            return Value(std::stod(numStr));
        } else {
            return Value(BigInteger(numStr));
        }
    }

    if (ctx->NAME()) {
        std::string name = ctx->NAME()->getText();
        if (name == "True") return Value(true);
        if (name == "False") return Value(false);
        if (name == "None") return Value();

        // Check if it's a built-in function
        if (name == "print" || name == "int" || name == "float" || name == "str" || name == "bool") {
            return Value(name);
        }

        // Check if it's a user-defined function name
        if (functions.find(name) != functions.end()) {
            return Value(name); // Return function name as string
        }

        return getVariable(name);
    }

    if (ctx->TRUE()) return Value(true);
    if (ctx->FALSE()) return Value(false);
    if (ctx->NONE()) return Value();

    if (ctx->STRING().size() > 0) {
        std::string result;

        for (auto str : ctx->STRING()) {
            std::string s = str->getText();

            // Remove quotes
            s = s.substr(1, s.length() - 2);
            result += s;
        }

        return Value(result);
    }

    if (ctx->format_string()) {
        return visit(ctx->format_string());
    }

    if (ctx->test()) {
        return visit(ctx->test());
    }

    return Value();
}

std::any EvalVisitor::visitFormat_string(Python3Parser::Format_stringContext *ctx) {
    std::string result;

    auto literals = ctx->FORMAT_STRING_LITERAL();
    auto testlists = ctx->testlist();

    size_t litIdx = 0;
    size_t testIdx = 0;

    // Process format string
    while (litIdx < literals.size() || testIdx < testlists.size()) {
        // Add string literal if present
        if (litIdx < literals.size()) {
            std::string lit = literals[litIdx]->getText();
            // Process escaped braces
            for (size_t i = 0; i < lit.length(); i++) {
                if (lit[i] == '{' && i + 1 < lit.length() && lit[i + 1] == '{') {
                    result += '{';
                    i++;
                } else if (lit[i] == '}' && i + 1 < lit.length() && lit[i + 1] == '}') {
                    result += '}';
                    i++;
                } else {
                    result += lit[i];
                }
            }
            litIdx++;
        }

        // Add expression value if present
        if (testIdx < testlists.size()) {
            auto tests = testlists[testIdx]->test();
            if (tests.size() > 0) {
                Value val = std::any_cast<Value>(visit(tests[0]));
                if (val.type == ValueType::STRING) {
                    result += val.stringVal;
                } else {
                    result += val.toString();
                }
            }
            testIdx++;
        }
    }

    return Value(result);
}

std::any EvalVisitor::visitTrailer(Python3Parser::TrailerContext *ctx) {
    if (ctx->arglist()) {
        return visit(ctx->arglist());
    }
    return std::make_pair(std::vector<Value>(), std::map<std::string, Value>());
}

std::any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx) {
    auto tests = ctx->test();
    if (tests.size() == 1) {
        return visit(tests[0]);
    }

    std::vector<Value> values;
    for (auto test : tests) {
        values.push_back(std::any_cast<Value>(visit(test)));
    }
    return Value(values);
}

std::any EvalVisitor::visitArglist(Python3Parser::ArglistContext *ctx) {
    std::vector<Value> positional;
    std::map<std::string, Value> keyword;

    for (auto arg : ctx->argument()) {
        auto argResult = visit(arg);
        auto argPair = std::any_cast<std::pair<std::string, Value>>(argResult);

        if (argPair.first.empty()) {
            positional.push_back(argPair.second);
        } else {
            keyword[argPair.first] = argPair.second;
        }
    }

    return std::make_pair(positional, keyword);
}

std::any EvalVisitor::visitArgument(Python3Parser::ArgumentContext *ctx) {
    auto tests = ctx->test();
    if (tests.size() == 2) {
        // Keyword argument
        std::string name = tests[0]->getText();
        Value val = std::any_cast<Value>(visit(tests[1]));
        return std::make_pair(name, val);
    } else {
        // Positional argument
        Value val = std::any_cast<Value>(visit(tests[0]));
        return std::make_pair(std::string(), val);
    }
}
