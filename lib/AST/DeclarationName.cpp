//===-- DeclarationName.cpp - Declaration names implementation --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the DeclarationName and DeclarationNameTable
// classes.
//
//===----------------------------------------------------------------------===//
#include "clang/AST/DeclarationName.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "clang/AST/TypeLoc.h"
#include "clang/AST/TypeOrdering.h"
#include "clang/Basic/IdentifierTable.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;

namespace clang {
/// CXXSpecialName - Records the type associated with one of the
/// "special" kinds of declaration names in C++, e.g., constructors,
/// destructors, and conversion functions.
class CXXSpecialName
  : public DeclarationNameExtra, public llvm::FoldingSetNode {
public:
  /// Type - The type associated with this declaration name.
  QualType Type;

  /// FETokenInfo - Extra information associated with this declaration
  /// name that can be used by the front end.
  void *FETokenInfo;

  void Profile(llvm::FoldingSetNodeID &ID) {
    ID.AddInteger(ExtraKindOrNumArgs);
    ID.AddPointer(Type.getAsOpaquePtr());
  }
};

/// CXXOperatorIdName - Contains extra information for the name of an
/// overloaded operator in C++, such as "operator+.
class CXXOperatorIdName : public DeclarationNameExtra {
public:
  /// FETokenInfo - Extra information associated with this operator
  /// name that can be used by the front end.
  void *FETokenInfo;
};

/// CXXLiteralOperatorName - Contains the actual identifier that makes up the
/// name.
///
/// This identifier is stored here rather than directly in DeclarationName so as
/// to allow Objective-C selectors, which are about a million times more common,
/// to consume minimal memory.
class CXXLiteralOperatorIdName
  : public DeclarationNameExtra, public llvm::FoldingSetNode {
public:
  IdentifierInfo *ID;

  /// FETokenInfo - Extra information associated with this operator
  /// name that can be used by the front end.
  void *FETokenInfo;

  void Profile(llvm::FoldingSetNodeID &FSID) {
    FSID.AddPointer(ID);
  }
};

static int compareInt(unsigned A, unsigned B) {
  return (A < B ? -1 : (A > B ? 1 : 0));
}

static int comparePtr(void const *A, void const *B) {
  return (A < B ? -1 : (A > B ? 1 : 0));
}

int DeclarationName::compare(DeclarationName LHS, DeclarationName RHS) {
  if (LHS.getNameKind() != RHS.getNameKind())
    return (LHS.getNameKind() < RHS.getNameKind() ? -1 : 1);
  
  switch (LHS.getNameKind()) {
  case DeclarationName::Identifier: {
    IdentifierInfo *LII = LHS.getAsIdentifierInfo();
    IdentifierInfo *RII = RHS.getAsIdentifierInfo();
    if (!LII) return RII ? -1 : 0;
    if (!RII) return 1;
    
    return LII->getName().compare(RII->getName());
  }

  case DeclarationName::ObjCZeroArgSelector:
  case DeclarationName::ObjCOneArgSelector:
  case DeclarationName::ObjCMultiArgSelector: {
    Selector LHSSelector = LHS.getObjCSelector();
    Selector RHSSelector = RHS.getObjCSelector();
    unsigned LN = LHSSelector.getNumArgs(), RN = RHSSelector.getNumArgs();
    for (unsigned I = 0, N = std::min(LN, RN); I != N; ++I) {
      switch (LHSSelector.getNameForSlot(I).compare(
                                               RHSSelector.getNameForSlot(I))) {
      case -1: return true;
      case 1: return false;
      default: break;
      }
    }

    return compareInt(LN, RN);
  }
  
  case DeclarationName::CXXConstructorName:
  case DeclarationName::CXXDestructorName:
  case DeclarationName::CXXConversionFunctionName:
    if (QualTypeOrdering()(LHS.getCXXNameType(), RHS.getCXXNameType()))
      return -1;
    if (QualTypeOrdering()(RHS.getCXXNameType(), LHS.getCXXNameType()))
      return 1;
    return 0;
              
  case DeclarationName::CXXOperatorName:
    return compareInt(LHS.getCXXOverloadedOperator(),
                      RHS.getCXXOverloadedOperator());

  case DeclarationName::CXXLiteralOperatorName:
    return LHS.getCXXLiteralIdentifier()->getName().compare(
                                   RHS.getCXXLiteralIdentifier()->getName());
              
  case DeclarationName::CXXUsingDirective:
    return 0;

  case DeclarationName::CXXTemplatedName:
    return comparePtr(LHS.getAsCXXTemplateDeclNameParmName(),
                      RHS.getAsCXXTemplateDeclNameParmName());

  case DeclarationName::SubstTemplatedName:
    return comparePtr(LHS.getAsSubstTemplateDeclNameParmName(),
                      RHS.getAsSubstTemplateDeclNameParmName());

  case DeclarationName::SubstTemplatedPackName:
    // FIXME: is this ok?
    return 0;
  }

  llvm_unreachable("Invalid DeclarationName Kind!");
}

static void printCXXConstructorDestructorName(QualType ClassType,
                                              raw_ostream &OS,
                                              PrintingPolicy Policy) {
  // We know we're printing C++ here. Ensure we print types properly.
  Policy.adjustForCPlusPlus();

  if (const RecordType *ClassRec = ClassType->getAs<RecordType>()) {
    OS << *ClassRec->getDecl();
    return;
  }
  if (Policy.SuppressTemplateArgsInCXXConstructors) {
    if (auto *InjTy = ClassType->getAs<InjectedClassNameType>()) {
      OS << *InjTy->getDecl();
      return;
    }
  }
  ClassType.print(OS, Policy);
}

void DeclarationName::print(raw_ostream &OS, const PrintingPolicy &Policy) {
  DeclarationName &N = *this;
  switch (N.getNameKind()) {
  case DeclarationName::Identifier:
    if (const IdentifierInfo *II = N.getAsIdentifierInfo())
      OS << II->getName();
    return;

  case DeclarationName::ObjCZeroArgSelector:
  case DeclarationName::ObjCOneArgSelector:
  case DeclarationName::ObjCMultiArgSelector:
    N.getObjCSelector().print(OS);
    return;

  case DeclarationName::CXXConstructorName:
    return printCXXConstructorDestructorName(N.getCXXNameType(), OS, Policy);

  case DeclarationName::CXXDestructorName: {
    OS << '~';
    return printCXXConstructorDestructorName(N.getCXXNameType(), OS, Policy);
  }

  case DeclarationName::CXXOperatorName: {
    static const char* const OperatorNames[NUM_OVERLOADED_OPERATORS] = {
      nullptr,
#define OVERLOADED_OPERATOR(Name,Spelling,Token,Unary,Binary,MemberOnly) \
      Spelling,
#include "clang/Basic/OperatorKinds.def"
    };
    const char *OpName = OperatorNames[N.getCXXOverloadedOperator()];
    assert(OpName && "not an overloaded operator");

    OS << "operator";
    if (OpName[0] >= 'a' && OpName[0] <= 'z')
      OS << ' ';
    OS << OpName;
    return;
  }

  case DeclarationName::CXXLiteralOperatorName:
    OS << "operator\"\"" << N.getCXXLiteralIdentifier()->getName();
    return;

  case DeclarationName::CXXConversionFunctionName: {
    OS << "operator ";
    QualType Type = N.getCXXNameType();
    if (const RecordType *Rec = Type->getAs<RecordType>()) {
      OS << *Rec->getDecl();
      return;
    }
    // We know we're printing C++ here, ensure we print 'bool' properly.
    PrintingPolicy CXXPolicy = Policy;
    CXXPolicy.adjustForCPlusPlus();
    Type.print(OS, CXXPolicy);
    return;
  }
  case DeclarationName::CXXUsingDirective:
    OS << "<using-directive>";
    return;

  case DeclarationName::CXXTemplatedName: {
    TemplateDeclNameParmDecl *TDP = N.getCXXTemplatedNameParmDecl();
    //if (IdentifierInfo *Id = TDP->getIdentifier())
    //  OS << Id->getName();
    //else
      OS << "declname-parameter-" << TDP->getDepth() << '-' << TDP->getIndex();
    return;
  }
  case DeclarationName::SubstTemplatedPackName: {
    TemplateDeclNameParmDecl *TDP = N.getAsSubstTemplateDeclNameParmPackName()
                                        ->getReplacedParameter()
                                        ->getDecl();
    if (IdentifierInfo *Id = TDP->getIdentifier())
      OS << Id->getName();
    else
      OS << "declname-parameter-" << TDP->getDepth() << '-' << TDP->getIndex();
    return;
  }
  case DeclarationName::SubstTemplatedName:
    N.getAsSubstTemplateDeclNameParmName()->getReplacementName().print(OS,
                                                                       Policy);
    return;
  }

  llvm_unreachable("Unexpected declaration name kind");
}

raw_ostream &operator<<(raw_ostream &OS, DeclarationName N) {
  LangOptions LO;
  N.print(OS, PrintingPolicy(LO));
  return OS;
}

} // end namespace clang

