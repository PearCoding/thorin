//#include "anydsl/dump.h"

#include "anydsl/airnode.h"
#include "anydsl/literal.h"
#include "anydsl/primop.h"
#include "anydsl/type.h"
#include "anydsl/util/for_all.h"

namespace anydsl {

class Dumper {
public:

    Dumper(std::ostream& o)
        : o(o)
    {}

    void dump(const AIRNode* n);
    void dumpBinOp(const std::string& str, const AIRNode* n);

    std::ostream& o;
};

void Dumper::dumpBinOp(const std::string& str, const AIRNode* n) {
    const BinOp* b = n->as<BinOp>();
    o << str << "("; 
    dump(b->ldef());
    o << ", ";
    dump(b->rdef());
    o << ")";
    return;
}

void Dumper::dump(const AIRNode* n) {
    std::string str;

    switch (n->index()) {
/*
 * types
 */

#define ANYDSL_U_TYPE(T) case Index_PrimType_##T: o << #T; return;
#define ANYDSL_F_TYPE(T) ANYDSL_U_TYPE(T)
#include "anydsl/tables/primtypetable.h"

        case Index_Sigma: {
            const Sigma* sigma = n->as<Sigma>();
            o << "sigma(";

            if (!sigma->ops().empty()) {
                for (Sigma::Ops::const_iterator i = sigma->ops().begin(), e = sigma->ops().end() - 1; i != e; ++i) {
                    dump(*i);
                    o << ", ";
                }

                dump(sigma->ops().back());
            }

            o << ')';
            return;
        }

        case Index_Pi: {
            const Pi* pi = n->as<Pi>();
            o << "pi(";
            dump(pi->sigma());
            o << ')';
            return;
        }

/*
 * literals
 */

#define ANYDSL_U_TYPE(T) case Index_PrimLit_##T: o << n->as<PrimLit>()->box().get_##T(); return;
#define ANYDSL_F_TYPE(T) ANYDSL_U_TYPE(T)
#include "anydsl/tables/primtypetable.h"

        case Index_Undef:    o << "<undef>"; return;
        case Index_ErrorLit: o << "<error>"; return;

/*
 * primops
 */

#define ANYDSL_ARITHOP(op) case Index_##op: return dumpBinOp(#op, n);
#include "anydsl/tables/arithoptable.h"

#define ANYDSL_RELOP(op)   case Index_##op: return dumpBinOp(#op, n);
#include "anydsl/tables/reloptable.h"

#define ANYDSL_CONVOP(op) case Index_##op:
#include "anydsl/tables/convoptable.h"
        ANYDSL_NOT_IMPLEMENTED;

        case Index_Proj:
            ANYDSL_NOT_IMPLEMENTED;

        case Index_Insert:
            ANYDSL_NOT_IMPLEMENTED;

        case Index_Select:
            ANYDSL_NOT_IMPLEMENTED;

        case Index_Jump:
            ANYDSL_NOT_IMPLEMENTED;

        case Index_Tuple:
            ANYDSL_NOT_IMPLEMENTED;

        case Index_NoRet: {
            const NoRet* noret = n->as<NoRet>();
            dump(noret->pi());
            return;
        }

/*
 * Param
 */
        case Index_Param:
            ANYDSL_NOT_IMPLEMENTED;

/*
 * Lambda
 */
        case Index_Lambda:
            ANYDSL_NOT_IMPLEMENTED;

        //default: ANYDSL_NOT_IMPLEMENTED;
    }
}

//------------------------------------------------------------------------------

void dump(const AIRNode* n, std::ostream& o /*= std::cout*/) {
    Dumper p(o);
    p.dump(n);
}

//------------------------------------------------------------------------------

} // namespace anydsl
