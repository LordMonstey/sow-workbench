#pragma once
#include "Monstey_GadbManagerFree.hpp"
#include "Monstey_BackendReference.hpp"
#include <variant>
namespace Monstey_FreeReview {
using Monstey_U64=std::uint64_t;
using Monstey_U32=std::uint32_t;
using Monstey_Image=Monstey_PlacementReview::Monstey_Image;
using Monstey_Raw=Monstey_PlacementReview::Monstey_Raw;
using Monstey_Services=Monstey::Monstey_GadbManagerFreeServices;
using Monstey_Bindings=Monstey::Monstey_GadbManagerFreeBindings;
enum class Monstey_Operation {dispatch,thunk,backend,region,trim,segments,find,link,trap};
struct Monstey_Dispatch {Monstey_U64 manager,payload;Monstey::Monstey_GadbManagerFreeContext context;};
struct Monstey_Backend {Monstey_U64 backend,payload;Monstey::Monstey_GadbManagerFreeBody body;};
struct Monstey_Thunk {Monstey_U64 backend,payload;Monstey::Monstey_GadbManagerFreeEntry entry;};
struct Monstey_Region {Monstey_U64 address,length,frame;};
struct Monstey_Trim {Monstey_U64 backend,frame,incoming_d;};
struct Monstey_Segments {Monstey_U64 backend,frame,incoming_d;};
struct Monstey_Find {Monstey_U64 backend,address;};
struct Monstey_Link {Monstey_U64 backend,segment;};
struct Monstey_Trap {Monstey_U64 entry;Monstey_Raw incoming;Monstey_U64 r8,r9;};
using Monstey_Request=std::variant<Monstey_Dispatch,Monstey_Backend,Monstey_Thunk,Monstey_Region,Monstey_Trim,Monstey_Segments,Monstey_Find,Monstey_Link,Monstey_Trap>;
struct Monstey_BoundaryFault:std::runtime_error {
 Monstey::Monstey_GadbManagerFreeImport kind;
 Monstey::Monstey_GadbManagerBackendImportSite site;
 Monstey::Monstey_GadbManagerFreeImportArguments arguments;
 Monstey_BoundaryFault(Monstey::Monstey_GadbManagerFreeImport,
 const Monstey::Monstey_GadbManagerBackendImportSite&,
 Monstey::Monstey_GadbManagerFreeImportArguments);
};
struct Monstey_DivideFault:std::runtime_error {Monstey_U64 site,body,high,low,divisor;Monstey_DivideFault(Monstey_U64,Monstey_U64,Monstey_U64,Monstey_U64);};
// Independent entry DTO only; no physical CPU context, whole-helper provider or
// Free product implementation. Canonical provider shapes are reused verbatim.
Monstey_Raw Monstey_Run(unsigned mode,Monstey_Image,Monstey_Operation,
 const Monstey_Request&,const Monstey_Services&,Monstey_Bindings={});
}
