#ifndef QUICKLINT_JS_LEX_H
#define QUICKLINT_JS_LEX_H

#include <cassert>
#include <cstddef>
#include <quicklint-js/location.h>
#include <string_view>

namespace quicklint_js {
class error_reporter;

enum class token_type {
  // Single-character symbols:
  ampersand = '&',
  bang = '!',
  circumflex = '^',
  colon = ':',
  comma = ',',
  slash = '/',
  dot = '.',
  equal = '=',
  greater = '>',
  left_curly = '{',
  left_paren = '(',
  left_square = '[',
  less = '<',
  minus = '-',
  percent = '%',
  pipe = '|',
  plus = '+',
  question = '?',
  right_curly = '}',
  right_paren = ')',
  right_square = ']',
  semicolon = ';',
  star = '*',
  tilde = '~',

  end_of_file,
  identifier,
  number,
  string,

  // Keywords:
  first_keyword,
  _await = first_keyword,
  _break,
  _case,
  _catch,
  _class,
  _const,
  _continue,
  _debugger,
  _default,
  _delete,
  _do,
  _else,
  _export,
  _extends,
  _false,
  _finally,
  _for,
  _function,
  _if,
  _import,
  _in,
  _instanceof,
  _let,
  _new,
  _null,
  _return,
  _static,
  _super,
  _switch,
  _this,
  _throw,
  _true,
  _try,
  _typeof,
  _var,
  _void,
  _while,
  _with,
  _yield,

  // Symbols:
  ampersand_ampersand,
  ampersand_equal,
  bang_equal,
  bang_equal_equal,
  circumflex_equal,
  dot_dot_dot,
  equal_equal,
  equal_equal_equal,
  equal_greater,
  greater_equal,
  greater_greater,
  greater_greater_equal,
  greater_greater_greater,
  greater_greater_greater_equal,
  less_equal,
  less_less,
  less_less_equal,
  minus_equal,
  minus_minus,
  percent_equal,
  pipe_equal,
  pipe_pipe,
  plus_equal,
  plus_plus,
  slash_equal,
  star_equal,
  star_star,
  star_star_equal,
};

class identifier {
 public:
  explicit identifier(source_code_span span) noexcept : span_(span) {}

  source_code_span span() const noexcept { return this->span_; }

  std::string_view string_view() const noexcept {
    return this->span_.string_view();
  }

 private:
  source_code_span span_;
};

struct token {
  identifier identifier_name() const noexcept;
  source_code_span span() const noexcept;

  token_type type;

  const char* begin;
  const char* end;
};

class lexer {
 public:
  explicit lexer(const char* input, error_reporter* error_reporter) noexcept
      : input_(input), error_reporter_(error_reporter) {
    this->parse_current_token();
  }

  void parse_current_token();

  const token& peek() const noexcept { return this->last_token_; }

  void skip() { this->parse_current_token(); }

 private:
  void skip_whitespace();

  static bool is_digit(char);
  static bool is_identifier_character(char);
  static token_type keyword_from_index(std::ptrdiff_t);

  token last_token_;
  const char* input_;
  error_reporter* error_reporter_;
};
}  // namespace quicklint_js

#endif
