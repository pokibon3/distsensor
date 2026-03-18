#include "Wire.h"

namespace {
volatile uint32_t g_millis = 0;
bool g_systick_started = false;
constexpr uint32_t kI2cTimeout = 100000;

void systickInit() {
  if (g_systick_started) {
    return;
  }

  SysTick->CTLR = 0;
  SysTick->CMP = DELAY_MS_TIME - 1;
  SysTick->CNT = 0;
  SysTick->SR = 0;
  g_millis = 0;
  SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
  NVIC_EnableIRQ(SysTick_IRQn);
  g_systick_started = true;
}
}  // namespace

extern "C" void SysTick_Handler(void) __attribute__((interrupt));
extern "C" void SysTick_Handler(void) {
  SysTick->CMP += DELAY_MS_TIME;
  SysTick->SR = 0;
  ++g_millis;
}

uint32_t millis() {
  return g_millis;
}

void delay(uint32_t ms) {
  Delay_Ms(ms);
}

void delayMicroseconds(uint32_t us) {
  Delay_Us(us);
}

void HardI2C::begin() {
  funGpioInitAll();
  RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
  RCC->APB1PCENR |= RCC_APB1Periph_I2C1;
  RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
  RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

  // Match ch32fun examples: AF open-drain, 10MHz setting.
  funPinMode(kSdaPin, GPIO_CFGLR_OUT_10Mhz_AF_OD);
  funPinMode(kSclPin, GPIO_CFGLR_OUT_10Mhz_AF_OD);

  I2C1->CTLR1 = 0;
  configureTiming();
  I2C1->CTLR1 = I2C_CTLR1_ACK | I2C_CTLR1_PE;
}

void HardI2C::setSpeed(uint32_t hz) {
  if (hz == 0) {
    hz = 100000;
  }
  clock_hz_ = hz;
  configureTiming();
}

void HardI2C::configureTiming() {
  const uint32_t pclk = FUNCONF_SYSTEM_CORE_CLOCK;
  uint32_t freq_field = pclk / 1000000U;
  if (freq_field == 0U) freq_field = 1U;
  if (freq_field > 0x3FU) freq_field = 0x3FU;

  // Match ch32fun behavior:
  // - <=100kHz: standard mode, CCR = PCLK / (2 * fSCL)
  // - >100kHz: fast mode (33% duty), CCR = PCLK / (3 * fSCL), set FS bit
  I2C1->CTLR1 &= ~I2C_CTLR1_PE;
  I2C1->CTLR2 = (uint16_t)(freq_field & I2C_CTLR2_FREQ);
  uint16_t ckcfgr = 0;
  if (clock_hz_ <= 100000) {
    uint32_t ccr = pclk / (clock_hz_ << 1);
    if (ccr < 4U) ccr = 4U;
    ckcfgr = (uint16_t)(ccr & I2C_CKCFGR_CCR);
  } else {
    uint32_t ccr = pclk / (clock_hz_ * 3U);
    if (ccr == 0U) ccr = 1U;
    ckcfgr = (uint16_t)((ccr & I2C_CKCFGR_CCR) | I2C_CKCFGR_FS);
  }
  I2C1->CKCFGR = ckcfgr;
  I2C1->CTLR1 |= I2C_CTLR1_PE;
  I2C1->CTLR1 |= I2C_CTLR1_ACK;
}

bool HardI2C::waitStar1Set(uint16_t mask, uint32_t timeout_ms) {
  uint32_t timeout = timeout_ms ? timeout_ms : kI2cTimeout;
  while ((I2C1->STAR1 & mask) == 0 && --timeout) {
  }
  return timeout != 0;
}

bool HardI2C::waitTxeOrBtf(uint32_t timeout_ms) {
  uint32_t timeout = timeout_ms ? timeout_ms : kI2cTimeout;
  while (((I2C1->STAR1 & I2C_STAR1_TXE) == 0) && ((I2C1->STAR1 & I2C_STAR1_BTF) == 0) && --timeout) {
  }
  return timeout != 0;
}

bool HardI2C::waitRxne(uint32_t timeout_ms) {
  uint32_t timeout = timeout_ms ? timeout_ms : kI2cTimeout;
  while ((I2C1->STAR1 & I2C_STAR1_RXNE) == 0 && --timeout) {
  }
  return timeout != 0;
}

void HardI2C::clearAddrFlag() {
  (void)I2C1->STAR1;
  (void)I2C1->STAR2;
}

