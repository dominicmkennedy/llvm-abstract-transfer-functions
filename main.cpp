#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/KnownBits.h>
#include <vector>

template <typename T>
concept APIntContainer = requires(T t) {
  typename T::value_type;
  requires std::same_as<typename T::value_type, llvm::APInt>;
  requires std::ranges::range<T>;
};

// TODO provied hashing and ordering func for llvm APInts

void print_abst_range(const llvm::KnownBits &x) {
  for (int32_t i = x.Zero.getBitWidth() - 1; i >= 0; --i) {
    const char bit = x.One[i] ? '1' : x.Zero[i] ? '0' : '?';
    printf("%c", bit);
  }

  if (x.isConstant())
    printf(" const %lu", x.getConstant().getZExtValue());

  if (x.isUnknown())
    printf(" (top)");

  printf("\n");
}

// TODO consider either set or generic container
// TODO consider printing full/top if it is
void print_conc_range(const APIntContainer auto &x) {
  if (x.empty())
    printf("empty");

  for (llvm::APInt i : x)
    printf("%ld ", i.getZExtValue());

  puts("");
}

// TODO there's a faster way to this but this works for now
// would also be nice if this moved up the lattice as the loops progressed
std::vector<llvm::KnownBits> const enum_abst_vals(const uint32_t bitwidth) {
  const llvm::APInt min = llvm::APInt::getSignedMinValue(bitwidth);
  const llvm::APInt max = llvm::APInt::getSignedMaxValue(bitwidth);
  std::vector<llvm::KnownBits> ret;

  for (auto i = min;; ++i) {
    for (auto j = min;; ++j) {
      auto x = llvm::KnownBits(bitwidth);
      x.One = i;
      x.Zero = j;

      if (!x.hasConflict())
        ret.push_back(x);

      if (j == max)
        break;
    }
    if (i == max)
      break;
  }

  return ret;
}

// TODO return a generic container based on what the caller asks for
// TODO there's a faster way to this but this works for now
std::vector<llvm::APInt> const to_concrete(const llvm::KnownBits &x) {
  std::vector<llvm::APInt> ret = std::vector<llvm::APInt>();
  const llvm::APInt min = llvm::APInt::getZero(x.Zero.getBitWidth());
  const llvm::APInt max = llvm::APInt::getMaxValue(x.Zero.getBitWidth());

  for (auto i = min;; ++i) {

    if (!x.Zero.intersects(i) && !x.One.intersects(~i))
      ret.push_back(i);

    if (i == max)
      break;
  }

  return ret;
}

llvm::KnownBits const to_abstract(const APIntContainer auto &conc_vals) {
  auto ret = llvm::KnownBits::makeConstant(conc_vals[0]);

  for (auto x : conc_vals) {
    ret = ret.intersectWith(llvm::KnownBits::makeConstant(x));
  }

  return ret;
}

// TODO parameterize this function to do any concret APInt oporation
// TODO parameterize for any generic std container
// TODO have some automated check for UB?
std::vector<llvm::APInt> const
concrete_op(const std::vector<llvm::APInt> &lhss,
            const std::vector<llvm::APInt> &rhss) {
  auto ret = std::vector<llvm::APInt>();

  for (auto lhs : lhss)
    for (auto rhs : rhss)
      ret.push_back(lhs ^ rhs);

  return ret;
}

// TODO make case enum
int compare(const llvm::KnownBits &approx,
            const std::vector<llvm::APInt> &exact) {

  const auto exact_abst = to_abstract(exact);

  if (exact_abst == approx)
    return 0;

  return 1;
}

int main() {
  const size_t bitwidth = 4;

  // TODO make cases a real cpp enum
  std::vector<int> cases = {0, 0};
  long long i = 0;

  for (auto lhs : enum_abst_vals(bitwidth)) {
    for (auto rhs : enum_abst_vals(bitwidth)) {
      const auto transfer_vals = lhs ^ rhs;
      const auto brute_vals = concrete_op(to_concrete(lhs), to_concrete(rhs));
      const int caseNum = compare(transfer_vals, brute_vals);
      cases[caseNum]++;
      i++;
    }
  }

  printf("case 0 (same): %i\n", cases[0]);
  printf("case 1 (diff): %i\n", cases[1]);
  printf("tests : %lld\n", i);

  return 0;
}
