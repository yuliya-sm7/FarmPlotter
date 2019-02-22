#include <AccelStepper.h>
// X20Y-50Z127 пример комманды управления, относительные перемещения

typedef struct {
  long x;
  long y;
  long z;
} Point;

const int n = 3; // количество ШД
long LIM[n] = {550, 1000, 400}; // предельные расстояния
bool fstop = true; // флаг остановки
int N = 32; // длина змейки
int k = 0;  // номер текущей целевой точки змейки

long wayX[32]; // миллиметры
long wayY[32];
long wayZ[32];
long* way[n] = {wayX, wayY, wayZ};

void initMotors();
void initSnake(long** way);
long total_distance();
Point run_together(Point A); // все двигатели одновременно
void inputCommand(char * command);
void inject();
void control(char *command);
void Snake();
bool parseXYZ(char* m, Point &A);
int readNum(char* m, int i);

const int       X_STP = 2;
const int       X_DIR = 3;
const int       Y_STP = 4;
const int       ENB = 5;
const int       Y_DIR = 8;
const int       Z_STP = 9; //PUL+(+5v) axis stepper motor step control
const int       Z_DIR = 10; //DIR+(+5v) axis stepper motor direction control

const int       X_SENS = 53; // концевой выключатель, LOW - активный уровень
const int       Y_SENS = 52;
const int       Z_SENS = 51;

const int       LED = 13;
const int       INJ1 = 11;  // помпы
const int       INJ2 = 12;  // помпы
const int       DRL =  6;  // сверло

int STP[n] =    {X_STP, Y_STP, Z_STP};
int DIR[n] =    {X_DIR, Y_DIR, Z_DIR};

AccelStepper    stepperX(AccelStepper::DRIVER, X_STP, X_DIR);
AccelStepper    stepperY(AccelStepper::DRIVER, Y_STP, Y_DIR);
AccelStepper    stepperZ(AccelStepper::DRIVER, Z_STP, Z_DIR);
AccelStepper    Motors[n] = {stepperX, stepperY, stepperZ};
int             zero[n] = {X_SENS, Y_SENS, Z_SENS};

/////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  pinMode(ENB, OUTPUT);
  digitalWrite(ENB, LOW); // общая земля - enable
  pinMode(INJ1, OUTPUT);
  pinMode(INJ2, OUTPUT);
  pinMode(DRL, OUTPUT);

  for (int i = 0; i < n; i++) {
    pinMode(zero[i], INPUT_PULLUP);
    pinMode(STP[i], OUTPUT);
    pinMode(DIR[i], OUTPUT);
  }
  initSnake(way);
  initMotors();
}

/////////////////////////////////////////////////////////////////////////
void loop() {
  if (fstop == false) {
    Snake();
  }
}

/////////////////////////////////////////////////////////////////////////
void serialEvent() {
  char command[32] = "";
  inputCommand(command);

  if (strstr(command, "stop") != NULL) {
    run_together({ 0, 0, 0});
    fstop = true;
    Serial.println("stopped!");
  }
  else if (strstr(command, "run") != NULL) {
    if (fstop == true) {
      fstop = false;
      if (k > 0)
        k--;
    }
    Serial.println("snake_ran!");
  }
  else if (strstr(command, "drill on") != NULL) {
    drill(true);
  }
  else if (strstr(command, "drill off") != NULL) {
    drill(false);
  }
  else if (strstr(command, "inject") != NULL) {
    inject();
  }
  else if (strstr(command, "WEED") != NULL) {
    WEED({0, -40, 200});
  }
  else if (fstop == true) {
    control(command);
  }
  else
    Serial.println("Busy!");
}

/////////////////////////////////////////////////////////////////////
void inputCommand(char * command) {
  int i = 0;
  while (Serial.available()) {
    command[i] = Serial.read();
    i++;
    delay(1);
  }
  strcat(command, "\0");
  Serial.print(command);
}

//////////////////////////////////////////////////////////////////////
void inject() {
  Serial.println("inject start");
  digitalWrite(INJ1, HIGH);
  digitalWrite(INJ2, HIGH);
  delay(3000);
  digitalWrite(INJ1, LOW);
  digitalWrite(INJ2, LOW);
  Serial.println("inject stop");
}

////////////////////////////////////////////////////////////////////////
void drill(bool sw) {
  if (sw) {
    Serial.println("dril start");
    analogWrite(DRL, 50);
  }
  else {
    Serial.println("drill stop");
    analogWrite(DRL, 0);
  }
}

