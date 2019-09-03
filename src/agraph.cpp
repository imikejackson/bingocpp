#include <algorithm>
#include <limits>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <BingoCpp/agraph.h>
#include <BingoCpp/backend.h>
#include <BingoCpp/constants.h>

const PrintMap kStackPrintMap {
  {2, "({}) + ({})"},
  {3, "({}) - ({})"},
  {4, "({}) * ({})"},
  {5, "({}) / ({}) "},
  {6, "sin ({})"},
  {7, "cos ({})"},
  {8, "exp ({})"},
  {9, "log ({})"},
  {10, "({}) ^ ({})"},
  {11, "abs ({})"},
  {12, "sqrt ({})"},
};

const PrintMap kLatexPrintMap {
  {2, "{} + {}"},
  {3, "{} - ({})"},
  {4, "({})({})"},
  {5, "\\frac{ {} }{ {} }"},
  {6, "sin{ {} }"},
  {7, "cos{ {} }"},
  {8, "exp{ {} }"},
  {9, "log{ {} }"},
  {10, "({})^{ ({}) }"},
  {11, "|{}|"},
  {12, "\\sqrt{ {} }"},
};

const PrintMap kConsolePrintMap {
  {2, "{} + {}"},
  {3, "{} - ({})"},
  {4, "({})({})"},
  {5, "({})/({})"},
  {6, "sin({})"},
  {7, "cos({})"},
  {8, "exp({})"},
  {9, "log({})"},
  {10, "({})^({})"},
  {11, "|{}|"},
  {12, "sqrt({})"},
};

const PrintVector kOperatorNames {
  std::vector<std::string> {"load", "x"},
  std::vector<std::string> {"constant", "c"},
  std::vector<std::string> {"add", "addition", "+"},
  std::vector<std::string> {"subtract", "subtraction", "-"},
  std::vector<std::string> {"multiply", "multiplication", "*"},
  std::vector<std::string> {"divide", "division", "/"},
  std::vector<std::string> {"sine", "sin"},
  std::vector<std::string> {"cosine", "cos"},
  std::vector<std::string> {"exponential", "exp", "e"},
  std::vector<std::string> {"logarithm", "log"},
  std::vector<std::string> {"power", "pow", "^"},
  std::vector<std::string> {"absolute value", "||", "|"},
  std::vector<std::string> {"square root", "sqrt"}
};

