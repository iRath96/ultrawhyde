#include "FspTimer.h"

constexpr uint8_t PIN_STEP = 6;
constexpr uint8_t PIN_DIRECTION = 4;
constexpr uint8_t PIN_DISABLE = 2;

static inline void digitalWriteFast(uint8_t pin, uint8_t val) __attribute__((always_inline, unused));

struct Executor {
  static constexpr uint32_t FREQUENCY = 65536;
  static constexpr uint32_t STEPS_PER_UNIT = 2; // technically a bit more?

  // steps in first 32 bits, fractional in remainder
  int64_t currentPos = 0;
  int64_t targetPos = 0;
  int64_t velocity = 0;

  int64_t acceleration = 400*1000; // 150k works, 3200k works with sled only
  int64_t maxVelocity = uint64_t(4*1000)*1000*1000 / acceleration; // 4k works
  int64_t minVelocity = 3;
  bool prevDir = false;

  template<bool Debug = false>
  void step() {
    const int64_t previousPos = currentPos;
    const int32_t prevSteps = currentPos >> 32;
    // if (currentPos != targetPos) {
    //   currentPos += currentPos < targetPos ? +(1<<30) : -(1<<30);
    // }

    const int64_t error = targetPos - currentPos;
    const int64_t dec = velocity * velocity * (acceleration / 2);
    const bool needsBreaking = abs(error) < dec;
    if (needsBreaking) {
      velocity += (velocity > 0 ? -1 : +1);
    } else {
      velocity += (error > 0 ? +1 : -1);
    }
    if (abs(velocity) > maxVelocity) velocity = velocity > 0 ? maxVelocity : -maxVelocity;
    if (abs(velocity) >= minVelocity) {
      currentPos += acceleration * velocity;
    }
    
    const int32_t curSteps = currentPos >> 32;

    if (abs(curSteps - prevSteps) > 1) {
      //digitalWrite(PIN_LED, true);
      currentPos = previousPos;
      return;
    }

    if (curSteps == prevSteps) return;

    const bool dir = curSteps > prevSteps;
    if (dir != prevDir) {
      prevDir = dir;
      digitalWriteFast(PIN_DIRECTION, dir);
      delayMicroseconds(5);
      currentPos = previousPos;
      return;
    }

    digitalWriteFast(PIN_STEP, prevSteps != curSteps);
    delayMicroseconds(3);
    digitalWriteFast(PIN_STEP, false);
    delayMicroseconds(3);

    // if (Serial.available()) {
    //   uint16_t pos;
    //   Serial.readBytes((uint8_t*)&pos, 2);
    //   setPos(pos);
    // }
    // digitalWriteFast(PIN_STEP, false);
  }

  int getPos() const {
    return (currentPos >> 32) / STEPS_PER_UNIT;
  }

  int getVelocity() const {
    return velocity * acceleration / (STEPS_PER_UNIT * FREQUENCY);
  }

  void setPos(int pos) {
    if (pos < 0 || pos > 9000) { // prev 12000
      // perhaps desync happened?
      //while (Serial.available()) Serial.read();
      return;
    }
    targetPos = int64_t(pos * STEPS_PER_UNIT) << 32;
  }
} executor;

FspTimer executorTimer;
void timer_callback(timer_callback_args_t __attribute((unused)) *p_args) {
  executor.step();
}

bool beginTimer(float rate) {
  uint8_t timer_type = GPT_TIMER;
  int8_t tindex = FspTimer::get_available_timer(timer_type);
  if (tindex < 0) {
    tindex = FspTimer::get_available_timer(timer_type, true);
  }
  if (tindex < 0) return false;

  FspTimer::force_use_of_pwm_reserved_timer();
  return (
    executorTimer.begin(TIMER_MODE_PERIODIC, timer_type, tindex, rate, 0.0f, timer_callback) &&
    executorTimer.setup_overflow_irq() &&
    executorTimer.open() &&
    executorTimer.start()
  );
}

void setup() {
  //Serial.begin(1000000);
  Serial.begin(115200);
  
  pinMode(PIN_DIRECTION, OUTPUT);
  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DISABLE, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_DISABLE, true);
  delay(4000);
  digitalWrite(PIN_DISABLE, false);

  beginTimer(Executor::FREQUENCY);
  digitalWrite(PIN_LED, false);
}

void loop() {
  if (Serial.available()) {
    uint16_t pos;
    Serial.readBytes((uint8_t*)&pos, 2);
    executor.setPos(pos);
  }

  static unsigned long lastT = micros();
  unsigned long nowT = micros();
  if ((nowT - lastT) > 4*1000) {
    uint16_t data[2] = { executor.getPos(), executor.getVelocity() };
    // digitalWrite(PIN_LED, false);
    Serial.write_raw((uint8_t*)data, 4);
    // digitalWrite(PIN_LED, true);
    lastT = nowT;
  }
}

