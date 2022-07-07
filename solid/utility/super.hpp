#pragma once
#include <functional>
#include <typeindex>

namespace solid {
using SuperFindFunctionT = std::function<bool(const std::type_index&, size_t&)>;

struct SuperBase {
    virtual ~SuperBase();
    virtual bool superFind(std::type_index& _rtype_index, size_t& _rid, const SuperFindFunctionT& _find_fnc) const;
};

template <class T, class S = SuperBase>
struct Super : S {

    using SuperT = S;

    template <typename... Args>
    explicit Super(Args&&... _args)
        : S(std::forward<Args>(_args)...)
    {
    }

    bool superFind(std::type_index& _rtype_index, size_t& _rid, const SuperFindFunctionT& _find_fnc) const override
    {
        {
            const std::type_index ti(typeid(T));
            if (_find_fnc(ti, _rid)) {
                _rtype_index = ti;
                return true;
            }
        }
        return SuperT::superFind(_rtype_index, _rid, _find_fnc);
    }
};
} // namespace solid
