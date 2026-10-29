#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
// Only the explicitly added SoW operation is removed for historical descriptor
// comparisons. Full Runtime/MCP catalog comparisons keep the complete227 rows.
inline Monstey_Json Monstey_HistoricalCatalogWithoutAtomUses(const Monstey_Json &current) {
    Monstey_Require(current.is_array() && current.size()==227,
                    "Current catalog must contain exactly227 operations");
    auto old=Monstey_Json::array();
    unsigned found=0;
    for(const auto &row:current.as_array())
        if(row.at("name")==Monstey_Json("database_atom_uses")) {
            ++found;
            Monstey_Require(row.at("read_only").as_bool() &&
                            row.at("annotations").at("readOnlyHint").as_bool(),
                            "Added SoW operation must remain read-only");
        } else old.as_array().push_back(row);
    Monstey_Require(found==1 && old.size()==226,
                    "Historical projection must remove exactly one named SoW operation");
    return old;
}
}