void HardI2C::clearErrorFlags() {
  I2C1->STAR1 &= ~(I2C_STAR1_AF | I2C_STAR1_ARLO | I2C_STAR1_BERR | I2C_STAR1_OVR);
}

bool HardI2C::generateStart() {
  clearErrorFlags();
  uint32_t timeout = kI2cTimeout;
  while ((I2C1->STAR2 & I2C_STAR2_BUSY) && --timeout) {
  }
  // Match ch32fun i2c_sensor_test: even if BUSY wait expires, still try START.
  I2C1->CTLR1 |= I2C_CTLR1_START;
  return waitStar1Set(I2C_STAR1_SB, kI2cTimeout);
}

bool HardI2C::sendAddress(uint8_t addressWithRw) {
  I2C1->DATAR = addressWithRw;
  uint32_t timeout = kI2cTimeout;
  while ((I2C1->STAR1 & I2C_STAR1_ADDR) == 0 && --timeout) {
    if (I2C1->STAR1 & I2C_STAR1_AF) {
      // NACK on address phase: clear flag and fail fast.
      I2C1->STAR1 &= ~I2C_STAR1_AF;
      return false;
    }
  }
  return timeout != 0;
}

void HardI2C::generateStop() {
  I2C1->CTLR1 |= I2C_CTLR1_STOP;
}

bool HardI2C::write(uint8_t address, const uint8_t *data, uint8_t length) {
  if (!generateStart()) {
    generateStop();
    return false;
  }
  if (!sendAddress((uint8_t)(address << 1))) {
    generateStop();
    return false;
  }
  clearAddrFlag();

  for (uint8_t i = 0; i < length; ++i) {
    if (!waitStar1Set(I2C_STAR1_TXE, kI2cTimeout)) {
      generateStop();
      return false;
    }
    I2C1->DATAR = data[i];
  }

  if (!waitStar1Set(I2C_STAR1_BTF, kI2cTimeout)) {
    generateStop();
    return false;
  }

  generateStop();
  return true;
}

bool HardI2C::read(uint8_t address, uint8_t *data, uint8_t length) {
  if (length == 0) {
    return true;
  }
  if (!generateStart()) {
    generateStop();
    return false;
  }
  if (!sendAddress((uint8_t)((address << 1) | 1))) {
    generateStop();
    return false;
  }

  // Match ch32fun i2c_sensor_test receive sequence.
  I2C1->CTLR1 |= I2C_CTLR1_ACK;
  clearAddrFlag();

  for (uint8_t i = 0; i < length; ++i) {
    if (i == (length - 1)) {
      I2C1->CTLR1 &= ~I2C_CTLR1_ACK;
    }
    if (!waitRxne(kI2cTimeout)) {
      generateStop();
      I2C1->CTLR1 |= I2C_CTLR1_ACK;
      return false;
    }
    data[i] = (uint8_t)I2C1->DATAR;
  }

  generateStop();
  I2C1->CTLR1 |= I2C_CTLR1_ACK;
  return true;
}

void TwoWire::begin() {
  systickInit();
  bus_.begin();
  bus_.setSpeed(100000);
}

void TwoWire::setClock(uint32_t hz) {
  bus_.setSpeed(hz);
}

void TwoWire::beginTransmission(uint8_t address) {
  tx_address_ = address;
  tx_length_ = 0;
}

size_t TwoWire::write(uint8_t value) {
  if (tx_length_ < kBufferSize) {
    tx_buffer_[tx_length_++] = value;
    return 1;
  }
  return 0;
}

size_t TwoWire::write(const uint8_t *buffer, size_t size) {
  size_t written = 0;
  while (written < size && tx_length_ < kBufferSize) {
    tx_buffer_[tx_length_++] = buffer[written++];
  }
  return written;
}

uint8_t TwoWire::endTransmission() {
  return bus_.write(tx_address_, tx_buffer_, tx_length_) ? 0 : 4;
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t quantity) {
  if (quantity > kBufferSize) {
    quantity = kBufferSize;
  }
  rx_index_ = 0;
  rx_length_ = bus_.read(address, rx_buffer_, quantity) ? quantity : 0;
  return rx_length_;
}

int TwoWire::read() {
  if (rx_index_ >= rx_length_) {
    return -1;
  }
  return rx_buffer_[rx_index_++];
}

TwoWire Wire;
