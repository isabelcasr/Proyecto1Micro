#include <Arduino.h>
#include <LiquidCrystal.h>

// pines usados por el panel LCD
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// definición de constantes para los botones
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

//tamaño del campo del juego
#define WIDTH 16
#define HEIGHT 4

//sprites personalizados
#define SHIP 0
#define BULLET_UP 1
#define BULLET_DOWN 2
#define SHIP_BULLET 3
#define ALIEN1 4
#define ALIEN2 5
#define ALIEN1BULLET 6
#define ALIEN2BULLET 7

#define GAME_STEP 100 //Retardo (ms) entre pasos del juego
#define ALIENS_NUM 8 //Numero de aliens

byte animationStep; //Número de paso en el juego
char screenBuffer[HEIGHT/2][WIDTH+1]; //Caracteres a mostrar en la pantalla
byte alienStep = 5; //Número de pasos del juego entre movimientos de los aliens
byte fireProbability = 20; //Probabilidad de que un alien dispare
int score = 0; // Puntaje del jugador
byte level = 1; // Nivel del juego
byte aliensLeft = 0; // Número de aliens restantes en el nivel actual

// ---------- INTEGRACIÓN MELODÍA (BUZZER) ----------
// Pin del buzzer (ajusta si lo conectaste a otro pin)
#define BUZZER 8

// Notas (frecuencias en Hz) usadas en la melodía
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_Eb4 311
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_Bb3 233

// Melodía (fragmento de Kirby que definiste)
int melody[] = {
  NOTE_C4, NOTE_G4, NOTE_Eb4, NOTE_D4, NOTE_C4, NOTE_C4, NOTE_D4, NOTE_Eb4,
  NOTE_C4, NOTE_Bb3, NOTE_C4, NOTE_G4, NOTE_C4, NOTE_G4, NOTE_Eb4, NOTE_D4,
  NOTE_C4, NOTE_C4, NOTE_D4, NOTE_Eb4, NOTE_F4, NOTE_D4, NOTE_Bb3, NOTE_C4, NOTE_G4, NOTE_C4
};

// Duraciones relativas (4=negra, 8=corchea, etc.) — ahora todas corcheas como quedamos
int noteDurations[] = {
  8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8
};

int numNotes = sizeof(melody) / sizeof(melody[0]);
int currentNote = 0;
unsigned long lastNoteTime = 0;
int tempo = 120; // BPM, más lento para que se entienda bien

// Función que reproduce la melodía sin bloquear el loop del juego
void playMelodyNonBlocking() {
  unsigned long now = millis();
  int wholenote = (60000 * 4) / tempo; // duración de una "negra" en ms multiplicada por 4 para wholenote
  int divider = noteDurations[currentNote];
  if (divider == 0) divider = 4; // seguridad (si hay silencio representado) — no usado aquí
  int noteDuration = wholenote / divider;

  if (now - lastNoteTime >= (unsigned long)noteDuration) {
    lastNoteTime = now;
    int freq = melody[currentNote];
    if (freq != 0) {
      // reproducir la nota (tone no bloquea el código principal)
      tone(BUZZER, freq, (unsigned long)(noteDuration * 0.9));
    } else {
      // silencio: aseguramos que noTone corte sonido si lo hubiera
      noTone(BUZZER);
    }
    currentNote++;
    if (currentNote >= numNotes) currentNote = 0; // repetir indefinidamente
  }
}
// --------------------------------------------------

// Definición de caracteres personalizados para los sprites del juego
byte ship_sprite[] = {
B00000,
B00000,
B00000,
B00000,
B00000,
B00100,
B01110,
B11011
};

byte ship_bullet_sprite[] = {
B00000,
B00100,
B00100,
B00000,
B00000,
B00100,
B01110,
B11011
};

byte bullet_down_sprite[] = {
B00000,
B00000,
B00000,
B00000,
B00000,
B00100,
B00100,
B00000
};

byte bullet_up_sprite[] = {
B00000,
B00100,
B00100,
B00000,
B00000,
B00000,
B00000,
B00000
};

