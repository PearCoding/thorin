#ifndef ANYDSL_LITERAL_H
#define ANYDSL_LITERAL_H

#include <vector>

#include "anydsl/defuse.h"
#include "anydsl/type.h"
#include "anydsl/util/box.h"

namespace anydsl {

class Type;
class World;

//------------------------------------------------------------------------------

class Literal : public Value {
protected:

    Literal(IndexKind index, const Type* type)
        : Value(index, type, 0)
    {}
};

//------------------------------------------------------------------------------

class Undef : public Literal {
private:

    Undef(const Type* type)
        : Literal(Index_Undef, type)
    {}

    friend class World;
};

//------------------------------------------------------------------------------

class ErrorLit : public Literal {
private:

    ErrorLit(const Type* type)
        : Literal(Index_ErrorLit, type)
    {}

    friend class World;
};

//------------------------------------------------------------------------------

class PrimLit : public Literal {
private:

    PrimLit(const Type* type, Box box)
        : Literal((IndexKind) type2lit(type->as<PrimType>()->kind()), type)
        , box_(box)
    {}

public:

    PrimLitKind kind() const { return (PrimLitKind) index(); }
    Box box() const { return box_; }

    virtual bool equal(const Value* other) const;
    virtual size_t hash() const;

private:

    Box box_;

    friend class World;
};

//------------------------------------------------------------------------------

} // namespace anydsl

#endif
