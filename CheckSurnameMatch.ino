byte CheckSurnameMatch(String s_input, String s_list, byte* syntax_errors, byte max_errors) {          //разные варианты совпадения строк и их сравнения
  s_input.trim();
  s_list.trim();
  if (s_input == s_list) return 1;                                            //полностью сошлись
  if (s_input.length() != s_list.length()) return 0;                          //не сошлись вообще
  for (byte i = 0; i < s_input.length() && i < s_list.length(); i+=2) {
    String symbol1 = s_input.substring(i, i+2);
    String symbol2 = s_list.substring(i, i+2);
    if (symbol1 != symbol2) {
      (*syntax_errors)++;
      if (*syntax_errors > max_errors) return 0;
    }
  }
  return 2;                                                                  //сошлись с допустимым количеством ошибок
}