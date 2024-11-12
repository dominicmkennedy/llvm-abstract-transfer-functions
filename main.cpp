#include <algorithm>
#include <cmath>
#include <cstdio>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/ConstantRange.h>
#include <vector>

// TODO is this the best way to handle bitwidth?
#define BITWIDTH 4

void print_abst_range(const llvm::ConstantRange &x) {
  printf("[%ld, %ld)", x.getLower().getSExtValue(),
         x.getUpper().getSExtValue());
  if (x.isFullSet())
    printf(" (top)");
  if (x.isEmptySet())
    printf(" (bottom)");
  puts("");
}

void print_conc_range(const std::vector<llvm::APInt> &x) {
  if (x.empty()) {
    puts("empty");
    return;
  }
  for (llvm::APInt i : x) {
    printf("%ld ", i.getSExtValue());
  }
  puts("");
}

// TODO this function is super hacky but whatev
std::vector<llvm::ConstantRange> enum_abst_vals() {
  std::vector<llvm::ConstantRange> ret = std::vector<llvm::ConstantRange>();
  llvm::APInt min = llvm::APInt::getSignedMinValue(BITWIDTH);
  llvm::APInt max = llvm::APInt::getSignedMaxValue(BITWIDTH);

  for (llvm::APInt i = min;; ++i) {
    for (llvm::APInt j = min;; ++j) {
      if (i == j && !(i.isMaxValue() || i.isMinValue())) {
        if (j == max)
          break;
        else
          continue;
      }

      // print_abs_range(llvm::ConstantRange(i, j));
      ret.push_back(llvm::ConstantRange(i, j));

      if (j == max)
        break;
    }
    if (i == max)
      break;
  }

  return ret;
}

// TODO also a lil hacky function but whatev
std::vector<llvm::APInt> to_concrete(const llvm::ConstantRange &x) {
  std::vector<llvm::APInt> ret = std::vector<llvm::APInt>();

  if (x.isFullSet()) {
    for (llvm::APInt i = llvm::APInt::getSignedMinValue(BITWIDTH);
         i != llvm::APInt::getSignedMaxValue(BITWIDTH); ++i)
      ret.push_back(i);

    ret.push_back(llvm::APInt::getSignedMaxValue(BITWIDTH));

    return ret;
  }

  for (llvm::APInt i = x.getLower(); i != x.getUpper(); ++i)
    ret.push_back(i);

  return ret;
}

// TODO find some way to ensure this always returns the smallest range
// maybe by pre-sorting, tho idk if that works for wrapped values
llvm::ConstantRange to_abstract(const std::vector<llvm::APInt> &conc_vals) {
  auto ret = llvm::ConstantRange::getEmpty(BITWIDTH);

  for (auto x : conc_vals) {
    ret = ret.unionWith(llvm::ConstantRange(x));
  }

  return ret;
}

std::vector<llvm::APInt> sort_n_dedup_vals(std::vector<llvm::APInt> x) {
  std::sort(x.begin(), x.end(), [](const llvm::APInt &a, const llvm::APInt &b) {
    return a.getSExtValue() < b.getSExtValue();
  });

  auto last = std::unique(x.begin(), x.end());
  x.erase(last, x.end());

  return x;
}

std::vector<llvm::APInt> concrete_sdiv(const std::vector<llvm::APInt> &lhss,
                                       const std::vector<llvm::APInt> &rhss) {
  auto ret = std::vector<llvm::APInt>();
  if (lhss.size() == 0 || rhss.size() == 0)
    return ret;

  llvm::APInt z = llvm::APInt::getZero(lhss[0].getBitWidth());

  for (auto lhs : lhss) {
    for (auto rhs : rhss) {
      // TODO this avoids potential UB
      // maybe it should return some kind of top instead??
      if (rhs == z)
        continue;
      // TODO this avoids potential UB
      // maybe it should return some kind of top instead??
      // TODO pt2. these values are hardcoded based on the bitwidth and must
      // change to support other widths
      // if (lhs == llvm::APInt(BITWIDTH, -4) && rhs == llvm::APInt(BITWIDTH,
      // -1))
      if (lhs == llvm::APInt(BITWIDTH, -8) && rhs == llvm::APInt(BITWIDTH, -1))
        continue;
      ret.push_back(lhs.sdiv(rhs));
    }
  }

  // TODO this is done to just get unique vals
  // but further testing is needed to see how to get the smallest range from a
  // list of these vals
  ret = sort_n_dedup_vals(ret);

  return ret;
}