namespace bingo {
namespace {

std::string print_string_with_args(const std::string& string,
                                   const std::string& arg1,
                                   const std::string& arg2);

bool check_optimization_requirement(AGraph& agraph,
                                    const std::vector<bool>& utilized_commands);

std::string get_stack_element_string(const AGraph& individual,
                                     int command_index,
                                     const Eigen::ArrayX3i& stack_element);

std::string get_formatted_element_string(const AGraph& individual,
                                         const Eigen::ArrayX3i& stack_element,
                                         std::vector<std::string> string_list,
                                         const PrintMap& format_map);


} // namespace

AGraph::AGraph(
    Eigen::ArrayX3i command_array,
    Eigen::ArrayX3i short_command_array,
    Eigen::VectorXd constants,
    int num_constants,
    bool manual_constants,
    int needs_opt,
    double fitness,
    bool fit_set,
    int genetic_age) {
  command_array_ = command_array;
  short_command_array_ = short_command_array;
  constants_ = constants;
  num_constants_ = num_constants;
  manual_constants_ = manual_constants;
  needs_opt_ = needs_opt;
  fitness_ = fitness;
  fit_set_ = fit_set;
  genetic_age_ = genetic_age;
}

AGraph::AGraph(const AGraph& agraph) {
  this->SetCommandArray(agraph.GetCommandArray());
  constants_ = agraph.GetLocalOptimizationParams();
  needs_opt_ = agraph.NeedsLocalOptimization();
  num_constants_ = agraph.GetNumberLocalOptimizationParams();
  fitness_ = agraph.GetFitness();
  fit_set_ = agraph.IsFitnessSet();
  genetic_age_ = agraph.GetGeneticAge();
}

AGraph AGraph::Copy() {
  AGraph return_val = AGraph(*this);
  return return_val;
}

const Eigen::ArrayX3i& AGraph::GetCommandArray() const {
  return command_array_;
}

void AGraph::SetCommandArray(Eigen::ArrayX3i command_array) {
  command_array_ = command_array;
  fitness_ = 1e9;
  fit_set_ = false;
  process_modified_command_array();
}

void AGraph::NotifyCommandArrayModificiation() {
  fitness_ = 1e9;
  fit_set_ = false;
  process_modified_command_array();
}

void AGraph::process_modified_command_array() {
  if (!manual_constants_) {
    std::vector<bool> util = GetUtilizedCommands();

    needs_opt_ = check_optimization_requirement(*this, util);
    if (needs_opt_)  {
      renumber_constants(util);
    }
  }
  short_command_array_ = backend::SimplifyStack(command_array_);
}

void AGraph::renumber_constants (const std::vector<bool>& utilized_commands) {
  int const_num = 0;
  int command_array_depth = command_array_.rows();
  for (int i = 0; i < command_array_depth; i++) {
    if (utilized_commands[i] &&
        command_array_(i, ArrayProps::kNodeIdx) == Op::LOAD_C) {
      command_array_.row(i) << 1, const_num , const_num; 
      const_num ++;
    }
  }
  num_constants_ = const_num;
}

double AGraph::GetFitness() const {
  return fitness_;
}

void AGraph::SetFitness(double fitness) {
  fitness_ = fitness;
  fit_set_ = true;
}

bool AGraph::IsFitnessSet() const {
  return fit_set_;
}

void AGraph::SetGeneticAge(const int age) {
  genetic_age_ = age;
}

int AGraph::GetGeneticAge() const {
  return genetic_age_;
}

std::vector<bool> AGraph::GetUtilizedCommands() const {
  return backend::GetUtilizedCommands(command_array_);
}

bool AGraph::NeedsLocalOptimization() const {
  return needs_opt_;
}

int AGraph::GetNumberLocalOptimizationParams() const {
  return num_constants_;
}

void AGraph::SetLocalOptimizationParams(Eigen::VectorXd params) {
  constants_ = params;
  needs_opt_ = false;
}

Eigen::VectorXd AGraph::GetLocalOptimizationParams() const {
  return constants_;
}

Eigen::ArrayXXd 
AGraph::EvaluateEquationAt(Eigen::ArrayXXd& x) {
  Eigen::ArrayXXd f_of_x; 
  try {
    f_of_x = backend::Evaluate(this->command_array_,
                               x,
                               this->constants_);
    return f_of_x;
  } catch (const std::underflow_error& ue) {
    return Eigen::ArrayXXd::Constant(x.rows(), x.cols(), kNaN);
  } catch (const std::overflow_error& oe) {
    return Eigen::ArrayXXd::Constant(x.rows(), x.cols(), kNaN);
  } 
}

EvalAndDerivative
AGraph::EvaluateEquationWithXGradientAt(Eigen::ArrayXXd& x) {
  EvalAndDerivative df_dx;
  try {
    df_dx = backend::EvaluateWithDerivative(this->command_array_,
                                            x,
                                            this->constants_,
                                            true);
    return df_dx;
  } catch (const std::underflow_error& ue) {
    Eigen::ArrayXXd nan_array = Eigen::ArrayXXd::Constant(x.rows(), x.cols(), kNaN);
    return std::make_pair(nan_array, nan_array);
  } catch (const std::overflow_error& oe) {
    Eigen::ArrayXXd nan_array = Eigen::ArrayXXd::Constant(x.rows(), x.cols(), kNaN);
    return std::make_pair(nan_array, nan_array);
  }
}

EvalAndDerivative
AGraph::EvaluateEquationWithLocalOptGradientAt(Eigen::ArrayXXd& x) {
  EvalAndDerivative df_dc;
  try {
    df_dc = backend::EvaluateWithDerivative(this->command_array_,
                                            x,
                                            this->constants_,
                                            false);
    return df_dc;
  } catch (const std::underflow_error& ue) {
    Eigen::ArrayXXd nan_array = Eigen::ArrayXXd::Constant(x.rows(), x.cols(), kNaN);
    return std::make_pair(nan_array, nan_array);
  } catch (const std::overflow_error& oe) {
    Eigen::ArrayXXd nan_array = Eigen::ArrayXXd::Constant(x.rows(), x.cols(), kNaN);
    return std::make_pair(nan_array, nan_array);
  }
}

std::ostream& operator<<(std::ostream& strm, const AGraph& graph) {
  return strm << graph.GetConsoleString();
}

std::string AGraph::GetLatexString() const {
  return get_formatted_string_using(kLatexPrintMap);
}

std::string AGraph::GetConsoleString() const {
  return get_formatted_string_using(kConsolePrintMap);
}

std::string AGraph::get_formatted_string_using(const PrintMap& format_map) const {
  std::vector<std::string> string_list;
  for (auto stack_element : short_command_array_.rowwise()) {
    std::string temp_string = get_formatted_element_string(
        *this, stack_element, string_list, format_map);
    string_list.push_back(temp_string);
  }
  return string_list.back();
}

std::string AGraph::GetStackString() const {
  std::stringstream print_str; 
  print_str << "---full stack---\n"
            << get_stack_string()
            << "---small stack---\n"
            << get_stack_string(true); 
  return print_str.str();
}

std::string AGraph::get_stack_string(const bool is_short) const {
  Eigen::ArrayX3i stack;
  if (is_short) {
    stack = short_command_array_; 
  } else {
    stack = command_array_;
  }
  std::string temp_string;
  for (int i = 0; i < stack.rows(); i++) {
    temp_string += get_stack_element_string(*this, i, stack.row(i));
  }
  return temp_string;
}

int AGraph::GetComplexity() const {
  std::vector<bool> commands = GetUtilizedCommands();
  return std::count_if (commands.begin(), commands.end(), [](bool i) {
    return i;
  });
}

void AGraph::ForceRenumberConstants() {
  std::vector<bool> util_commands = GetUtilizedCommands();
  renumber_constants(util_commands);
}

int AGraph::Distance(const AGraph& agraph) {
  return (command_array_ != agraph.GetCommandArray()).sum();
}

bool AGraph::HasArityTwo(int node) {
  return kIsArity2Map[node];
}

bool AGraph::IsTerminal(int node) {
  return kIsTerminalMap[node];
}

const bool AGraph::kIsArity2Map[13] = {
  false,
  false,
  true,
  true,
  true,
  true,
  false,
  false,
  false,
  false,
  true,
  false,
  false
};

const bool AGraph::kIsTerminalMap[13] = {
  true,
  true,
  false,
  false,
  false,
  false,
  false,
  false,
  false,
  false,
  false,
  false,
  false
};

namespace {

std::string print_string_with_args(const std::string& string,
                                   const std::string& arg1,
                                   const std::string& arg2) {
  std::stringstream stream;
  bool first_found = false;
  for (auto character = string.begin(); character != string.end(); character++) {
    if (*character == '{' && *(character + 1) == '}') {
      stream << ((!first_found) ? arg1 : arg2);
      character++;
      first_found = true;
    } else {
      stream << *character;
    }
  }
  return stream.str();
}

bool check_optimization_requirement(AGraph& agraph,
                                    const std::vector<bool>& utilized_commands) {
  Eigen::ArrayX3i command_array = agraph.GetCommandArray();
  for (int i = 0; i < command_array.rows(); i++) {
    if (utilized_commands[i] && command_array(i, ArrayProps::kNodeIdx) == Op::LOAD_C) {
      if (command_array(i, ArrayProps::kOp1) == Op::C_OPTIMIZE ||
          command_array(i, ArrayProps::kOp1) >=
          agraph.GetLocalOptimizationParams().size()) {
        return true;
      }
    }
  }
  return false;
}

std::string get_stack_element_string(const AGraph& individual,
                                     int command_index,
                                     const Eigen::ArrayX3i& stack_element) {
  int node = stack_element(0, ArrayProps::kNodeIdx);
  int param1 = stack_element(0, ArrayProps::kOp1);
  int param2 = stack_element(0, ArrayProps::kOp2);

  std::string temp_string = "("+ std::to_string(command_index) +") <= ";
  if (node == Op::LOAD_X) {
    temp_string += "X_" + std::to_string(param1);
  } else if (node == Op::LOAD_C) {
    if (param1 == Op::C_OPTIMIZE ||
        param1 >= individual.GetLocalOptimizationParams().size()) {
      temp_string += "C";
    } else {
      Eigen::VectorXd parameter = individual.GetLocalOptimizationParams();
      temp_string += "C_" + std::to_string(param1) + " = " + 
                     std::to_string(parameter[param1]);
    }
  } else {
    std::string param1_str = std::to_string(param1);
    std::string param2_str = std::to_string(param2);
    temp_string += print_string_with_args(kStackPrintMap.at(node),
                                          param1_str,
                                          param2_str);
  }
  temp_string += '\n';
  return temp_string;
}

std::string get_formatted_element_string(const AGraph& individual,
                                         const Eigen::ArrayX3i& stack_element,
                                         std::vector<std::string> string_list,
                                         const PrintMap& format_map) {
  int node = stack_element(0, ArrayProps::kNodeIdx);
  int param1 = stack_element(0, ArrayProps::kOp1);
  int param2 = stack_element(0, ArrayProps::kOp2);

  std::string temp_string;
  if (node == Op::LOAD_X) {
    temp_string = "X_" + std::to_string(param1);
  } else if (node == Op::LOAD_C) {
    if (param1 == Op::C_OPTIMIZE ||
        param1 >= individual.GetLocalOptimizationParams().size()) {
      temp_string = "?";
    } else {
      Eigen::VectorXd parameter = individual.GetLocalOptimizationParams();
      temp_string = std::to_string(parameter[param1]);
    }
  } else {
    temp_string = print_string_with_args(format_map.at(node),
                                         string_list[param1],
                                         string_list[param2]);
  }
  return temp_string;
}
} // namespace (anonymous)
} // namespace bingo