DeclarationName::NameKind DeclarationName::getNameKind() const {
  switch (getStoredNameKind()) {
  case StoredIdentifier:          return Identifier;
  case StoredObjCZeroArgSelector: return ObjCZeroArgSelector;
  case StoredObjCOneArgSelector:  return ObjCOneArgSelector;

  case StoredDeclarationNameExtra:
    switch (getExtra()->ExtraKindOrNumArgs) {
    case DeclarationNameExtra::CXXConstructor:
      return CXXConstructorName;

    case DeclarationNameExtra::CXXDestructor:
      return CXXDestructorName;

    case DeclarationNameExtra::CXXConversionFunction:
      return CXXConversionFunctionName;

    case DeclarationNameExtra::CXXLiteralOperator:
      return CXXLiteralOperatorName;

    case DeclarationNameExtra::CXXUsingDirective:
      return CXXUsingDirective;

    case DeclarationNameExtra::CXXTemplatedName:
      return CXXTemplatedName;

    case DeclarationNameExtra::SubstTemplatedPackName:
      return SubstTemplatedPackName;

    case DeclarationNameExtra::SubstTemplatedName:
      return SubstTemplatedName;

    default:
      // Check if we have one of the CXXOperator* enumeration values.
      if (getExtra()->ExtraKindOrNumArgs <
            DeclarationNameExtra::CXXUsingDirective)
        return CXXOperatorName;

      return ObjCMultiArgSelector;
    }
  }

  // Can't actually get here.
  llvm_unreachable("This should be unreachable!");
}

