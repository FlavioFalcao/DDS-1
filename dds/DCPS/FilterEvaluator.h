/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_FILTER_EVALUATOR_H
#define OPENDDS_DCPS_FILTER_EVALUATOR_H

#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE

#include "dds/DdsDcpsInfrastructureC.h"
#include "Comparator_T.h"

#include <vector>
#include <string>

namespace OpenDDS {
namespace DCPS {

class MetaStruct;

template<typename T>
const MetaStruct& getMetaStruct();

class OpenDDS_Dcps_Export FilterEvaluator {
public:
  FilterEvaluator(const char* filter, bool allowOrderBy);

  ~FilterEvaluator();

  std::vector<std::string> getOrderBys() const;

  bool hasFilter() const;

  template<typename T>
  bool eval(const T& sample, const DDS::StringSeq& params) const
  {
    return eval_i(static_cast<const void*>(&sample),
                  getMetaStruct<T>(), params);
  }

  class EvalNode;

private:
  FilterEvaluator(const FilterEvaluator&);
  FilterEvaluator& operator=(const FilterEvaluator&);

  struct AstNodeWrapper;
  EvalNode* walkAst(const AstNodeWrapper& node, EvalNode* prev);

  bool eval_i(const void* sample, const MetaStruct& meta,
              const DDS::StringSeq& params) const;

  std::string filter_;
  EvalNode* filter_root_;
  std::vector<std::string> order_bys_;
};

struct OpenDDS_Dcps_Export Value {
  Value(bool b, bool conversion_preferred = false);
  Value(int i, bool conversion_preferred = false);
  Value(unsigned int u, bool conversion_preferred = false);
  Value(ACE_INT64 l, bool conversion_preferred = false);
  Value(ACE_UINT64 m, bool conversion_preferred = false);
  Value(char c, bool conversion_preferred = false);
  Value(double f, bool conversion_preferred = false);
  Value(const char* s, bool conversion_preferred = false);

  ~Value();
  Value(const Value& v);
  Value& operator=(const Value& v);
  void swap(Value& other);

  bool operator==(const Value& v) const;
  bool operator<(const Value& v) const;
  bool like(const Value& v) const;

  enum Type {VAL_BOOL, VAL_INT, VAL_UINT, VAL_I64, VAL_UI64, VAL_FLOAT,
    VAL_LARGEST_NUMERIC = VAL_FLOAT,
    VAL_CHAR, VAL_STRING};
  bool convert(Type t);
  static void conversion(Value& lhs, Value& rhs);
  template<typename T> T& get();
  template<typename T> const T& get() const;

  Type type_;
  union {
    bool b_;
    int i_;
    unsigned int u_;
    ACE_INT64 l_;
    ACE_UINT64 m_;
    char c_;
    double f_;
    const char* s_;
  };
  bool conversion_preferred_;
};

class MetaStruct {
public:
  virtual Value getValue(const void* stru, const char* fieldSpec) const = 0;
  virtual ComparatorBase::Ptr create_qc_comparator(const char* fieldSpec,
    ComparatorBase::Ptr next) const = 0;
};

/// Each user-defined struct type will have an instantiation of this template
/// generated by opendds_idl.
template<typename T>
struct MetaStructImpl;

}  }

#endif // OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE
#endif
