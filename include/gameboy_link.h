#pragma once

#include <Arduino.h>

class GameBoyLink {
 public:
  struct Stats {
    uint32_t transfers;
    uint32_t queueOverflows;
    uint32_t resyncs;
    uint32_t edgeFaults;
  };

  GameBoyLink(int clockPin, int fromGameBoyPin, int toGameBoyPin);

  void begin(uint8_t idleByte, uint8_t captureCommand, uint8_t busyByte);
  bool popReceived(uint8_t& value);
  void setStatus(uint8_t statusByte);
  void publishCaption(const uint8_t* bytes, size_t length, uint8_t doneStatus);
  void cancelStream(uint8_t statusByte);
  bool isStreaming() const;
  Stats stats() const;

 private:
  static void IRAM_ATTR clockIsrThunk();
  void IRAM_ATTR onClockChange();

  static GameBoyLink* instance_;
  static constexpr size_t kRxQueueSize = 16;
  static constexpr size_t kMaxFrameSize = 66;

  const int clockPin_;
  const int fromGameBoyPin_;
  const int toGameBoyPin_;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

  volatile uint8_t rxQueue_[kRxQueueSize]{};
  volatile uint8_t rxHead_ = 0;
  volatile uint8_t rxTail_ = 0;
  volatile uint8_t rxShift_ = 0;
  volatile uint8_t bitIndex_ = 0;
  volatile bool transferActive_ = false;
  volatile bool waitingForIdleGap_ = false;
  volatile int8_t lastClockLevel_ = -1;
  volatile int64_t lastClockEdgeUs_ = 0;
  volatile uint8_t activeTx_ = 0xFF;
  volatile bool activeTxWasStream_ = false;

  volatile uint8_t statusByte_ = 0xFF;
  volatile uint8_t nextTx_ = 0xFF;
  volatile bool nextTxIsStream_ = false;
  volatile bool streaming_ = false;
  volatile uint8_t streamFrame_[kMaxFrameSize]{};
  volatile uint8_t streamFrameLength_ = 0;
  volatile uint8_t streamPosition_ = 0;
  volatile uint8_t doneStatus_ = 0xFF;
  volatile uint8_t captureCommand_ = 0x43;
  volatile uint8_t busyByte_ = 0x17;

  volatile uint32_t transfers_ = 0;
  volatile uint32_t queueOverflows_ = 0;
  volatile uint32_t resyncs_ = 0;
  volatile uint32_t edgeFaults_ = 0;
};