bool DeclarationName::isDependentName() const {
  QualType T = getCXXNameType();
  return !T.isNull() && T->isDependentType() ||
         getNameKind() == CXXTemplatedName;
}

bool DeclarationName::isTemplatedName() const {
  return getStoredNameKind() == StoredDeclarationNameExtra &&
         getExtra()->ExtraKindOrNumArgs ==
             DeclarationNameExtra::CXXTemplatedName;
}

bool DeclarationName::containsUnexpandedParameterPack() const {
  if (TemplateDeclNameParmDecl *TDP = getCXXTemplatedNameParmDecl()) {
    return TDP->isParameterPack();
  }
  return getAsSubstTemplateDeclNameParmPackName() != nullptr;
}

std::string DeclarationName::getAsString() const {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  OS << *this;
  return OS.str();
}

QualType DeclarationName::getCXXNameType() const {
  if (CXXSpecialName *CXXName = getAsCXXSpecialName())
    return CXXName->Type;
  else
    return QualType();
}

OverloadedOperatorKind DeclarationName::getCXXOverloadedOperator() const {
  if (CXXOperatorIdName *CXXOp = getAsCXXOperatorIdName()) {
    unsigned value
      = CXXOp->ExtraKindOrNumArgs - DeclarationNameExtra::CXXConversionFunction;
    return static_cast<OverloadedOperatorKind>(value);
  } else {
    return OO_None;
  }
}

IdentifierInfo *DeclarationName::getCXXLiteralIdentifier() const {
  if (CXXLiteralOperatorIdName *CXXLit = getAsCXXLiteralOperatorIdName())
    return CXXLit->ID;
  else
    return nullptr;
}

