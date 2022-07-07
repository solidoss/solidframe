#include "solid/utility/super.hpp"

namespace solid {
/*virtual*/ SuperBase::~SuperBase() {}

/*virtual*/ bool SuperBase::superFind(std::type_index& _rtype_index, size_t& _rid, const SuperFindFunctionT& _find_fnc) const
{
    {
        const std::type_index ti(typeid(SuperBase));
        if (_find_fnc(ti, _rid)) {
            _rtype_index = ti;
            return true;
        }
    }
    return false;
}
} // namespace solid
