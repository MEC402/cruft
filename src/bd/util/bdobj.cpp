//
// Created by jim on 4/14/15.
//

#include <bd/util/bdobj.h>

namespace bd
{
unsigned int BDObj::counter = 0;

BDObj::BDObj()
    : BDObj("bd_"+std::to_string(counter))
{
    counter++;
}

BDObj::BDObj(const std::string &id)
    : ID( std::string( "{" + id + "}" ) )
{
}

BDObj::~BDObj()
{
}

void BDObj::id(const std::string &id)
{
    ID = id;
}

std::string BDObj::to_string() const
{
    return ID;
}



} // namespace bd