R_PORT0_Type *port_table[] = { R_PORT0, R_PORT1, R_PORT2, R_PORT3, R_PORT4, R_PORT5, R_PORT6, R_PORT7 };
static const uint16_t mask_table[] = { 1 << 0, 1 << 1, 1 << 2, 1 << 3, 1 << 4, 1 << 5, 1 << 6, 1 << 7,
                                       1 << 8, 1 << 9, 1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15 };

// Cause a digital pin to output either HIGH or LOW.  The pin must
// have been configured with pinMode().  This fast version of
// digitalWrite has minimal overhead when the pin number is a
// constant.  Successive digitalWriteFast without delays can be
// too quick in many applications!
static inline void digitalWriteFast(uint8_t pin, uint8_t val) {
  if (__builtin_constant_p(pin)) {
    if (val) {
      if (pin == 0) {
        port_table[g_pin_cfg[0].pin >> 8]->POSR = mask_table[g_pin_cfg[0].pin & 0xff];
      } else if (pin == 1) {
        port_table[g_pin_cfg[1].pin >> 8]->POSR = mask_table[g_pin_cfg[1].pin & 0xff];
      } else if (pin == 2) {
        port_table[g_pin_cfg[2].pin >> 8]->POSR = mask_table[g_pin_cfg[2].pin & 0xff];
      } else if (pin == 3) {
        port_table[g_pin_cfg[3].pin >> 8]->POSR = mask_table[g_pin_cfg[3].pin & 0xff];
      } else if (pin == 4) {
        port_table[g_pin_cfg[4].pin >> 8]->POSR = mask_table[g_pin_cfg[4].pin & 0xff];
      } else if (pin == 5) {
        port_table[g_pin_cfg[5].pin >> 8]->POSR = mask_table[g_pin_cfg[5].pin & 0xff];
      } else if (pin == 6) {
        port_table[g_pin_cfg[6].pin >> 8]->POSR = mask_table[g_pin_cfg[6].pin & 0xff];
      } else if (pin == 7) {
        port_table[g_pin_cfg[7].pin >> 8]->POSR = mask_table[g_pin_cfg[7].pin & 0xff];
      } else if (pin == 8) {
        port_table[g_pin_cfg[8].pin >> 8]->POSR = mask_table[g_pin_cfg[8].pin & 0xff];
      } else if (pin == 9) {
        port_table[g_pin_cfg[9].pin >> 8]->POSR = mask_table[g_pin_cfg[9].pin & 0xff];
      } else if (pin == 10) {
        port_table[g_pin_cfg[10].pin >> 8]->POSR = mask_table[g_pin_cfg[10].pin & 0xff];
      } else if (pin == 11) {
        port_table[g_pin_cfg[11].pin >> 8]->POSR = mask_table[g_pin_cfg[11].pin & 0xff];
      } else if (pin == 12) {
        port_table[g_pin_cfg[12].pin >> 8]->POSR = mask_table[g_pin_cfg[12].pin & 0xff];
      } else if (pin == 13) {
        port_table[g_pin_cfg[13].pin >> 8]->POSR = mask_table[g_pin_cfg[13].pin & 0xff];
      } else if (pin == 14) {
        port_table[g_pin_cfg[14].pin >> 8]->POSR = mask_table[g_pin_cfg[14].pin & 0xff];
      } else if (pin == 15) {
        port_table[g_pin_cfg[15].pin >> 8]->POSR = mask_table[g_pin_cfg[15].pin & 0xff];
      } else if (pin == 16) {
        port_table[g_pin_cfg[16].pin >> 8]->POSR = mask_table[g_pin_cfg[16].pin & 0xff];
      } else if (pin == 17) {
        port_table[g_pin_cfg[17].pin >> 8]->POSR = mask_table[g_pin_cfg[17].pin & 0xff];
      } else if (pin == 18) {
        port_table[g_pin_cfg[18].pin >> 8]->POSR = mask_table[g_pin_cfg[18].pin & 0xff];
      } else if (pin == 19) {
        port_table[g_pin_cfg[19].pin >> 8]->POSR = mask_table[g_pin_cfg[19].pin & 0xff];
      } else if (pin == 20) {
        port_table[g_pin_cfg[20].pin >> 8]->POSR = mask_table[g_pin_cfg[20].pin & 0xff];
      } else if (pin == 21) {
        port_table[g_pin_cfg[21].pin >> 8]->POSR = mask_table[g_pin_cfg[21].pin & 0xff];
      } else if (pin == 22) {
        port_table[g_pin_cfg[22].pin >> 8]->POSR = mask_table[g_pin_cfg[22].pin & 0xff];
      } else if (pin == 23) {
        port_table[g_pin_cfg[23].pin >> 8]->POSR = mask_table[g_pin_cfg[23].pin & 0xff];
      } else if (pin == 24) {
        port_table[g_pin_cfg[24].pin >> 8]->POSR = mask_table[g_pin_cfg[24].pin & 0xff];
      } else if (pin == 25) {
        port_table[g_pin_cfg[25].pin >> 8]->POSR = mask_table[g_pin_cfg[25].pin & 0xff];
      } else if (pin == 26) {
        port_table[g_pin_cfg[26].pin >> 8]->POSR = mask_table[g_pin_cfg[26].pin & 0xff];
      } else if (pin == 27) {
        port_table[g_pin_cfg[27].pin >> 8]->POSR = mask_table[g_pin_cfg[27].pin & 0xff];
      } else if (pin == 28) {
        port_table[g_pin_cfg[28].pin >> 8]->POSR = mask_table[g_pin_cfg[28].pin & 0xff];
      } else if (pin == 29) {
        port_table[g_pin_cfg[29].pin >> 8]->POSR = mask_table[g_pin_cfg[29].pin & 0xff];
      } else if (pin == 30) {
        port_table[g_pin_cfg[30].pin >> 8]->POSR = mask_table[g_pin_cfg[30].pin & 0xff];
      } else if (pin == 31) {
        port_table[g_pin_cfg[31].pin >> 8]->POSR = mask_table[g_pin_cfg[31].pin & 0xff];
      } else if (pin == 32) {
        port_table[g_pin_cfg[32].pin >> 8]->POSR = mask_table[g_pin_cfg[32].pin & 0xff];
      } else if (pin == 33) {
        port_table[g_pin_cfg[33].pin >> 8]->POSR = mask_table[g_pin_cfg[33].pin & 0xff];
      } else if (pin == 34) {
        port_table[g_pin_cfg[34].pin >> 8]->POSR = mask_table[g_pin_cfg[34].pin & 0xff];
      } else if (pin == 35) {
        port_table[g_pin_cfg[35].pin >> 8]->POSR = mask_table[g_pin_cfg[35].pin & 0xff];
      } else if (pin == 36) {
        port_table[g_pin_cfg[36].pin >> 8]->POSR = mask_table[g_pin_cfg[36].pin & 0xff];
      } else if (pin == 37) {
        port_table[g_pin_cfg[37].pin >> 8]->POSR = mask_table[g_pin_cfg[37].pin & 0xff];
      } else if (pin == 38) {
        port_table[g_pin_cfg[38].pin >> 8]->POSR = mask_table[g_pin_cfg[38].pin & 0xff];
      }
    } else {
      if (pin == 0) {
        port_table[g_pin_cfg[0].pin >>8]->PORR = mask_table[g_pin_cfg[0].pin & 0xff];
      } else if (pin == 1) {
        port_table[g_pin_cfg[1].pin >>8]->PORR = mask_table[g_pin_cfg[1].pin & 0xff];
      } else if (pin == 2) {
        port_table[g_pin_cfg[2].pin >>8]->PORR = mask_table[g_pin_cfg[2].pin & 0xff];
      } else if (pin == 3) {
        port_table[g_pin_cfg[3].pin >>8]->PORR = mask_table[g_pin_cfg[3].pin & 0xff];
      } else if (pin == 4) {
        port_table[g_pin_cfg[4].pin >>8]->PORR = mask_table[g_pin_cfg[4].pin & 0xff];
      } else if (pin == 5) {
        port_table[g_pin_cfg[5].pin >>8]->PORR = mask_table[g_pin_cfg[5].pin & 0xff];
      } else if (pin == 6) {
        port_table[g_pin_cfg[6].pin >>8]->PORR = mask_table[g_pin_cfg[6].pin & 0xff];
      } else if (pin == 7) {
        port_table[g_pin_cfg[7].pin >>8]->PORR = mask_table[g_pin_cfg[7].pin & 0xff];
      } else if (pin == 8) {
        port_table[g_pin_cfg[8].pin >>8]->PORR = mask_table[g_pin_cfg[8].pin & 0xff];
      } else if (pin == 9) {
        port_table[g_pin_cfg[9].pin >>8]->PORR = mask_table[g_pin_cfg[9].pin & 0xff];
      } else if (pin == 10) {
        port_table[g_pin_cfg[10].pin >>8]->PORR = mask_table[g_pin_cfg[10].pin & 0xff];
      } else if (pin == 11) {
        port_table[g_pin_cfg[11].pin >>8]->PORR = mask_table[g_pin_cfg[11].pin & 0xff];
      } else if (pin == 12) {
        port_table[g_pin_cfg[12].pin >>8]->PORR = mask_table[g_pin_cfg[12].pin & 0xff];
      } else if (pin == 13) {
        port_table[g_pin_cfg[13].pin >>8]->PORR = mask_table[g_pin_cfg[13].pin & 0xff];
      } else if (pin == 14) {
        port_table[g_pin_cfg[14].pin >>8]->PORR = mask_table[g_pin_cfg[14].pin & 0xff];
      } else if (pin == 15) {
        port_table[g_pin_cfg[15].pin >>8]->PORR = mask_table[g_pin_cfg[15].pin & 0xff];
      } else if (pin == 16) {
        port_table[g_pin_cfg[16].pin >>8]->PORR = mask_table[g_pin_cfg[16].pin & 0xff];
      } else if (pin == 17) {
        port_table[g_pin_cfg[17].pin >>8]->PORR = mask_table[g_pin_cfg[17].pin & 0xff];
      } else if (pin == 18) {
        port_table[g_pin_cfg[18].pin >>8]->PORR = mask_table[g_pin_cfg[18].pin & 0xff];
      } else if (pin == 19) {
        port_table[g_pin_cfg[19].pin >>8]->PORR = mask_table[g_pin_cfg[19].pin & 0xff];
      } else if (pin == 20) {
        port_table[g_pin_cfg[20].pin >>8]->PORR = mask_table[g_pin_cfg[20].pin & 0xff];
      } else if (pin == 21) {
        port_table[g_pin_cfg[21].pin >>8]->PORR = mask_table[g_pin_cfg[21].pin & 0xff];
      } else if (pin == 22) {
        port_table[g_pin_cfg[22].pin >>8]->PORR = mask_table[g_pin_cfg[22].pin & 0xff];
      } else if (pin == 23) {
        port_table[g_pin_cfg[23].pin >>8]->PORR = mask_table[g_pin_cfg[23].pin & 0xff];
      } else if (pin == 24) {
        port_table[g_pin_cfg[24].pin >>8]->PORR = mask_table[g_pin_cfg[24].pin & 0xff];
      } else if (pin == 25) {
        port_table[g_pin_cfg[25].pin >>8]->PORR = mask_table[g_pin_cfg[25].pin & 0xff];
      } else if (pin == 26) {
        port_table[g_pin_cfg[26].pin >>8]->PORR = mask_table[g_pin_cfg[26].pin & 0xff];
      } else if (pin == 27) {
        port_table[g_pin_cfg[27].pin >>8]->PORR = mask_table[g_pin_cfg[27].pin & 0xff];
      } else if (pin == 28) {
        port_table[g_pin_cfg[28].pin >>8]->PORR = mask_table[g_pin_cfg[28].pin & 0xff];
      } else if (pin == 29) {
        port_table[g_pin_cfg[29].pin >>8]->PORR = mask_table[g_pin_cfg[29].pin & 0xff];
      } else if (pin == 30) {
        port_table[g_pin_cfg[30].pin >>8]->PORR = mask_table[g_pin_cfg[30].pin & 0xff];
      } else if (pin == 31) {
        port_table[g_pin_cfg[31].pin >>8]->PORR = mask_table[g_pin_cfg[31].pin & 0xff];
      } else if (pin == 32) {
        port_table[g_pin_cfg[32].pin >>8]->PORR = mask_table[g_pin_cfg[32].pin & 0xff];
      } else if (pin == 33) {
        port_table[g_pin_cfg[33].pin >>8]->PORR = mask_table[g_pin_cfg[33].pin & 0xff];
      } else if (pin == 34) {
        port_table[g_pin_cfg[34].pin >>8]->PORR = mask_table[g_pin_cfg[34].pin & 0xff];
      } else if (pin == 35) {
        port_table[g_pin_cfg[35].pin >>8]->PORR = mask_table[g_pin_cfg[35].pin & 0xff];
      } else if (pin == 36) {
        port_table[g_pin_cfg[36].pin >>8]->PORR = mask_table[g_pin_cfg[36].pin & 0xff];
      } else if (pin == 37) {
        port_table[g_pin_cfg[37].pin >>8]->PORR = mask_table[g_pin_cfg[37].pin & 0xff];
      } else if (pin == 38) {
        port_table[g_pin_cfg[38].pin >>8]->PORR = mask_table[g_pin_cfg[38].pin & 0xff];
      }
    }
  } else {
    digitalWrite(pin, val);
  }
}
