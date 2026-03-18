#include "Arduino.h"
#include "sensor_app.h"

int main() {
  SystemInit();
  Delay_Ms(100);
  appInit();
  while (1) {
    appLoopStep();
  }
}
