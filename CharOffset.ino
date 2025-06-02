/*String charOffset(String str, int off) {
	String ret = "";
	char c, d = 'A';

	if (str.length() == 1) {
		c = str[0];
		d = '@';
	}

	else if (str.length() == 2) {
		c = str[1];
		d = str[0];
	}

	else {
    bot.sendMessage("err_charOffset", error_chat);
    return str;
  }

	if (int(c) + off > 90) {
		ret = (char(int('A') + (((int(c) + off) / 90) - 1) + (int(d) - 64)));
		ret += ((int('A') + off) % 90 - 1);
	}
	else {
		if (str.length() == 2) ret = d;
		ret += (char(int(c) + off));
	}

	return ret;
}*/
/*
String charOffset(String str, int off) {
    String ret = "";
    char c;
    char d = 'A';

    if (str.length() == 1) {
        c = str[0];
    }
    else if (str.length() == 2) {
        c = str[1];
        d = str[0];
    }
    else {
      bot.sendMessage("err_charOffset", error_chat);
        return str;
    }

    if (int(c) + off > int('Z')) {
        int newOffset = (int(c) + off - int('Z') - 1);

        if (str.length() == 2) {
            if (d == 'Z') {
                d = 'A';
            }
            else {
                d = char(int(d) + 1);
            }
            ret += d;
        }
        else {
            ret += 'A';
        }

        ret += char(int('A') + newOffset % 26);
    }
    else {
        if (str.length() == 2) ret = d;
        ret += char(int(c) + off);
    }

    return ret;
}*/

String charOffset(String str, int off) {
  String ret = "";
  char c;
  char d = 'A'; // Значение по умолчанию для первой буквы

  if (str.length() == 0) {                            //Обработка случая пустой строки
      bot.sendMessage("err_charOffset", error_chat);
      return str;
  }
  else if (str.length() == 1) {
    c = str[0];
  }
  else if (str.length() == 2) {
    d = str[0];
    c = str[1];
  }
  else {
    bot.sendMessage("err_charOffset", error_chat);
    return str;
  }

  if (int(c) + off > int('Z')) {
    int newOffset = (int(c) + off - int('Z') - 1);

    if (str.length() == 2) {
      if (d == 'Z') {
        d = 'A';
      } else {
        d = char(int(d) + 1);
      }
      ret += d;
    } else {
      ret += 'A';
    }

    ret += char(int('A') + newOffset % 26);
  } else {
    if (str.length() == 2) ret += d;  // Добавляем d, если длина строки 2.
    ret += char(int(c) + off);
  }

  return ret;
}