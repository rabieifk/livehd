//  This file is distributed under the BSD 3-Clause License. See LICENSE for details.
#pragma once

#include <array>
#include <unordered_map>

#include "lconst.hpp"
#include "lnast.hpp"
#include "pass.hpp"

class Opt_lnast {
protected:
  int                                           level;
  std::unordered_map<std::string, std::string> level_forward_ref;
  std::unordered_map<std::string, Lconst>      level_forward_val;

  void process_plus(std::shared_ptr<Lnast> ln, const Lnast_nid &lnid);
  void process_assign(std::shared_ptr<Lnast> ln, const Lnast_nid &lnid);
  void process_todo(std::shared_ptr<Lnast> ln, const Lnast_nid &lnid);

public:
  Opt_lnast(const Eprp_var &var);

  void opt(std::shared_ptr<Lnast> ln);
};