byte alien1_1_sprite[] = {
B01010,
B10101,
B01110,
B10001,
B00000,
B00000,
B00000,
B00000
};

byte alien1_2_sprite[] = {
B01010,
B10101,
B01110,
B01010,
B00000,
B00000,
B00000,
B00000
};

byte alien1_1_bullet_sprite[] = {
B01010,
B10101,
B01110,
B10001,
B00000,
B00100,
B00100,
B00000
};

byte alien1_2_bullet_sprite[] = {
B01010,
B10101,
B01110,
B01010,
B00000,
B00100,
B00100,
B00000
};

//Clase base para los objetos del juego
class GameObject
{
// Coordenadas y velocidad del objeto
protected:
  int8_t _x;
  int8_t _y;
  int8_t _speed;
public:
  GameObject():_x(0),_y(0),_speed(0){}
  GameObject(int8_t x, int8_t y): _x(x), _y(y), _speed(0){}
  GameObject(int8_t x, int8_t y, int8_t speed): _x(x), _y(y), _speed(speed){}
//Getters and setters
  int8_t x() const{
    return _x;
  }
  int8_t y() const{
    return _y;
  }
  int8_t speed() const{
    return _speed;
  }
  bool setX(int8_t x){
    if ((x<0)||(x>WIDTH))
      return false;
    _x = x;
    return true;
  }
  bool setY(int8_t y){
    if ((y<0)||(y>HEIGHT))
      return false;
    _y = y;
    return true;
  }
  bool setSpeed(int8_t speed){
    _speed = speed;
    return true;
  }
// Detección de colisiones
  bool collides(const GameObject& o){
    return ((_x==o.x())&&(_y==o.y())) ? true : false;
  }
};

// Clase para las balas
class Bullet:public GameObject
{
private:
  bool _active;//Bullet is active while it is within game field
public:
  Bullet():GameObject(), _active(false){}

  void setActive(bool active){
    _active = active;
  }

  bool active(){
    return _active;
  }
  
/* Movimiento de la bala. Devuelve true si tuvo éxito */
  bool move(){
    _y+=_speed;//for bullets speed is always vertical
    if ((_y<0)||(_y>=HEIGHT)){//if bullet leaves the field
      _y-=_speed;
      _active = false;
      return false;
    }
    else return true;
  }
} shipBullet, alienBullets[ALIENS_NUM];//bullet objects for ship and aliens

/*Clase paa la nave*/
class Ship:public GameObject
{
public:
// Movimiento hacia la derecha. Devuelve true si tuvo éxito
  bool moveRight(){
    _x++;
    if (_x>=WIDTH){
      _x--;
      return false;
    }
    else return true;
  }
// Movimiento hacia la izquierda. Devuelve true si tuvo éxito
  bool moveLeft(){
    _x--;
    if (_x<0){
      _x++;
      return false;
    }
    else return true;
  }
} ship;

//Clase para los aliens
class Alien: public GameObject
{
private:
  bool _alive;//shows wether alien is alive
  bool _state;//alien's current state for animation purpose
public:
  Alien():GameObject(), _alive(false), _state(false){}

  void setAlive(bool alive){
    _alive = alive;
  }

  bool alive(){
    return _alive;
  }

  void setState(bool state){
    _state = state;
  }

  bool state(){
    return _state;
  }
// Movimiento del alien. Devuelve true si tuvo éxito
  bool move(){
    _x+=_speed;
    _state = !_state;
    if ((_x<0)||(_x>=WIDTH)){
      _x-=_speed;
      return false;
    }
    else return true;
  }
}aliens[8];

// Procesamiento del estado de los botones
byte buttonPressed()
{
 int adc_key_in = analogRead(0);      // leer valor analógico
// Ajustar estos valores según tu versión del shield LCD
 if (adc_key_in <= 60)   return btnRIGHT;
 if (adc_key_in <= 200)  return btnUP;
 if (adc_key_in <= 400)  return btnDOWN;
 if (adc_key_in <= 600)  return btnLEFT;
 if (adc_key_in <= 800)  return btnSELECT;
 return btnNONE;
}