void *DeclarationName::getFETokenInfoAsVoidSlow() const {
  switch (getNameKind()) {
  case Identifier:
    llvm_unreachable("Handled by getFETokenInfo()");

  case CXXConstructorName:
  case CXXDestructorName:
  case CXXConversionFunctionName:
    return getAsCXXSpecialName()->FETokenInfo;

  case CXXOperatorName:
    return getAsCXXOperatorIdName()->FETokenInfo;

  case CXXLiteralOperatorName:
    return getAsCXXLiteralOperatorIdName()->FETokenInfo;

  case CXXTemplatedName:
    return getAsCXXTemplateDeclNameParmName()->FETokenInfo;

  default:
    llvm_unreachable("Declaration name has no FETokenInfo");
  }
}

void DeclarationName::setFETokenInfo(void *T) {
  switch (getNameKind()) {
  case Identifier:
    getAsIdentifierInfo()->setFETokenInfo(T);
    break;

  case CXXConstructorName:
  case CXXDestructorName:
  case CXXConversionFunctionName:
    getAsCXXSpecialName()->FETokenInfo = T;
    break;

  case CXXOperatorName:
    getAsCXXOperatorIdName()->FETokenInfo = T;
    break;

  case CXXLiteralOperatorName:
    getAsCXXLiteralOperatorIdName()->FETokenInfo = T;
    break;

  case CXXTemplatedName:
    getAsCXXTemplateDeclNameParmName()->FETokenInfo = T;
    break;

  default:
    llvm_unreachable("Declaration name has no FETokenInfo");
  }
}

DeclarationName DeclarationName::getUsingDirectiveName() {
  // Single instance of DeclarationNameExtra for using-directive
  static const DeclarationNameExtra UDirExtra =
    { DeclarationNameExtra::CXXUsingDirective };

  uintptr_t Ptr = reinterpret_cast<uintptr_t>(&UDirExtra);
  Ptr |= StoredDeclarationNameExtra;

  return DeclarationName(Ptr);
}

LLVM_DUMP_METHOD void DeclarationName::dump() const {
  llvm::errs() << *this << '\n';
}

DeclarationNameTable::DeclarationNameTable(const ASTContext &C) : Ctx(C) {
  CXXSpecialNamesImpl = new llvm::FoldingSet<CXXSpecialName>;
  CXXLiteralOperatorNames = new llvm::FoldingSet<CXXLiteralOperatorIdName>;
  CXXTemplatedNames = new llvm::FoldingSet<CXXTemplateDeclNameParmName>;
  SubstTemplatedNames = new llvm::FoldingSet<SubstTemplateDeclNameParmName>;
  SubstTemplatedPackNames =
      new llvm::FoldingSet<SubstTemplateDeclNameParmPackName>;

  // Initialize the overloaded operator names.
  CXXOperatorNames = new (Ctx) CXXOperatorIdName[NUM_OVERLOADED_OPERATORS];
  for (unsigned Op = 0; Op < NUM_OVERLOADED_OPERATORS; ++Op) {
    CXXOperatorNames[Op].ExtraKindOrNumArgs
      = Op + DeclarationNameExtra::CXXConversionFunction;
    CXXOperatorNames[Op].FETokenInfo = nullptr;
  }
}

DeclarationNameTable::~DeclarationNameTable() {
  llvm::FoldingSet<CXXSpecialName> *SpecialNames =
      static_cast<llvm::FoldingSet<CXXSpecialName> *>(CXXSpecialNamesImpl);
  llvm::FoldingSet<CXXLiteralOperatorIdName> *LiteralNames =
      static_cast<llvm::FoldingSet<CXXLiteralOperatorIdName> *>(
          CXXLiteralOperatorNames);
  llvm::FoldingSet<CXXTemplateDeclNameParmName> *TemplatedNames =
      static_cast<llvm::FoldingSet<CXXTemplateDeclNameParmName> *>(
          CXXTemplatedNames);
  llvm::FoldingSet<SubstTemplateDeclNameParmName> *SubstNames =
      static_cast<llvm::FoldingSet<SubstTemplateDeclNameParmName> *>(
          SubstTemplatedNames);
  llvm::FoldingSet<SubstTemplateDeclNameParmName> *SubstPackNames =
      static_cast<llvm::FoldingSet<SubstTemplateDeclNameParmName> *>(
          SubstTemplatedPackNames);

  delete SpecialNames;
  delete LiteralNames;
  delete TemplatedNames;
  delete SubstNames;
  delete SubstPackNames;
}

