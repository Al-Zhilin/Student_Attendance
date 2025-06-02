byte CheckSurnameMatch(const String s_input, const String s_list, byte* syntax_errors) {          //разные варианты совпадения строк и их сравнения
  if (s_input == s_list) return 1;
  if (s_input.length() != s_list.length()) return 0;
  for (byte i = 0; i < s_input.length() && i < s_list.length(); i+=2) {
    String symbol1 = s_input.substring(i, i+2);
    String symbol2 = s_list.substring(i, i+2);
    if (symbol1 != symbol2) {
      (*syntax_errors)++;
      if (*syntax_errors > SURNAME_ERRORS_NUM) return 0;
    }
  }
  return 2;
}


/*
for (byte iter = 0; iter < s_input.length(); iter += sizeof(s_input[0])) {
    if (s_input[iter] != s_list[iter]) {
      (*syntax_errors)++;
      if (*syntax_errors > SURNAME_ERRORS_NUM) return 0;
    }
  }
*/