/*Actualizar pantalla LCD
primero: se dibuja en un buffet de caracteres, luego se imprime en la pantalla para evitar parpadeo.
Nota: tenemos que dibujar la nave por separado ya que tiene el codigo de caracter 0 y lcd.print() lo procesa como EDL */
void updateScreen(){
  bool shipDisplayed = false; // indica si la nave ya fue dibujada con el sprite SHIP_BULLET
// Limpieza del buffer
  for (byte i = 0; i < HEIGHT/2; i++){
    for (byte j = 0; j < WIDTH; j++)
      screenBuffer[i][j] = ' ';
    screenBuffer[i][WIDTH] = '\0';
  }
// Dibujando la bala de la nave
  if (shipBullet.active()){
    if ((ship.x()==shipBullet.x()) && (shipBullet.y()==2)){
      screenBuffer[shipBullet.y()/2][shipBullet.x()] = SHIP_BULLET;
      shipDisplayed = true;
    }
    else
      screenBuffer[shipBullet.y()/2][shipBullet.x()] = shipBullet.y()%2 ? BULLET_DOWN : BULLET_UP;
  }
//Dibujar los aliens
  for (byte i = 0; i<ALIENS_NUM; i++){
    if(aliens[i].alive()){
      screenBuffer[aliens[i].y()/2][aliens[i].x()] = aliens[i].state() ? ALIEN1 : ALIEN2;
    }
  }
// Dibujar los aliens y las balas
  bool bulletDisplayed = false;
  for (byte i = 0; i < ALIENS_NUM; i++){
    if(alienBullets[i].active()){
      bulletDisplayed = false;
      for (int j = 0; j < ALIENS_NUM; j++){
        if ((aliens[j].x()==alienBullets[i].x()) && (alienBullets[i].y()==1) && (aliens[i].alive())){
          screenBuffer[alienBullets[i].y()/2][alienBullets[i].x()] = aliens[i].state() ? ALIEN1BULLET : ALIEN2BULLET;
          bulletDisplayed = true;
        }
      }
      if (!bulletDisplayed){
        if ((ship.x()==alienBullets[i].x()) && (alienBullets[i].y()==2)){
          screenBuffer[alienBullets[i].y()/2][alienBullets[i].x()] = SHIP_BULLET;
          shipDisplayed = true;
        }
        else
          screenBuffer[alienBullets[i].y()/2][alienBullets[i].x()] = alienBullets[i].y()%2 ? BULLET_DOWN : BULLET_UP;
      }
    }
  }
// Enviando el buffer a la pantalla
  for (byte i = 0; i < HEIGHT/2; i++){
    lcd.setCursor(0,i);
    lcd.print(screenBuffer[i]);
  }
//Finalmente, mostrando la nave
  if (!shipDisplayed){
    lcd.setCursor(ship.x(), ship.y()/2);
    lcd.write(byte(SHIP));
  }
}

//Reiniciar todos los objetos antes de cada nivel
void initLevel(byte l)
{
  level = l;
  if (level>42)//Easter egg: 42 es el ultimate level:)
    level = 42;
//Reiniciar objeto nave
  ship.setX(WIDTH/2);
  ship.setY(3);
  shipBullet.setX(WIDTH/2);
  shipBullet.setY(3);
  shipBullet.setActive(false);
//Reiniciar objeto aliens
  for (byte i = 0; i<ALIENS_NUM; i++){
     aliens[i].setX(i);
     aliens[i].setY(0);
     aliens[i].setSpeed(1);
     aliens[i].setAlive(true);
     aliens[i].setState(false);
     alienBullets[i].setActive(false);
  }
//Reiniciar el resto de variables del minijuego
  animationStep = 0;
  alienStep = 6-level/2;// la velocidad de los aliens depende del nivel
  if (alienStep < 1)
    alienStep = 1;
  fireProbability = 110-level*10;// la probabilidad de disparo de los aliens dependen del nivel
  if (fireProbability < 10)
    fireProbability = 10;
  aliensLeft = ALIENS_NUM;
// Mostrar número de nivel
  lcd.clear();
  lcd.print("Level ");
  lcd.setCursor(8,0);
  lcd.print(level);
  delay(1000);
  lcd.clear();
}