DeclarationName DeclarationNameTable::getCXXConstructorName(CanQualType Ty) {
  return getCXXSpecialName(DeclarationName::CXXConstructorName,
                           Ty.getUnqualifiedType());
}

DeclarationName DeclarationNameTable::getCXXDestructorName(CanQualType Ty) {
  return getCXXSpecialName(DeclarationName::CXXDestructorName,
                           Ty.getUnqualifiedType());
}

DeclarationName
DeclarationNameTable::getCXXConversionFunctionName(CanQualType Ty) {
  return getCXXSpecialName(DeclarationName::CXXConversionFunctionName, Ty);
}

DeclarationName
DeclarationNameTable::getCXXSpecialName(DeclarationName::NameKind Kind,
                                        CanQualType Ty) {
  assert(Kind >= DeclarationName::CXXConstructorName &&
         Kind <= DeclarationName::CXXConversionFunctionName &&
         "Kind must be a C++ special name kind");
  llvm::FoldingSet<CXXSpecialName> *SpecialNames
    = static_cast<llvm::FoldingSet<CXXSpecialName>*>(CXXSpecialNamesImpl);

  DeclarationNameExtra::ExtraKind EKind;
  switch (Kind) {
  case DeclarationName::CXXConstructorName:
    EKind = DeclarationNameExtra::CXXConstructor;
    assert(!Ty.hasQualifiers() &&"Constructor type must be unqualified");
    break;
  case DeclarationName::CXXDestructorName:
    EKind = DeclarationNameExtra::CXXDestructor;
    assert(!Ty.hasQualifiers() && "Destructor type must be unqualified");
    break;
  case DeclarationName::CXXConversionFunctionName:
    EKind = DeclarationNameExtra::CXXConversionFunction;
    break;
  default:
    return DeclarationName();
  }

  // Unique selector, to guarantee there is one per name.
  llvm::FoldingSetNodeID ID;
  ID.AddInteger(EKind);
  ID.AddPointer(Ty.getAsOpaquePtr());

  void *InsertPos = nullptr;
  if (CXXSpecialName *Name = SpecialNames->FindNodeOrInsertPos(ID, InsertPos))
    return DeclarationName(Name);

  CXXSpecialName *SpecialName = new (Ctx) CXXSpecialName;
  SpecialName->ExtraKindOrNumArgs = EKind;
  SpecialName->Type = Ty;
  SpecialName->FETokenInfo = nullptr;

  SpecialNames->InsertNode(SpecialName, InsertPos);
  return DeclarationName(SpecialName);
}

DeclarationName
DeclarationNameTable::getCXXOperatorName(OverloadedOperatorKind Op) {
  return DeclarationName(&CXXOperatorNames[(unsigned)Op]);
}

DeclarationName
DeclarationNameTable::getCXXLiteralOperatorName(IdentifierInfo *II) {
  llvm::FoldingSet<CXXLiteralOperatorIdName> *LiteralNames
    = static_cast<llvm::FoldingSet<CXXLiteralOperatorIdName>*>
                                                      (CXXLiteralOperatorNames);

  llvm::FoldingSetNodeID ID;
  ID.AddPointer(II);

  void *InsertPos = nullptr;
  if (CXXLiteralOperatorIdName *Name =
                               LiteralNames->FindNodeOrInsertPos(ID, InsertPos))
    return DeclarationName (Name);
  
  CXXLiteralOperatorIdName *LiteralName = new (Ctx) CXXLiteralOperatorIdName;
  LiteralName->ExtraKindOrNumArgs = DeclarationNameExtra::CXXLiteralOperator;
  LiteralName->ID = II;
  LiteralName->FETokenInfo = nullptr;

  LiteralNames->InsertNode(LiteralName, InsertPos);
  return DeclarationName(LiteralName);
}

