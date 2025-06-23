/*
 * Copyright (c) 2025 Axoflow
 * Copyright (c) 2025 Attila Szakacs <attila.szakacs@axoflow.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef FILTERX_EVAL_EXCEPTION_HPP
#define FILTERX_EVAL_EXCEPTION_HPP

#include "compat/cpp-start.h"
#include "filterx/filterx-eval.h"
#include "compat/cpp-end.h"

#include <exception>
#include <string>

namespace syslogng {

class FilterXEvalException : public std::exception
{
public:
  FilterXEvalException(const char *message_, const std::string &info_, FilterXExpr *expr_ = nullptr) :
    message(message_), info(info_), expr(expr_) {}

  FilterXEvalException(const FilterXEvalException &other) :
    message(other.message), info(other.info), expr(other.expr) {}

  const char *what() const noexcept override
  {
    return this->info.c_str();
  }

  void push_filterx_error() const noexcept
  {
    filterx_eval_push_error_info(this->message, this->expr, g_strdup(this->info.c_str()), TRUE);
  }

private:
  const char *message;
  const std::string info;
  FilterXExpr *expr;
};

}

#endif