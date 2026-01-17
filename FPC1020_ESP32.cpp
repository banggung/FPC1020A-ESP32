#include "FPC1020_ESP32.h"

uint16_t g_matchedId = 0;
uint8_t g_matchedPerm = 0;

FPC1020_ESP32::FPC1020_ESP32(HardwareSerial *ser) : _serial(ser) {}

void FPC1020_ESP32::begin(int rx, int tx) {
  _serial->begin(19200, SERIAL_8N1, rx, tx);
  delay(200);
  _flush();
}

void FPC1020_ESP32::_flush() {
  _serial->flush();
  while (_serial->available()) _serial->read();
}

void FPC1020_ESP32::_sendCmd(uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
  _flush();
  delay(50);
  
  uint8_t chk = cmd ^ p1 ^ p2 ^ p3 ^ p4;
  uint8_t pkt[8] = {0xF5, cmd, p1, p2, p3, p4, chk, 0xF5};
  
  Serial.print("TX: ");
  for (int i = 0; i < 8; i++) Serial.printf("%02X ", pkt[i]);
  Serial.println();
  
  _serial->write(pkt, 8);
  _serial->flush();
}

int FPC1020_ESP32::_readResp(uint8_t expectedCmd, unsigned long timeout) {
  unsigned long start = millis();
  int idx = 0;
  memset(_buf, 0, sizeof(_buf));
  
  while (millis() - start < timeout) {
    while (_serial->available() && idx < 8) {
      uint8_t c = _serial->read();
      if (idx == 0) {
        if (c == 0xF5) _buf[idx++] = c;
      } else {
        _buf[idx++] = c;
      }
    }
    
    if (idx == 8) {
      if (_buf[1] == expectedCmd && _buf[7] == 0xF5) break;
      idx = 0;
      memset(_buf, 0, sizeof(_buf));
    }
    delay(10);
    yield();
  }
  
  Serial.print("RX: ");
  for (int i = 0; i < idx; i++) Serial.printf("%02X ", _buf[i]);
  Serial.printf("(len=%d)\n", idx);
  
  return idx;
}

bool FPC1020_ESP32::fingerPresent() {
  _sendCmd(0x2F, 0, 0, 0, 0);
  int len = _readResp(0x2F, 300);
  return (len == 8 && _buf[3] < 0xFA);
}

uint8_t FPC1020_ESP32::Search() {
  // Search waits internally for finger, needs longer timeout
  _sendCmd(0x0C, 0, 0, 0, 0);
  int len = _readResp(0x0C, 10000);  // 10 second timeout
  if (len == 8) {
    uint8_t q3 = _buf[4];
    Serial.printf("Search Q3=0x%02X\n", q3);
    if (q3 >= 1 && q3 <= 3) {
      g_matchedId = ((uint16_t)_buf[2] << 8) | _buf[3];
      g_matchedPerm = q3;
      return ACK_SUCCESS;
    }
    return q3;
  }
  return ACK_TIMEOUT;
}

uint8_t FPC1020_ESP32::Enroll1(uint16_t userId, uint8_t perm) {
  _sendCmd(0x01, userId >> 8, userId & 0xFF, perm, 0);
  int len = _readResp(0x01, 10000);
  if (len == 8) return _buf[4];
  return ACK_TIMEOUT;
}

uint8_t FPC1020_ESP32::Enroll2(uint16_t userId, uint8_t perm) {
  _sendCmd(0x02, userId >> 8, userId & 0xFF, perm, 0);
  int len = _readResp(0x02, 10000);
  if (len == 8) return _buf[4];
  return ACK_TIMEOUT;
}

uint8_t FPC1020_ESP32::Enroll3(uint16_t userId, uint8_t perm) {
  _sendCmd(0x03, userId >> 8, userId & 0xFF, perm, 0);
  int len = _readResp(0x03, 10000);
  if (len == 8) return _buf[4];
  return ACK_TIMEOUT;
}