int compare_abst(const llvm::ConstantRange &lhs, const llvm::ConstantRange &rhs,
                 const llvm::ConstantRange &llvm,
                 const std::vector<llvm::APInt> &conc_brute) {

  llvm::ConstantRange brute = to_abstract(conc_brute);

  // case 0 -- both methods produced excatly the same set
  if (llvm == brute)
    return 0;

  std::vector<llvm::APInt> conc_llvm = to_concrete(llvm);

  // TODO one (or more?) of these cases should check for the unsoundness of llvm
  //
  // case 1 -- llvm is a proper superset of brute (most likely)
  bool case1 = true;
  for (auto x : conc_brute) {
    if (!llvm.contains(x))
      case1 = false;
  }
  if (case1)
    return 1;
  // case 2 -- brute is a proper superset of llvm (def unsound)
  bool case2 = true;
  for (auto x : conc_llvm) {
    if (std::count(conc_brute.begin(), conc_brute.end(), x) == 0)
      case2 = false;
  }
  if (case2)
    return 2;

  return 3;
  // TODO do these cases later
  // case 3 -- brute and llvm are incomperable but brute > llvm (may be unsound)
  // case 4 -- brute and llvm are incomperable but brute < llvm (may be unsound)
  // case 5 -- brute and llvm are incomperable but brute = llvm (may be unsound)
}

int main() {
  int case0 = 0;
  int case1 = 0;
  int case2 = 0;
  int case3 = 0;

  for (auto lhs : enum_abst_vals()) {
    for (auto rhs : enum_abst_vals()) {
      auto transfer_vals = lhs.sdiv(rhs);
      auto brute_vals = concrete_sdiv(to_concrete(lhs), to_concrete(rhs));
      int caseNum = compare_abst(lhs, rhs, transfer_vals, brute_vals);
      switch (caseNum) {
      case 0:
        case0++;
        break;
      case 1:
        case1++;
        break;
      case 2:
        case2++;
        break;
      case 3:
        case3++;
        break;
      }
    }
  }

  printf("case 0: %i\n", case0);
  printf("case 1: %i\n", case1);
  printf("case 2: %i\n", case2);
  printf("case 3: %i\n", case3);

  return 0;
}

// Q's for john for class tmro
//
// Q1:
// what to do about possible UB? could return top, or could just pretend that it
// doesn't exist it looks like llvm pretends that it doesn't exist, and this
// would still result in a refinement example: 3 bit signed division -4 / -1,
// would be 4 but that's too wide for 3 bits doing this operation using APInts
// returns -4 but idk
//
// Q2:
// what if results are incomperable?
// example in 3 bit signed integer division dividing the range
// [3, 2) / [-1, 1) yeilds the exact set {-3, -1, 0, 1, 2, 3}
// i.e. every value in i3 except for -4 and -2
// this  could be represented by the range [-3, -4) (i.e. every value but -4;
// correct and what llvm does) or it could be repersented by the range [-1, -2)
// (i.e. every value but -2; correct and what my code does)
//
// Q2 pt2
// When results are incomparable how do I check for soundness?
// I could check that every value contained in my concrete set is contained
// within the llvm range should we do that for the assingment