/////////////////////////////////////////////////////////////////////
void WEED(Point A) {
  drill(true);
  Point B = run_together(A);
  delay(500);
  drill(false);
  inject();
  B = { -B.x, -B.y, -B.z};
  run_together(B); // возвращение
  Serial.println("done");
  k--;
}

/////////////////////////////////////////////////////////////////////
void control(char *command) {
  Point A = {0, 0, 0};
  if (parseXYZ(command, A) == false) {
    Serial.println("Parse_fail!");
    return;
  }
  run_together(A);
  Serial.println("On point");
}

////////////////////////////////////////////////////////////////////
void initSnake(long** way) {
  for (int i = 0; i < N / 4; i++) {
    way[0][4 * i] = 0;
    way[0][4 * i + 1] = 550;
    way[0][4 * i + 2] = 550;
    way[0][4 * i + 3] = 0;
  }
  for (int i = 0; i < N / 2; i++) {
    way[1][2 * i] = i * 50;
    way[1][2 * i + 1] = i * 50;
  }
  for (int i = 0; i < N; i++) {
    way[2][i] = 0;
  }
}

////////////////////////////////////////////////////////////////////
void Snake() {
  if (total_distance() == 0) {
    k++;
    if (k >= N) {
      k = 0;
      Serial.println("new Snake begin");
    }
    Serial.println(k);
    for (int i = 0; i < n; i++) {
      Motors[i].moveTo(way[i][k] * 100);
    }
  }
  for (int i = 0; i < n; i++) {
    Motors[i].run();
  }
}
////////////////////////////////////////////////////////////////////
void initMotors() {
  Serial.print("Setup...");
  digitalWrite(LED, HIGH);
  Motors[2].setPinsInverted(true);

  for (int i = 0; i < n; i++) {
    Motors[i].setMinPulseWidth (20);
    Motors[i].setMaxSpeed (3000);
    Motors[i].setAcceleration (6000);
    Motors[i].moveTo(-600);
  }
  while (total_distance() != 0) {
    for (int i = 0; i < n; i++) {
      if (digitalRead(zero[i]) != 0) {
        Motors[i].run();
      } else {
        Motors[i].setCurrentPosition(0);
        Motors[i].moveTo(0);
      }
    }
  }
  for (int i = 0; i < n; i++)
    Motors[i].setCurrentPosition(0);
  Point indent = {20, 20, 20};
  run_together(indent);
  for (int i = 0; i < n; i++)
    Motors[i].setCurrentPosition(0);
  digitalWrite(LED, LOW);
  Serial.println("Ready");
}

/////////////////////////////////////////////////////////////////////////
Point run_together(Point A) {
  long target[n] = {A.x, A.y, A.z}; // относительные перемещения
  for (int i = 0; i < n; i++) {
    Motors[i].move(target[i] * 100);
    if (Motors[i].targetPosition() / 100 > LIM[i])
      Motors[i].moveTo(LIM[i] * 100);
    if (Motors[i].targetPosition() < 0 )
      Motors[i].moveTo(0);
    Serial.println(Motors[i].targetPosition() / 100);
  }
  Point B = {Motors[0].distanceToGo() / 100, Motors[1].distanceToGo() / 100, Motors[2].distanceToGo() / 100};
  digitalWrite(LED, HIGH);
  while (total_distance() != 0) {
    for (int i = 0; i < n; i++) {
      Motors[i].run();
    }
  }
  digitalWrite(LED, LOW);
  return B;  // реальное смещение
}

////////////////////////////////////////////////////////////////////////
long total_distance() {
  return Motors[0].distanceToGo() + Motors[1].distanceToGo() + Motors[2].distanceToGo();
}

////////////////////////////////////////////////////////////////////////
bool parseXYZ(char* m, Point &A) {
  for (int i = 0; i < 32; i++) {
    if (m[i] == '\n' || m[i] == '\r' || m[i] == '\0')
      break;
    if (m[i] == 'X') {
      i++;
      A.x = readNum(m, i);
    }
    if (m[i] == 'Y') {
      i++;
      A.y = readNum(m, i);
    }
    if (m[i] == 'Z') {
      i++;
      A.z = readNum(m, i);
    }
  }
  // проверка на адекватность будет
  return true;
}

/////////////////////////////////////////////////////////////////////////
int readNum(char* m, int i)
{
  char buff[10] = "";
  int n = 0;
  while ((isdigit(m[i]) || m[i] == '.' || m[i] == '-') && n < 9) {
    buff[n] = m[i];
    i++;
    n++;
  }
  buff[n] = '\0';
  if (n == 0)
    return (0);
  return (atoi(buff));
}