/*Mostrar puntaje final en el display*/
void gameOver()
{
  lcd.setCursor(0,0);
  lcd.print("G a m e  o v e r");
  lcd.setCursor(0,1);
  lcd.print("Score:  ");
  lcd.setCursor(8,1);
  lcd.print(score);
  while(1);
}

/*Inicialización: iniciar LCD y crear caracteres personalizados */
void setup()
{
 lcd.begin(16, 2);
 lcd.createChar(SHIP, ship_sprite);
 lcd.createChar(BULLET_UP, bullet_up_sprite);
 lcd.createChar(BULLET_DOWN, bullet_down_sprite);
 lcd.createChar(SHIP_BULLET, ship_bullet_sprite);
 lcd.createChar(ALIEN1, alien1_1_sprite);
 lcd.createChar(ALIEN2, alien1_2_sprite);
 lcd.createChar(ALIEN1BULLET, alien1_1_bullet_sprite);
 lcd.createChar(ALIEN2BULLET, alien1_2_bullet_sprite);
 score = 0;
 randomSeed(analogRead(1));
 initLevel (1);
}

/* Bucle principal del juego */
void loop()
{
//Procesar los botones
  switch(buttonPressed()){
  case btnRIGHT:{
    ship.moveRight();
    break;
  }
  case btnLEFT:{
    ship.moveLeft();
    break;
  }
  case btnDOWN:{ //Pausa del juego
    while (buttonPressed()==btnDOWN);
    while (buttonPressed()!=btnDOWN);
    while (buttonPressed()==btnDOWN);
    break;
  }

  //Disparar
  case btnSELECT:
  case btnUP:{
    if(!shipBullet.active()){
      shipBullet.setX(ship.x());
      shipBullet.setY(ship.y());
      shipBullet.setSpeed(-1);
      shipBullet.setActive(true);
    }
    break;
  }
  default:
    break;
  }
    // Reproducir la melodía en modo no bloqueante (música de fondo)
  playMelodyNonBlocking();

//Movimiento de todos los objetos
  if(shipBullet.active()) //Movimiento de la bala de la nave
    shipBullet.move();
//Movimiento de los aliens y sus balas
  for (byte i = 0; i<ALIENS_NUM; i++){
      if (alienBullets[i].active()){
        alienBullets[i].move();
        if (alienBullets[i].collides(ship)) // Destrucción de la nave
          gameOver();
      }
      if (!(animationStep%alienStep))
        aliens[i].move();
      if ((aliens[i].collides(shipBullet))&&(shipBullet.active())&&(aliens[i].alive())){ //Muere alien
        aliens[i].setAlive(false);
        score += 10*level;
        aliensLeft--;
      }
      if ((!random(fireProbability))&&(!alienBullets[i].active())&&(aliens[i].alive())){ //Alien dispara
        alienBullets[i].setX(aliens[i].x());
        alienBullets[i].setY(aliens[i].y()+1);
        alienBullets[i].setSpeed(1);
        alienBullets[i].setActive(true);
      }
    }
    if ( (!(animationStep%alienStep))&& ((aliens[0].x()==0)||(aliens[ALIENS_NUM-1].x()==WIDTH-1)) )//Cambiando la dirección del movimiento de los aliens
        for (byte i = 0; i<ALIENS_NUM; i++)
          aliens[i].setSpeed(-aliens[i].speed());
//Refresh pantalla
  updateScreen();
  animationStep++;
  delay (GAME_STEP);
// Si no quedan aliens, iniciar siguiente nivel
  if(!aliensLeft)
    initLevel(level+1);
}