DeclarationName
DeclarationNameTable::getCXXTemplatedName(unsigned Depth, unsigned Index,
                                          bool ParameterPack,
                                          TemplateDeclNameParmDecl *TDPDecl) {
  llvm::FoldingSet<CXXTemplateDeclNameParmName> *TemplatedNames =
      static_cast<llvm::FoldingSet<CXXTemplateDeclNameParmName> *>(
          CXXTemplatedNames);

  llvm::FoldingSetNodeID ID;
  CXXTemplateDeclNameParmName::Profile(ID, Depth, Index, ParameterPack,
                                       TDPDecl);

  void *InsertPos = nullptr;
  CXXTemplateDeclNameParmName *NameParm =
      TemplatedNames->FindNodeOrInsertPos(ID, InsertPos);

  if (NameParm)
    return DeclarationName(NameParm);

  if (TDPDecl) {
    DeclarationName Canon = getCXXTemplatedName(Depth, Index, ParameterPack);
    NameParm = new (Ctx) CXXTemplateDeclNameParmName(
        TDPDecl, Canon.getAsCXXTemplateDeclNameParmName());

    CXXTemplateDeclNameParmName *NameCheck =
        TemplatedNames->FindNodeOrInsertPos(ID, InsertPos);
    assert(!NameCheck && "Template declname parameter canonical name broken");
    (void)NameCheck;
  } else
    NameParm =
        new (Ctx) CXXTemplateDeclNameParmName(TemplateDeclNameParmDecl::Create(
            Ctx, Ctx.getTranslationUnitDecl(), SourceLocation(),
            SourceLocation(), Depth, Index, ParameterPack, nullptr));

  NameParm->FETokenInfo = nullptr;

  TemplatedNames->InsertNode(NameParm, InsertPos);
  return DeclarationName(NameParm);
}

DeclarationName DeclarationNameTable::getSubstTemplatedName(
    CXXTemplateDeclNameParmName *Replaced, DeclarationName Replacement) {
  assert(Replacement.isCanonical() &&
         "replacement names must always be canonical");

  llvm::FoldingSet<SubstTemplateDeclNameParmName> *SubstNames =
      static_cast<llvm::FoldingSet<SubstTemplateDeclNameParmName> *>(
          SubstTemplatedNames);

  llvm::FoldingSetNodeID ID;
  SubstTemplateDeclNameParmName::Profile(ID, Replaced, Replacement);

  void *InsertPos = nullptr;
  SubstTemplateDeclNameParmName *NameParm =
      SubstNames->FindNodeOrInsertPos(ID, InsertPos);

  if (NameParm)
    return DeclarationName(NameParm);

  NameParm = new (Ctx) SubstTemplateDeclNameParmName(Replaced, Replacement);
  SubstNames->InsertNode(NameParm, InsertPos);
  return DeclarationName(NameParm);
}

DeclarationName DeclarationNameTable::getSubstTemplatedNamePack(
    CXXTemplateDeclNameParmName *Replaced, const TemplateArgument &ArgPack) {
#ifndef NDEBUG
  for (const auto &P : ArgPack.pack_elements()) {
    assert(P.getKind() == TemplateArgument::DeclName &&
           "Pack contains a non-declname");
    assert(P.getAsDeclName().isCanonical() &&
           "Pack contains non-canonical name");
  }
#endif

  llvm::FoldingSet<SubstTemplateDeclNameParmPackName> *SubstPackNames =
      static_cast<llvm::FoldingSet<SubstTemplateDeclNameParmPackName> *>(
          SubstTemplatedPackNames);

  llvm::FoldingSetNodeID ID;
  SubstTemplateDeclNameParmPackName::Profile(ID, Replaced, ArgPack);

  void *InsertPos = nullptr;
  SubstTemplateDeclNameParmPackName *NameParm =
      SubstPackNames->FindNodeOrInsertPos(ID, InsertPos);

  if (NameParm)
    return DeclarationName(NameParm);

  DeclarationName Canon;
  DeclarationName Parm(Replaced);
  if (!Parm.isCanonical()) {
    Canon = Parm.getCanonicalName();
    Canon = getSubstTemplatedNamePack(Canon.getAsCXXTemplateDeclNameParmName(),
                                      ArgPack);
  }
  NameParm =
      new (Ctx) SubstTemplateDeclNameParmPackName(Replaced, Canon, ArgPack);
  SubstPackNames->InsertNode(NameParm, InsertPos);
  return DeclarationName(NameParm);
}