uint8_t FPC1020_ESP32::Delete(uint16_t userId) {
  _sendCmd(0x04, userId >> 8, userId & 0xFF, 0, 0);
  int len = _readResp(0x04, 2000);
  if (len == 8) return _buf[4];
  return ACK_TIMEOUT;
}

uint8_t FPC1020_ESP32::Clear() {
  _sendCmd(0x05, 0, 0, 0, 0);
  int len = _readResp(0x05, 3000);
  if (len == 8) return _buf[4];
  return ACK_TIMEOUT;
}

uint8_t FPC1020_ESP32::GetUserCount(uint16_t *count) {
  _sendCmd(0x09, 0, 0, 0, 0);
  int len = _readResp(0x09, 2000);
  if (len == 8 && _buf[4] == ACK_SUCCESS) {
    *count = ((uint16_t)_buf[2] << 8) | _buf[3];
    return ACK_SUCCESS;
  }
  return ACK_TIMEOUT;
}

uint8_t FPC1020_ESP32::CheckUser(uint16_t userId) {
  _sendCmd(0x0A, userId >> 8, userId & 0xFF, 0, 0);
  int len = _readResp(0x0A, 500);
  if (len == 8) {
    return _buf[4];
  }
  return ACK_TIMEOUT;
}

uint8_t FPC1020_ESP32::GetAllUsers(uint16_t *ids, uint8_t *perms, uint16_t *count, uint8_t maxUsers) {
  // Send 0x2B command
  _sendCmd(0x2B, 0, 0, 0, 0);
  
  // Read header (8 bytes)
  int len = _readResp(0x2B, 2000);
  if (len != 8 || _buf[4] != ACK_SUCCESS) {
    Serial.printf("GetAllUsers header failed: len=%d, Q3=0x%02X\n", len, _buf[4]);
    *count = 0;
    return (_buf[4] != 0) ? _buf[4] : ACK_FAIL;
  }
  
  // Get data length from header
  uint16_t dataLen = ((uint16_t)_buf[2] << 8) | _buf[3];
  Serial.printf("GetAllUsers dataLen=%d\n", dataLen);
  
  if (dataLen < 2) {
    *count = 0;
    return ACK_SUCCESS;
  }
  
  // Read data part: F5 + data + CHK + F5
  delay(50);
  unsigned long start = millis();
  uint8_t dataBuf[200];
  int idx = 0;
  
  while (millis() - start < 3000 && idx < dataLen + 3) {
    if (_serial->available()) {
      uint8_t c = _serial->read();
      if (idx == 0 && c != 0xF5) continue;
      dataBuf[idx++] = c;
    }
    yield();
  }
  
  Serial.print("Data RX: ");
  for (int i = 0; i < idx && i < 30; i++) Serial.printf("%02X ", dataBuf[i]);
  if (idx > 30) Serial.print("...");
  Serial.printf(" (len=%d)\n", idx);
  
  if (idx < dataLen + 3) {
    Serial.println("GetAllUsers data incomplete");
    *count = 0;
    return ACK_TIMEOUT;
  }
  
  // Parse: dataBuf[0]=0xF5, dataBuf[1..2]=count, dataBuf[3..]=user data
  uint16_t userCount = ((uint16_t)dataBuf[1] << 8) | dataBuf[2];
  *count = userCount;
  Serial.printf("User count: %d\n", userCount);
  
  // Extract IDs and permissions
  int numExtracted = 0;
  for (int i = 0; i < userCount && numExtracted < maxUsers; i++) {
    int offset = 3 + i * 3;  // 3 bytes per user
    if (offset + 2 < idx) {
      ids[numExtracted] = ((uint16_t)dataBuf[offset] << 8) | dataBuf[offset + 1];
      perms[numExtracted] = dataBuf[offset + 2];
      numExtracted++;
    }
  }
  
  return ACK_SUCCESS;
}
