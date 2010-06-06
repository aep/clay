#include "clay.hpp"

vector<OverloadPtr> typeOverloads;

void addTypeOverload(OverloadPtr x)
{
    typeOverloads.insert(typeOverloads.begin(), x);
}

void initTypeOverloads(TypePtr t)
{
    assert(!t->overloadsInitialized);

    for (unsigned i = 0; i < typeOverloads.size(); ++i) {
        OverloadPtr x = typeOverloads[i];
        EnvPtr env = new Env(x->env);
        const vector<IdentifierPtr> &pvars = x->code->patternVars;
        for (unsigned i = 0; i < pvars.size(); ++i) {
            PatternCellPtr cell = new PatternCell(pvars[i], NULL);
            addLocal(env, pvars[i], cell.ptr());
        }
        PatternPtr pattern = evaluatePattern(x->target, env);
        if (unify(pattern, t.ptr()))
            t->overloads.push_back(x);
    }

    switch (t->typeKind) {
    case ARRAY_TYPE :
        initBuiltinConstructor((ArrayType *)t.ptr());
        break;
    case TUPLE_TYPE :
        initBuiltinConstructor((TupleType *)t.ptr());
        break;
    case RECORD_TYPE :
        initBuiltinConstructor((RecordType *)t.ptr());
        break;
    }

    t->overloadsInitialized = true;
}

void initBuiltinConstructor(ArrayTypePtr t)
{
    CodePtr code = new Code();
    unsigned size = t->size;
    vector<IdentifierPtr> argNames;
    for (unsigned i = 0; i < size; ++i) {
        ostringstream sout;
        sout << "arg" << i;
        IdentifierPtr argName = new Identifier(sout.str());
        argNames.push_back(argName);
        ExprPtr type = new ObjectExpr(t->elementType.ptr());
        FormalArgPtr arg = new FormalArg(argName, type);
        code->formalArgs.push_back(arg.ptr());
    }
    ExprPtr retType = new ObjectExpr(t.ptr());
    IdentifierPtr retName = new Identifier("%ret");
    code->returnSpecs.push_back(new ReturnSpec(false, retType, retName));

    BlockPtr body = new Block();
    for (unsigned i = 0; i < size; ++i) {
        IndexingPtr left = new Indexing(new NameRef(retName));
        ExprPtr indexExpr = new ObjectExpr(sizeTToValueHolder(i).ptr());
        left->args.push_back(indexExpr);
        ExprPtr right = new NameRef(argNames[i]);
        StatementPtr stmt = new InitAssignment(left.ptr(), right);
        body->statements.push_back(stmt);
    }
    code->body = body.ptr();

    OverloadPtr overload = new Overload(retType, code, true);
    overload->env = new Env();
    t->overloads.push_back(overload);
}

void initBuiltinConstructor(TupleTypePtr t)
{
    CodePtr code = new Code();
    const vector<TypePtr> &elementTypes = t->elementTypes;
    vector<IdentifierPtr> argNames;
    for (unsigned i = 0; i < elementTypes.size(); ++i) {
        ostringstream sout;
        sout << "arg" << i;
        IdentifierPtr argName = new Identifier(sout.str());
        argNames.push_back(argName);
        ExprPtr type = new ObjectExpr(elementTypes[i].ptr());
        FormalArgPtr arg = new FormalArg(argName, type);
        code->formalArgs.push_back(arg.ptr());
    }
    ExprPtr retType = new ObjectExpr(t.ptr());
    IdentifierPtr retName = new Identifier("%ret");
    code->returnSpecs.push_back(new ReturnSpec(false, retType, retName));

    BlockPtr body = new Block();
    for (unsigned i = 0; i < elementTypes.size(); ++i) {
        ExprPtr left = new TupleRef(new NameRef(retName), i);
        ExprPtr right = new NameRef(argNames[i]);
        StatementPtr stmt = new InitAssignment(left, right);
        body->statements.push_back(stmt);
    }
    code->body = body.ptr();

    OverloadPtr overload = new Overload(retType, code, true);
    overload->env = new Env();
    t->overloads.push_back(overload);
}

void initBuiltinConstructor(RecordTypePtr t)
{
    CodePtr code = new Code();
    const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
    vector<IdentifierPtr> argNames;
    for (unsigned i = 0; i < fieldTypes.size(); ++i) {
        ostringstream sout;
        sout << "arg" << i;
        IdentifierPtr argName = new Identifier(sout.str());
        argNames.push_back(argName);
        ExprPtr type = new ObjectExpr(fieldTypes[i].ptr());
        FormalArgPtr arg = new FormalArg(argName, type);
        code->formalArgs.push_back(arg.ptr());
    }
    ExprPtr retType = new ObjectExpr(t.ptr());
    IdentifierPtr retName = new Identifier("%ret");
    code->returnSpecs.push_back(new ReturnSpec(false, retType, retName));

    BlockPtr body = new Block();
    for (unsigned i = 0; i < fieldTypes.size(); ++i) {
        CallPtr left = new Call(primNameRef("recordFieldRef"));
        left->args.push_back(new NameRef(retName));
        ExprPtr indexExpr = new ObjectExpr(sizeTToValueHolder(i).ptr());
        ExprPtr indexExpr2 = new StaticExpr(indexExpr);
        left->args.push_back(indexExpr2);
        ExprPtr right = new NameRef(argNames[i]);
        StatementPtr stmt = new InitAssignment(left.ptr(), right);
        body->statements.push_back(stmt);
    }
    code->body = body.ptr();

    OverloadPtr overload = new Overload(retType, code, true);
    overload->env = new Env();
    t->overloads.push_back(overload);
}

void initBuiltinConstructor(RecordPtr x)
{
    assert(!(x->builtinOverloadInitialized));
    x->builtinOverloadInitialized = true;

    assert(x->patternVars.size() > 0);

    ExprPtr recName = new NameRef(x->name);
    recName->location = x->name->location;

    CodePtr code = new Code();
    code->location = x->location;
    code->patternVars = x->patternVars;

    for (unsigned i = 0; i < x->fields.size(); ++i) {
        RecordFieldPtr f = x->fields[i];
        FormalArgPtr arg = new FormalArg(f->name, f->type);
        arg->location = f->location;
        code->formalArgs.push_back(arg.ptr());
    }

    IndexingPtr retType = new Indexing(recName);
    retType->location = x->location;
    for (unsigned i = 0; i < x->patternVars.size(); ++i) {
        ExprPtr typeArg = new NameRef(x->patternVars[i]);
        typeArg->location = x->patternVars[i]->location;
        retType->args.push_back(typeArg);
    }

    CallPtr returnExpr = new Call(retType.ptr());
    returnExpr->location = x->location;
    for (unsigned i = 0; i < x->fields.size(); ++i) {
        ExprPtr callArg = new NameRef(x->fields[i]->name);
        callArg->location = x->fields[i]->location;
        returnExpr->args.push_back(callArg);
    }

    vector<bool> isRef; isRef.push_back(false);
    vector<ExprPtr> exprs; exprs.push_back(returnExpr.ptr());
    code->body = new Return(isRef, exprs);
    code->body->location = returnExpr->location;

    OverloadPtr defaultOverload = new Overload(recName, code, true);
    defaultOverload->location = x->location;
    defaultOverload->env = x->env;
    x->overloads.push_back(defaultOverload);
}
