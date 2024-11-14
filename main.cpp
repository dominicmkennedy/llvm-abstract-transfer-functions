#include <algorithm>
#include <cmath>
#include <cstdio>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/ConstantRange.h>
#include <vector>

// TODO likely a better way to parameterize bitwidth
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
// this function tries out a few methods but there are no gaurentees
llvm::ConstantRange to_abstract(const std::vector<llvm::APInt> &conc_vals) {
  auto ret0 = llvm::ConstantRange::getEmpty(BITWIDTH);
  auto ret1 = llvm::ConstantRange::getEmpty(BITWIDTH);
  auto ret2 = llvm::ConstantRange::getEmpty(BITWIDTH);
  int largest_gap = 0;
  int largest_gap_idx = 0;

  for (auto x : conc_vals) {
    ret0 = ret0.unionWith(x);
  }

  for (auto it = conc_vals.rbegin(); it != conc_vals.rend(); ++it) {
    ret1 = ret1.unionWith(*it);
  }

  auto smallestRet = ret1.isSizeStrictlySmallerThan(ret0) ? ret1 : ret0;
  if (conc_vals.size() > 1) {
    for (size_t i = 0; i < conc_vals.size() - 1; ++i) {
      auto gap = llvm::APIntOps::abds(conc_vals[i], conc_vals[i + 1]);
      if (gap.uge(largest_gap)) {
        largest_gap = gap.getSExtValue();
        largest_gap_idx = i;
      }
    }

    for (size_t i = 0; i < conc_vals.size(); ++i) {
      ret2 = ret2.unionWith(llvm::ConstantRange(
          conc_vals[(i + largest_gap_idx + 1) % conc_vals.size()]));
    }

    return ret2.isSizeStrictlySmallerThan(smallestRet) ? ret2 : smallestRet;
  }

  return smallestRet;
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
      // this check refines div by zero UB
      if (rhs == z)
        continue;
      // this check refines singed int wrapping UB
      // TODO these values are hardcoded based on the bitwidth and must
      if (lhs == llvm::APInt(BITWIDTH, -8) && rhs == llvm::APInt(BITWIDTH, -1))
        continue;

      ret.push_back(lhs.sdiv(rhs));
    }
  }

  return sort_n_dedup_vals(ret);
}

int compare_abst(const llvm::ConstantRange &lhs, const llvm::ConstantRange &rhs,
                 const llvm::ConstantRange &llvm,
                 const std::vector<llvm::APInt> &conc_brute) {

  llvm::ConstantRange brute = to_abstract(conc_brute);

  // case 0 -- both methods produced excatly the same set
  if (llvm == brute)
    return 0;

  std::vector<llvm::APInt> conc_llvm = to_concrete(llvm);

  // case 1 -- incomparable sets of the same size
  if (conc_llvm.size() == conc_brute.size())
    return 1;
  // case 2 -- incomparable sets where llvm's is bigger
  if (conc_llvm.size() > conc_brute.size())
    return 2;

  // case 3 -- incomparable sets where llvm's is smaller
  return 3;
}

int main() {
  std::vector<int> cases = {0, 0, 0, 0};
  long long i = 0;

  for (auto lhs : enum_abst_vals()) {
    for (auto rhs : enum_abst_vals()) {
      auto transfer_vals = lhs.sdiv(rhs);
      auto brute_vals = concrete_sdiv(to_concrete(lhs), to_concrete(rhs));
      int caseNum = compare_abst(lhs, rhs, transfer_vals, brute_vals);
      cases[caseNum]++;
      i++;
    }
  }

  printf("case 0: %i\n", cases[0]);
  printf("case 1: %i\n", cases[1]);
  printf("case 2: %i\n", cases[2]);
  printf("case 3: %i\n", cases[3]);
  printf("tests : %lld\n", i);

  return 0;
}