DeclarationNameLoc::DeclarationNameLoc(DeclarationName Name) {
  switch (Name.getNameKind()) {
  case DeclarationName::Identifier:
    break;
  case DeclarationName::CXXConstructorName:
  case DeclarationName::CXXDestructorName:
  case DeclarationName::CXXConversionFunctionName:
    NamedType.TInfo = nullptr;
    break;
  case DeclarationName::CXXOperatorName:
    CXXOperatorName.BeginOpNameLoc = SourceLocation().getRawEncoding();
    CXXOperatorName.EndOpNameLoc = SourceLocation().getRawEncoding();
    break;
  case DeclarationName::CXXLiteralOperatorName:
    CXXLiteralOperatorName.OpNameLoc = SourceLocation().getRawEncoding();
    break;
  case DeclarationName::ObjCZeroArgSelector:
  case DeclarationName::ObjCOneArgSelector:
  case DeclarationName::ObjCMultiArgSelector:
    // FIXME: ?
    break;
  case DeclarationName::CXXUsingDirective:
  case DeclarationName::CXXTemplatedName:
  case DeclarationName::SubstTemplatedPackName:
  case DeclarationName::SubstTemplatedName:
    break;
  }
}

bool DeclarationNameInfo::containsUnexpandedParameterPack() const {
  switch (Name.getNameKind()) {
  case DeclarationName::Identifier:
  case DeclarationName::ObjCZeroArgSelector:
  case DeclarationName::ObjCOneArgSelector:
  case DeclarationName::ObjCMultiArgSelector:
  case DeclarationName::CXXOperatorName:
  case DeclarationName::CXXLiteralOperatorName:
  case DeclarationName::CXXUsingDirective:
  case DeclarationName::SubstTemplatedName:
    return false;

  case DeclarationName::CXXConstructorName:
  case DeclarationName::CXXDestructorName:
  case DeclarationName::CXXConversionFunctionName:
    if (TypeSourceInfo *TInfo = LocInfo.NamedType.TInfo)
      return TInfo->getType()->containsUnexpandedParameterPack();

    return Name.getCXXNameType()->containsUnexpandedParameterPack();

  case DeclarationName::CXXTemplatedName:
    return Name.getCXXTemplatedNameParmDecl()->isParameterPack();

  case DeclarationName::SubstTemplatedPackName:
    return true;
  }
  llvm_unreachable("All name kinds handled.");
}

bool DeclarationNameInfo::isInstantiationDependent() const {
  switch (Name.getNameKind()) {
  case DeclarationName::Identifier:
  case DeclarationName::ObjCZeroArgSelector:
  case DeclarationName::ObjCOneArgSelector:
  case DeclarationName::ObjCMultiArgSelector:
  case DeclarationName::CXXOperatorName:
  case DeclarationName::CXXLiteralOperatorName:
  case DeclarationName::CXXUsingDirective:
  case DeclarationName::SubstTemplatedName:
    return false;
    
  case DeclarationName::CXXConstructorName:
  case DeclarationName::CXXDestructorName:
  case DeclarationName::CXXConversionFunctionName:
    if (TypeSourceInfo *TInfo = LocInfo.NamedType.TInfo)
      return TInfo->getType()->isInstantiationDependentType();
    
    return Name.getCXXNameType()->isInstantiationDependentType();
  case DeclarationName::CXXTemplatedName:
  case DeclarationName::SubstTemplatedPackName:
    return true;
  }
  llvm_unreachable("All name kinds handled.");
}

std::string DeclarationNameInfo::getAsString() const {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  printName(OS);
  return OS.str();
}

