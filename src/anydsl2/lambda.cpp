#include "anydsl2/lambda.h"

#include <algorithm>

#include "anydsl2/type.h"
#include "anydsl2/primop.h"
#include "anydsl2/world.h"
#include "anydsl2/analyses/scope.h"
#include "anydsl2/util/array.h"
#include "anydsl2/util/for_all.h"

namespace anydsl2 {

Lambda::Lambda(size_t gid, const Pi* pi, uint32_t flags)
    : Def(Node_Lambda, pi)
    , gid_(gid)
    , flags_(flags)
{
    params_.reserve(pi->size());
}

Lambda::~Lambda() {
    for_all (param, params())
        delete param;
}

Lambda* Lambda::stub() const { 
    Lambda* result = world().lambda(pi(), flags());
    result->debug = debug;

    for (size_t i = 0, e = params().size(); i != e; ++i)
        result->param(i)->debug = param(i)->debug;

    return result;
}

const Pi* Lambda::pi() const { return type()->as<Pi>(); }
const Pi* Lambda::to_pi() const { return to()->type()->as<Pi>(); }

const Param* Lambda::append_param(const Type* type) {
    size_t size = pi()->size();

    Array<const Type*> elems(size + 1);
    *std::copy(pi()->elems().begin(), pi()->elems().end(), elems.begin()) = type;

    // update type
    set_type(world().pi(elems));

    // append new param
    const Param* param = new Param(type, this, size);
    params_.push_back(param);

    return param;
}

bool Lambda::equal(const Node* other) const { return this == other; }
size_t Lambda::hash() const { return boost::hash_value(this); }

static void find_lambdas(const Def* def, LambdaSet& result) {
    if (Lambda* lambda = def->isa_lambda()) {
        result.insert(lambda);
        return;
    }

    for_all (op, def->ops())
        find_lambdas(op, result);
}

template<bool direct>
inline static void find_preds(Use use, LambdaSet& result) {
    const Def* def = use.def();
    if (Lambda* lambda = def->isa_lambda()) {
        if (!direct || use.index() == 0)
            result.insert(lambda);
    } else {
        assert(def->isa<PrimOp>() && "not a PrimOp");

        for_all (use, def->uses())
            find_preds<direct>(use, result);
    }
}

LambdaSet Lambda::preds() const {
    LambdaSet result;

    for_all (use, uses())
        find_preds<false>(use, result);

    return result;
}

LambdaSet Lambda::direct_preds() const {
    LambdaSet result;

    for_all (use, uses())
        find_preds<true>(use, result);

    return result;
}

LambdaSet Lambda::targets() const {
    LambdaSet result;
    find_lambdas(to(), result);

    return result;
}

LambdaSet Lambda::hos() const {
    LambdaSet result;
    for_all (def, args())
        find_lambdas(def, result);

    return result;
}

LambdaSet Lambda::succs() const {
    LambdaSet result;
    for_all (def, ops())
        find_lambdas(def, result);

    return result;
}

template<bool fo>
Array<const Param*> Lambda::classify_params() const {
    Array<const Param*> res(params().size());

    size_t size = 0;
    for_all (param, params())
        if (fo ^ (param->type()->isa<Pi>() != 0))
            res[size++] = param;

    res.shrink(size);

    return res;
}

template<bool fo>
Array<const Def*> Lambda::classify_args() const {
    Array<const Def*> res(args().size());

    size_t size = 0;
    for_all (arg, args())
        if (fo ^ (arg->type()->isa<Pi>() != 0))
            res[size++] = arg;

    res.shrink(size);

    return res;
}

bool Lambda::is_cascading() const {
    if (uses().size() != 1)
        return false;

    Use use = *uses().begin();
    return use.def()->isa<Lambda>() && use.index() > 0;
}

Array<const Param*> Lambda::fo_params() const { return classify_params<true>(); }
Array<const Param*> Lambda::ho_params() const { return classify_params<false>(); }
Array<const Def*> Lambda::fo_args() const { return classify_args<true>(); }
Array<const Def*> Lambda::ho_args() const { return classify_args<false>(); }
bool Lambda::is_fo()  const { return pi()->is_fo(); }
bool Lambda::is_ho() const { return pi()->is_ho(); }

void Lambda::jump(const Def* to, ArrayRef<const Def*> args) {
    alloc(args.size() + 1);
    set_op(0, to);

    size_t x = 1;
    for_all (arg, args)
        set_op(x++, arg);
}

void Lambda::branch(const Def* cond, const Def* tto, const Def*  fto) {
    return jump(world().select(cond, tto, fto), ArrayRef<const Def*>(0, 0));
}

Lambda* Lambda::drop(size_t i, const Def* with, bool self) {
    const Def* awith[] = { with };
    size_t indices[] = { i };

    return drop(indices, awith, self);
}

Lambda* Lambda::drop(ArrayRef<const Def*> with, bool self) {
    Array<size_t> indices(with.size());
    for (size_t i = 0, e = indices.size(); i < e; ++i)
        indices[i] = i;

    return drop(indices, with, self);
}

class Dropper {
public:

    typedef boost::unordered_map<const Def*, const Def*> Old2New;
    typedef boost::unordered_set<const Def*> Cached;

