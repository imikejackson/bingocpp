#include <cstdlib>
#include <ctime>

#include "BingoCpp/agraph_crossover.h"

namespace bingo {
namespace {
int find_random_int(int min, int max) {
  return std::rand()%(max - min + 1) + min;
}

void perform_crossover(AGraph& child, AGraph& parent, int cross_point) {

  Eigen::ArrayX3i child_array = child.getCommandArray();
  Eigen::ArrayX3i parent_array = parent.getCommandArray();
  size_t agraph_size = parent.getCommandArray.rows();

  child_array.block(cross_point, 0, agraph_size, agraph_size) =
    parent_array.block(cross_point, 0, agraph_size, agraph_size);
}
} // namespace

AGraphCrossover::AGraphCrossover() {
  std::srand(std::time(nullptr));
}

CrossoverChildren AGraphCrossover::crossover(AGraph& parent_1,
                                             AGraph& parent_2) {
  AGraph child_1 = parent_1.copy();
  AGraph child_2 = parent_2.copy();

  size_t agraph_size = parent_1.getCommandArray().rows();
  int cross_point = find_random_int(1, agraph_size - 1);
  perform_crossover(child_1, parent_2, cross_point);
  perform_crossover(child_2, parent_1, cross_point);

  child_1.notifyCommandArrayModificiation();
  child_2.notifyCommandArrayModificiation();

  int child_age = std::max(parent_1.getGeneticAge(), parent_2.getGeneticAge());
  child_1.setGeneticAge(child_age);
  child_2.setGeneticAge(child_age);

  return std::make_pair(child_1, child_2);
}
} // namespace bingo