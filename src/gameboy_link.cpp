#include "gameboy_link.h"

#include <driver/gpio.h>
#include <esp_timer.h>

namespace {

// A DMG byte takes about 977 us at 8192 Hz. A gap above 3 ms therefore
// cannot occur inside a healthy byte and is safe to use as a new boundary.
constexpr int64_t kClockIdleResyncUs = 3000;

}  // namespace

GameBoyLink* GameBoyLink::instance_ = nullptr;

GameBoyLink::GameBoyLink(int clockPin, int fromGameBoyPin, int toGameBoyPin)
    : clockPin_(clockPin),
      fromGameBoyPin_(fromGameBoyPin),
      toGameBoyPin_(toGameBoyPin) {}

void GameBoyLink::begin(uint8_t idleByte, uint8_t captureCommand,
                        uint8_t busyByte) {
  instance_ = this;
  statusByte_ = idleByte;
  nextTx_ = idleByte;
  doneStatus_ = idleByte;
  captureCommand_ = captureCommand;
  busyByte_ = busyByte;

  pinMode(fromGameBoyPin_, INPUT_PULLUP);
  pinMode(clockPin_, INPUT_PULLUP);
  pinMode(toGameBoyPin_, OUTPUT);
  digitalWrite(toGameBoyPin_, HIGH);
  attachInterrupt(digitalPinToInterrupt(clockPin_), clockIsrThunk, CHANGE);
}

bool GameBoyLink::popReceived(uint8_t& value) {
  bool available = false;
  portENTER_CRITICAL(&mux_);
  if (rxTail_ != rxHead_) {
    value = rxQueue_[rxTail_];
    rxTail_ = static_cast<uint8_t>((rxTail_ + 1) % kRxQueueSize);
    available = true;
  }
  portEXIT_CRITICAL(&mux_);
  return available;
}

void GameBoyLink::setStatus(uint8_t statusByte) {
  portENTER_CRITICAL(&mux_);
  statusByte_ = statusByte;
  doneStatus_ = statusByte;
  if (!nextTxIsStream_) {
    nextTx_ = statusByte;
  }
  portEXIT_CRITICAL(&mux_);
}

void GameBoyLink::cancelStream(uint8_t statusByte) {
  portENTER_CRITICAL(&mux_);
  streaming_ = false;
  nextTxIsStream_ = false;
  streamFrameLength_ = 0;
  streamPosition_ = 0;
  statusByte_ = statusByte;
  doneStatus_ = statusByte;
  nextTx_ = statusByte;
  portEXIT_CRITICAL(&mux_);
}

void GameBoyLink::publishCaption(const uint8_t* bytes, size_t length,
                                 uint8_t doneStatus) {
  if (bytes == nullptr || length == 0 || length > 63 ||
      length + 2 > kMaxFrameSize) {
    cancelStream(doneStatus);
    return;
  }

  uint8_t checksum = 0;
  portENTER_CRITICAL(&mux_);
  streamFrame_[0] = static_cast<uint8_t>(0x80 | length);
  for (size_t i = 0; i < length; ++i) {
    streamFrame_[i + 1] = bytes[i];
    checksum ^= bytes[i];
  }
  streamFrame_[length + 1] = checksum;
  streamFrameLength_ = static_cast<uint8_t>(length + 2);
  streamPosition_ = 0;
  doneStatus_ = doneStatus;
  statusByte_ = doneStatus;
  streaming_ = true;
  nextTx_ = streamFrame_[0];
  nextTxIsStream_ = true;
  portEXIT_CRITICAL(&mux_);
}

bool GameBoyLink::isStreaming() const {
  bool result;
  portENTER_CRITICAL(&mux_);
  result = streaming_;
  portEXIT_CRITICAL(&mux_);
  return result;
}

GameBoyLink::Stats GameBoyLink::stats() const {
  Stats result{};
  portENTER_CRITICAL(&mux_);
  result.transfers = transfers_;
  result.queueOverflows = queueOverflows_;
  result.resyncs = resyncs_;
  result.edgeFaults = edgeFaults_;
  portEXIT_CRITICAL(&mux_);
  return result;
}

