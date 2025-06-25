void newMsg(FB_msg& msg) {
  bool undefined_user = true;
  static String last_undefined = "";

  if (msg.chatID == last_undefined) return;       //неизвестный пишет снова. Просто игнорируем

  for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
    if (msg.chatID == Admins[i])  {
      undefined_user = false;
      break;
    }
  }

  if (undefined_user) {
    for (byte i = 0; i < sizeof(Groups)/sizeof(Groups[0]); i++) {
      if (msg.chatID == Groups[i])  {
      undefined_user = false;
      break;
    }
    }
  }

  if (undefined_user) {       //отправитель сообщения не задан как админ или поддерживаемая группа, с такими не общаемся
    last_undefined = msg.chatID;
    bot.sendMessage(msg.username + ", по всей информации обращайтесь к старосте (" + GROUP_COMMANDER + ") или в общую группу!", msg.chatID);
    return;
  }

  //-----------------------------------------Обработка ТОЛЬКО чатов с админами------------------------------------------
  for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
    if (msg.chatID == Admins[i]) {

      if (msg.OTA) bot.update();

      else if (msg.text == "/start")  {
        for (String adm: Admins) {
          if (adm == msg.chatID) bot.sendMessage("Приветствую, приветствую, " + msg.username + "!", msg.chatID);
          else  bot.sendMessage("Пользователь \"" + msg.username + "\" присоединился к нам!", adm);
        }
      }

      else if (msg.text == "/res") {
        bot.tickManual();
        bot.sendMessage("Перезагружаюсь!", msg.chatID);
        ESP.restart();
      }
      
      else if (msg.data != "") {
        Text parse(msg.data);
        menu.menuEdit(parse.decodeUnicode(), msg.chatID);
      }

      else {
        Text mess_text(msg.text);
        briefInput(mess_text.decodeUnicode(), msg.chatID);                                    //обработка возможного сокращенного ввода
      }
      break;     
    }
  }

  //-------------------------------------------Обработка ТОЛЬКО групп------------------------------------------
  for (int i = 0; i < sizeof(Groups)/sizeof(Groups[0]); i++) {                   
    if (msg.chatID == Groups[i]) {
      if (msg.text == BOT_USERNAME) bot.replyMessage("Чо случилось? Список моих возможностей можно посмотреть с помощью /comms", bot.lastUsrMsg());
    }
  }



  //----------------------------------------------Обработка всех чатов вместе-------------------------------------------
  if (msg.text == "Кинуть кубик" || msg.text == "Бросить кубик")  bot.replyMessage(msg.username + ", выпало число: " + String(random(UINT_MAX)%6+1), bot.lastUsrMsg());      //добавить рандом для числа из пользовательнского диапазона
  else if (msg.text == "/comms")  {
    bot.sendTyping(msg.chatID);
    commandList(bot.lastUsrMsg());
  }



  else if (msg.text.startsWith("/")) bot.replyMessage("А вот щас вообще не понял, что вы хотите от меня?", bot.lastUsrMsg());
}