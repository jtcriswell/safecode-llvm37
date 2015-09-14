///
/// A simple template helper to create Function Type Arguments
/// 
/// $Id: VectorListHelper.h,v 1.1 2008-10-13 02:12:30 mai4 Exp $
///

#ifndef _VECTOR_LIST_HELPER_H_
#define _VECTOR_LIST_HELPER_H_

#include <vector>

namespace llvm
{
template <class T>
struct args {
  typedef T t_arg;
  typedef std::vector<t_arg> t_list;
  static t_list list() {
    return t_list();
  };
  static t_list list(t_arg ty1) {
    const t_arg arr[] = {ty1};
    return t_list(arr, arr + sizeof(arr) / sizeof(t_arg)); 
  };
  static t_list list(t_arg ty1, t_arg ty2) {
    const t_arg arr[] = {ty1, ty2};
    return t_list(arr, arr + sizeof(arr) / sizeof(t_arg)); 
  };
  static t_list list(t_arg ty1, t_arg ty2, t_arg ty3) {
    const t_arg arr[] = {ty1, ty2, ty3};
    return t_list(arr, arr + sizeof(arr) / sizeof(t_arg)); 
  };
  static t_list list(t_arg ty1, t_arg ty2, t_arg ty3, t_arg ty4) {
    const t_arg arr[] = {ty1, ty2, ty3, ty4};
    return t_list(arr, arr + sizeof(arr) / sizeof(t_arg)); 
  }
  static t_list list(t_arg ty1, t_arg ty2, t_arg ty3, t_arg ty4, t_arg ty5) {
    const t_arg arr[] = {ty1, ty2, ty3, ty4, ty5};
    return t_list(arr, arr + sizeof(arr) / sizeof(t_arg)); 
  }
  private:
    args();
};

}
#endif
