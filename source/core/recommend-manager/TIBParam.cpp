#include "TIBParam.h"

namespace sf1r
{

TIBParam::TIBParam()
    : limit(0)
    , minFreq(0)
{}

bool TIBParam::check(std::string& errorMsg) const
{
    if (limit <= 0)
    {
        errorMsg = "Require a positive value in request[resource][max_count].";
        return false;
    }

    return true;
}

} // namespace sf1r
