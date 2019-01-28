#include "thorin/transform/mangle.h"

#include "thorin/primop.h"
#include "thorin/world.h"
#include "thorin/analyses/scope.h"

namespace thorin {

Mangler::Mangler(const Scope& scope, Defs args, Defs lift)
    : scope_(scope)
    , args_(args)
    , old_entry_(scope.entry())
    , defs_(scope.defs().capacity())
    , old2new_(scope.defs().capacity())
{
    assert(!old_entry()->is_empty());
    assert(args.size() == old_entry()->num_params());

    // TODO correctly deal with lams here
    std::queue<const Def*> queue;
    auto enqueue = [&](const Def* def) {
        if (!within(def)) {
            defs_.insert(def);
            queue.push(def);
        }
    };

    for (auto def : lift)
        enqueue(def);

    while (!queue.empty()) {
        for (auto use : pop(queue)->uses())
            enqueue(use);
    }
}

Lam* Mangler::mangle() {
    // create new_entry - but first collect and specialize all param types
    std::vector<const Def*> param_types;
    for (size_t i = 0, e = old_entry()->num_params(); i != e; ++i) {
        if (is_top(args_[i]))
            param_types.emplace_back(old_entry()->param(i)->type());
    }

    auto cn = world().cn(param_types);
    new_entry_ = world().lam(cn, old_entry()->debug_history());

    // map params
    old2new_[old_entry()] = old_entry();
    for (size_t i = 0, j = 0, e = old_entry()->num_params(); i != e; ++i) {
        auto old_param = old_entry()->param(i);
        if (!is_top(args_[i]))
            old2new_[old_param] = args_[i];
        else {
            auto new_param = new_entry()->param(j++);
            old2new_[old_param] = new_param;
            new_param->debug().set(old_param->name());
        }
    }

    // map filter
    Array<const Def*> new_filter(new_entry()->num_params());
    size_t j = 0;
    for (size_t i = 0, e = old_entry()->num_params(); i != e; ++i) {
        if (is_top(args_[i]))
            new_filter[j++] = mangle(old_entry()->filter(i));
    }

    new_entry()->set_filter(new_filter);
    new_entry()->set_body(mangle(old_entry()->body()));

    return new_entry();
}

const Def* Mangler::mangle(const Def* old_def) {
    // TODO merge with importer
    if (auto new_def = find(old2new_, old_def)) return new_def;
    if (!within(old_def)) return old_def;

    auto new_type = mangle(old_def->type());

    const Def* new_def = nullptr;
    if (old_def->is_nominal()) {
        new_def = old_def->stub(world(), new_type);
        old2new_[old_def] = new_def;
    }

    size_t size = old_def->num_ops();
    Array<const Def*> new_ops(size);
    for (size_t i = 0; i != size; ++i) {
        new_ops[i] = mangle(old_def->op(i));
        assert(&new_ops[i]->world() == &world());
    }

    if (new_def) {
        for (size_t i = 0; i != size; ++i)
            const_cast<Def*>(new_def)->set(i, new_ops[i]);
    } else {
        // check whether we can optimize tail recursion
        if (auto app = old_def->isa<App>()) {
            assert(new_ops.size() == 2);

            if (app->callee() == old_entry()) {
                auto new_args = new_ops[1]->isa<Tuple>() ? new_ops[1]->as<Tuple>()->ops() : new_ops.skip_front();
                assert(new_args.size() == args_.size());

                std::vector<size_t> cut;
                bool substitute = true;
                for (size_t i = 0, e = args_.size(); i != e && substitute; ++i) {
                    if (!is_top(args_[i])) {
                        substitute &= args_[i] == new_args[i];
                        cut.push_back(i);
                    }
                }

                if (substitute) {
                    // TODO lifting
                    //const auto& args = concat(new_args.cut(cut), new_entry()->params().get_back(lift_.size()));
                    auto args = new_args.cut(cut);
                    return world().app(new_entry(), args, app->debug());
                }
            }
        }

        new_def = old_def->rebuild(world(), new_type, new_ops);
        old2new_[old_def] = new_def;
    }

    return new_def;
}

//------------------------------------------------------------------------------

Lam* mangle(const Scope& scope, Defs args, Defs lift) {
    return Mangler(scope, args, lift).mangle();
}

Lam* drop(const App* app) {
    Scope scope(app->callee()->as_lam());
    return drop(scope, app->args());
}

//------------------------------------------------------------------------------

}