    Dropper(Lambda* olambda, ArrayRef<size_t> indices, ArrayRef<const Def*> with, bool self)
        : scope(olambda)
        , indices(indices)
        , with(with)
        , world(olambda->world())
        , self(self)
    {}

    Lambda* drop();
    void drop_body(Lambda* olambda, Lambda* nlambda);
    const Def* drop(const Def* odef);
    const Def* drop(bool& is_new, const Def* odef);

    Scope scope;
    ArrayRef<size_t> indices;
    ArrayRef<const Def*> with;
    World& world;
    bool self;
    Lambda* nentry;
    Lambda* oentry;
    Old2New old2new;
    Cached cached;
};

Lambda* Lambda::drop(ArrayRef<size_t> indices, ArrayRef<const Def*> with, bool self) {
    Dropper dropper(this, indices, with, self);
    return dropper.drop();
}

Lambda* Dropper::drop() {
    oentry = scope.entry();
    const Pi* o_pi = oentry->pi();

    size_t o_numparams = o_pi->size();
    size_t numdrop = indices.size();
    size_t n_numparams = o_numparams - numdrop;

    Array<const Type*> nelems(n_numparams);

    // TODO generics
    for (size_t oe = 0, i = 0, ne = 0; oe < o_numparams; ++oe) {
        if (i < o_numparams && indices[i] == oe)
            ++i;
        else
            nelems[ne++] = o_pi->elem(oe);
    }

    const Pi* n_pi = world.pi(nelems);
    nentry = world.lambda(n_pi);
    nentry->debug = oentry->debug + ".dropped";

    // put in params for entry (oentry)
    // op -> iterates over old params
    // np -> iterates over new params
    //  i -> iterates over indices
    for (size_t op = 0, np = 0, i = 0; op < o_numparams; ++op) {
        const Param* oparam = oentry->param(op);
        if (i < indices.size() && indices[i] == op)
            old2new[oparam] = with[i++];
        else {
            const Param* nparam = nentry->param(np++);
            nparam->debug = oparam->debug + ".dropped";
            old2new[oparam] = nparam;
        }
    }

    // create stubs for all other lambdas and put their params into the map
    for_all (olambda, scope.rpo().slice_back(1)) {
        Lambda* nlambda = olambda->stub();
        nlambda->debug += ".dropped";
        old2new[olambda] = nlambda;

        for (size_t i = 0, e = nlambda->params().size(); i != e; ++i) {
            old2new[olambda->param(i)] = nlambda->param(i);
            nlambda->param(i)->debug += ".dropped";
        }
    }

    drop_body(oentry, nentry);

    for_all (cur, scope.rpo().slice_back(1))
        drop_body(cur, old2new[cur]->as_lambda());

    return nentry;
}

void Dropper::drop_body(Lambda* olambda, Lambda* nlambda) {
    Array<const Def*> ops(olambda->ops().size());
    for (size_t i = 0, e = ops.size(); i != e; ++i)
        ops[i] = drop(olambda->op(i));

    ArrayRef<const Def*> nargs(ops.slice_back(1));  // new args of nlambda
    const Def* ntarget = ops.front();               // new target of nlambda

    // check whether we can optimize tail recursion
    if (self && ntarget == oentry) {
        bool substitute = true;
        for (size_t i = 0, e = indices.size(); i != e && substitute; ++i)
            substitute &= nargs[indices[i]] == with[i];

        if (substitute) {                           // yes, we can
            // sargs -> substituted args for tail call optimization
            Array<const Def*> sargs(nargs.size() - indices.size());

            // na -> iterates over nargs
            // sa -> iterates over sargs
            //  i -> iterates over  indices
            for (size_t na = 0, sa = 0, i = 0, e = nargs.size(); na != e; ++na) {
                if (i < indices.size() && indices[i] == na)
                    ++i;
                else
                    sargs[sa++] = nargs[na];
            }

            nlambda->jump(nentry, sargs);
            return;
        }
    }

    nlambda->jump(ntarget, nargs);
}

const Def* Dropper::drop(const Def* odef) {
    bool ignore;
    return drop(ignore, odef);
}

const Def* Dropper::drop(bool& is_new, const Def* odef) {
    Old2New::iterator o_iter = old2new.find(odef);
    if (o_iter != old2new.end()) {
        is_new = true;
        return o_iter->second;
    }

    Cached::iterator c_iter = cached.find(odef);
    if (c_iter != cached.end()) {
        assert(odef == *c_iter);
        is_new = false;
        return odef;
    }

    if (odef->isa<Lambda>() || odef->isa<Param>()) {
        cached.insert(odef);
        is_new = false;
        return odef;
    }

    const PrimOp* oprimop = odef->as<PrimOp>();

    Array<const Def*> nops(oprimop->size());
    size_t i = 0;
    is_new = false;
    for_all (op, oprimop->ops()) {
        bool op_is_new;
        nops[i++] = drop(op_is_new, op);
        is_new |= op_is_new;
    }

    if (is_new)
        return old2new[oprimop] = world.primop(oprimop->kind(), oprimop->type(), nops);

    cached.insert(oprimop);
    return odef;
}

} // namespace anydsl2