// GRAVEYARD
// llvm::ConstantRange to_abstract_old(const std::vector<llvm::APInt> &x) {
//   if (x.empty())
//     return llvm::ConstantRange::getEmpty(BITWIDTH);
//
//   llvm::APInt minElem =
//       *std::min_element(x.begin(), x.end(), [](llvm::APInt a, llvm::APInt b)
//       {
//         return a.getSExtValue() < b.getSExtValue();
//       });
//
//   llvm::APInt maxElem =
//       *std::max_element(x.begin(), x.end(), [](llvm::APInt a, llvm::APInt b)
//       {
//         return a.getSExtValue() < b.getSExtValue();
//       });
//
//   return llvm::ConstantRange(
//       minElem,
//       maxElem.isNegative() && (minElem != maxElem) ? maxElem - 1 : maxElem +
//       1);
// }
//
// compare_abst_old {
//   std::vector<llvm::APInt> llvm_m_brute{};
//   std::set_difference(conc_llvm.begin(), conc_llvm.end(), conc_brute.begin(),
//                       conc_brute.end(), back_inserter(llvm_m_brute),
//                       [](const llvm::APInt &a, const llvm::APInt &b) {
//                         return a.getSExtValue() != b.getSExtValue();
//                       });
//   std::vector<llvm::APInt> brute_m_llvm{};
//   std::set_difference(conc_brute.begin(), conc_brute.end(),
//   conc_llvm.begin(),
//                       conc_llvm.end(), back_inserter(brute_m_llvm),
//                       [](const llvm::APInt &a, const llvm::APInt &b) {
//                         return a.getSExtValue() != b.getSExtValue();
//                       });
//
//   if (!brute_m_llvm.empty()) {
//     printf("lhs: ");
//     print_abst_range(lhs);
//     printf("rhs: ");
//     print_abst_range(rhs);
//     printf("brute - llvm: ");
//     print_conc_range(brute_m_llvm);
//     printf("llvm - brute: ");
//     print_conc_range(llvm_m_brute);
//     printf("brute vals: ");
//     print_conc_range(conc_brute);
//     printf("brute range: ");
//     print_abst_range(brute);
//   puts("----------------------------");
//   return 1;
//   }
//   else {
//     if (!conc_brute.empty())
//       return 1;
//     else {
//
//       printf("lhs: ");
//       print_abst_range(lhs);
//       printf("rhs: ");
//       print_abst_range(rhs);
//       printf("brute - llvm: ");
//       print_conc_range(brute_m_llvm);
//       printf("llvm - brute: ");
//       print_conc_range(llvm_m_brute);
//       printf("brute vals: ");
//       print_conc_range(conc_brute);
//       printf("brute range: ");
//       print_abst_range(brute);
//       puts("----------------------------");
//       return 0;
//     }
//   }
//   printf("llvm:  ");
//   print_abst_range(llvm);
//   printf("brute: ");
//   print_abst_range(brute);
//   printf("llvm - brute: ");
//   print_conc_range(llvm_m_brute);
//   printf("brute - llvm: ");
//   print_conc_range(brute_m_llvm);
//   puts("----------------------------");
// }
//
// old_main {
//   puts("");
//   auto lhsx =
//       llvm::ConstantRange(llvm::APInt(BITWIDTH, 3), llvm::APInt(BITWIDTH,
//       2));
//   auto rhsx =
//       llvm::ConstantRange(llvm::APInt(BITWIDTH, -1), llvm::APInt(BITWIDTH,
//       1));
//   printf("lhs: ");
//   print_conc_range(to_concrete(lhsx));
//   printf("rhs: ");
//   print_conc_range(to_concrete(rhsx));
//   auto brute_vals = concrete_sdiv(to_concrete(lhsx), to_concrete(rhsx));
//   printf("brute force vals: ");
//   print_conc_range(brute_vals);
//   auto llvm_vals = lhsx.sdiv(rhsx);
//   printf("llvm vals conc:   ");
//   print_conc_range(to_concrete(llvm_vals));
//   printf("llvm vals:  ");
//   print_abst_range(llvm_vals);
//   printf("brute vals: ");
//   print_abst_range(to_abstract(brute_vals));
// }