void DeclarationNameInfo::printName(raw_ostream &OS) const {
  switch (Name.getNameKind()) {
  case DeclarationName::Identifier:
  case DeclarationName::ObjCZeroArgSelector:
  case DeclarationName::ObjCOneArgSelector:
  case DeclarationName::ObjCMultiArgSelector:
  case DeclarationName::CXXOperatorName:
  case DeclarationName::CXXLiteralOperatorName:
  case DeclarationName::CXXUsingDirective:
  case DeclarationName::CXXTemplatedName:
  case DeclarationName::SubstTemplatedPackName:
  case DeclarationName::SubstTemplatedName:
    OS << Name;
    return;

  case DeclarationName::CXXConstructorName:
  case DeclarationName::CXXDestructorName:
  case DeclarationName::CXXConversionFunctionName:
    if (TypeSourceInfo *TInfo = LocInfo.NamedType.TInfo) {
      if (Name.getNameKind() == DeclarationName::CXXDestructorName)
        OS << '~';
      else if (Name.getNameKind() == DeclarationName::CXXConversionFunctionName)
        OS << "operator ";
      LangOptions LO;
      LO.CPlusPlus = true;
      LO.Bool = true;
      OS << TInfo->getType().getAsString(PrintingPolicy(LO));
    } else
      OS << Name;
    return;
  }
  llvm_unreachable("Unexpected declaration name kind");
}

SourceLocation DeclarationNameInfo::getEndLoc() const {
  switch (Name.getNameKind()) {
  case DeclarationName::Identifier:
    return NameLoc;

  case DeclarationName::CXXOperatorName: {
    unsigned raw = LocInfo.CXXOperatorName.EndOpNameLoc;
    return SourceLocation::getFromRawEncoding(raw);
  }

  case DeclarationName::CXXLiteralOperatorName: {
    unsigned raw = LocInfo.CXXLiteralOperatorName.OpNameLoc;
    return SourceLocation::getFromRawEncoding(raw);
  }

  case DeclarationName::CXXConstructorName:
  case DeclarationName::CXXDestructorName:
  case DeclarationName::CXXConversionFunctionName:
    if (TypeSourceInfo *TInfo = LocInfo.NamedType.TInfo)
      return TInfo->getTypeLoc().getEndLoc();
    else
      return NameLoc;

    // DNInfo work in progress: FIXME.
  case DeclarationName::ObjCZeroArgSelector:
  case DeclarationName::ObjCOneArgSelector:
  case DeclarationName::ObjCMultiArgSelector:
  case DeclarationName::CXXUsingDirective:
  case DeclarationName::CXXTemplatedName:
  case DeclarationName::SubstTemplatedPackName:
  case DeclarationName::SubstTemplatedName:
    return NameLoc;
  }
  llvm_unreachable("Unexpected declaration name kind");
}

void CXXTemplateDeclNameParmName::Profile(llvm::FoldingSetNodeID &ID) {
  Profile(ID, TDPDecl->getDepth(), TDPDecl->getIndex(),
          TDPDecl->isParameterPack(),
          DeclarationName(this).isCanonical() ? nullptr : TDPDecl);
}

SubstTemplateDeclNameParmPackName::SubstTemplateDeclNameParmPackName(
    CXXTemplateDeclNameParmName *Param, DeclarationName Canon,
    const TemplateArgument &ArgPack)
    : DeclarationNameExtra(SubstTemplatedPackName, Canon), Replaced(Param),
      Arguments(ArgPack.pack_begin()), NumArguments(ArgPack.pack_size()) {
  if (!Canon)
    CanonicalPtr = DeclarationName(this).getAsOpaqueInteger();
}

TemplateArgument SubstTemplateDeclNameParmPackName::getArgumentPack() const {
  return TemplateArgument(llvm::makeArrayRef(Arguments, NumArguments));
}

void SubstTemplateDeclNameParmPackName::Profile(llvm::FoldingSetNodeID &ID) {
  Profile(ID, Replaced, getArgumentPack());
}

void SubstTemplateDeclNameParmPackName::Profile(
    llvm::FoldingSetNodeID &ID, const CXXTemplateDeclNameParmName *Replaced,
    const TemplateArgument &ArgPack) {
  ID.AddPointer(Replaced);
  ID.AddInteger(ArgPack.pack_size());
  for (const auto &P : ArgPack.pack_elements())
      ID.AddPointer(P.getAsDeclName().getAsOpaquePtr());
}