void IRAM_ATTR GameBoyLink::clockIsrThunk() {
  if (instance_ != nullptr) {
    instance_->onClockChange();
  }
}

void IRAM_ATTR GameBoyLink::onClockChange() {
  const bool clockHigh = gpio_get_level(static_cast<gpio_num_t>(clockPin_));
  const int64_t nowUs = esp_timer_get_time();

  portENTER_CRITICAL_ISR(&mux_);
  const bool idleGap =
      lastClockEdgeUs_ != 0 && nowUs - lastClockEdgeUs_ > kClockIdleResyncUs;
  if (idleGap) {
    if (transferActive_ || waitingForIdleGap_) ++resyncs_;
    transferActive_ = false;
    waitingForIdleGap_ = false;
    bitIndex_ = 0;
    rxShift_ = 0;
    activeTxWasStream_ = false;
  } else if (lastClockLevel_ == static_cast<int8_t>(clockHigh)) {
    // CHANGE interrupts must alternate levels. Seeing the same level twice
    // means at least one edge was missed, so byte alignment is no longer
    // trustworthy. Ignore the rest of that burst and restart after idle.
    ++edgeFaults_;
    waitingForIdleGap_ = true;
    transferActive_ = false;
    bitIndex_ = 0;
    rxShift_ = 0;
    activeTxWasStream_ = false;
  }
  lastClockEdgeUs_ = nowUs;
  lastClockLevel_ = static_cast<int8_t>(clockHigh);

  if (waitingForIdleGap_) {
    portEXIT_CRITICAL_ISR(&mux_);
    return;
  }

  if (!clockHigh) {
    if (!transferActive_) {
      transferActive_ = true;
      activeTx_ = nextTx_;
      activeTxWasStream_ = nextTxIsStream_;
    }
    const uint8_t mask = static_cast<uint8_t>(0x80u >> bitIndex_);
    gpio_set_level(static_cast<gpio_num_t>(toGameBoyPin_),
                   (activeTx_ & mask) != 0);
    portEXIT_CRITICAL_ISR(&mux_);
    return;
  }

  if (!transferActive_) {
    portEXIT_CRITICAL_ISR(&mux_);
    return;
  }

  rxShift_ = static_cast<uint8_t>(
      (rxShift_ << 1) |
      (gpio_get_level(static_cast<gpio_num_t>(fromGameBoyPin_)) ? 1 : 0));
  ++bitIndex_;
  if (bitIndex_ < 8) {
    portEXIT_CRITICAL_ISR(&mux_);
    return;
  }

  const uint8_t receivedByte = rxShift_;
  const uint8_t nextHead =
      static_cast<uint8_t>((rxHead_ + 1) % kRxQueueSize);
  if (nextHead == rxTail_) {
    ++queueOverflows_;
  } else {
    rxQueue_[rxHead_] = rxShift_;
    rxHead_ = nextHead;
  }
  ++transfers_;
  rxShift_ = 0;
  bitIndex_ = 0;
  transferActive_ = false;

  if (activeTxWasStream_ && streaming_) {
    ++streamPosition_;
    if (streamPosition_ < streamFrameLength_) {
      nextTx_ = streamFrame_[streamPosition_];
      nextTxIsStream_ = true;
    } else {
      streaming_ = false;
      nextTxIsStream_ = false;
      statusByte_ = doneStatus_;
      nextTx_ = doneStatus_;
    }
  } else if (!nextTxIsStream_) {
    nextTx_ = statusByte_;
  }

  // Reset the response at the exact byte boundary where CAPTURE arrives.
  // Waiting for loop() left a small window in which a consecutive request
  // could receive the previous frame header and mistake it for a new result.
  if (receivedByte == captureCommand_) {
    streaming_ = false;
    nextTxIsStream_ = false;
    streamFrameLength_ = 0;
    streamPosition_ = 0;
    statusByte_ = busyByte_;
    doneStatus_ = busyByte_;
    nextTx_ = busyByte_;
  }

  portEXIT_CRITICAL_ISR(&mux_);
}
