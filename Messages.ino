void newMsg(FB_msg& msg) {
  bool admin_flag = false;

  //-----------------------------------------Обработка ТОЛЬКО чатов с админами------------------------------------------
  for (byte i = 0; i < AdmNum; i++) {
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

      admin_flag = true;
      break;     
    }
  }

  //-------------------------------------------Обработка ТОЛЬКО публичных чатов------------------------------------------
  if (!admin_flag) {                    
    if (msg.text == BOT_USERNAME) bot.replyMessage("Чо случилось? Список моих возможностей можно посмотреть с помощью /comms", bot.lastUsrMsg());

  }



  //----------------------------------------------Обработка всех чатов вместе-------------------------------------------
  if (msg.text == "Кинуть кубик" || msg.text == "Бросить кубик")  bot.replyMessage(msg.username + ", выпало число: " + String(random(UINT_MAX)%6+1), bot.lastUsrMsg());      //добавить рандом для числа из пользовательнского диапазона
  else if (msg.text == "/comms")  {
    bot.sendTyping(msg.chatID);
    commandList(bot.lastUsrMsg());
  }



  else if (msg.text.startsWith("/")) bot.replyMessage("А вот щас вообще не понял, что вы хотите от меня?", bot.lastUsrMsg());
}