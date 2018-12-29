void displayWelcomeMsg(int period) {

/* -- display of welcoming message -- */

  lcd.clear();  

  // fixed message
  char* messages[] = {

  //"01234567890123456789"     Position of display characters  
    "  TERMOREGOLAZIONE  ",    // line0
    "   CABINA ARMADIO   ",    // line1
    "                    ",    // line2
    " Ver.0.9 27/12/2018 ",    // line3
  };

  for(int i = 0; i < 4; i++){
    lcd.setCursor(0,i);
    lcd.print(messages[i]);
  }

  // rolling message
  char rollMessage = '-';
  
  for(int i = 0; i < 20; i++){
    lcd.setCursor(i,2);
    lcd.print(rollMessage);
    delay(period/20);
  }
}
