#ifndef FPC1020_ESP32_H
#define FPC1020_ESP32_H

#include <Arduino.h>

#define ACK_SUCCESS 0x00
#define ACK_FAIL 0x01
#define ACK_FULL 0x04
#define ACK_NOUSER 0x05
#define ACK_USER_OCCUPIED 0x06
#define ACK_USER_EXIST 0x07
#define ACK_TIMEOUT 0x08

extern uint16_t g_matchedId;
extern uint8_t g_matchedPerm;

class FPC1020_ESP32 {
public:
  FPC1020_ESP32(HardwareSerial *ser);
  void begin(int rx, int tx);
  bool fingerPresent();
  uint8_t Search();
  uint8_t Enroll1(uint16_t userId, uint8_t perm = 1);
  uint8_t Enroll2(uint16_t userId, uint8_t perm = 1);
  uint8_t Enroll3(uint16_t userId, uint8_t perm = 1);
  uint8_t Delete(uint16_t userId);
  uint8_t Clear();
  uint8_t GetUserCount(uint16_t *count);
  uint8_t CheckUser(uint16_t userId);
  uint8_t GetAllUsers(uint16_t *ids, uint8_t *perms, uint16_t *count, uint8_t maxUsers);

private:
  HardwareSerial *_serial;
  uint8_t _buf[12];
  void _sendCmd(uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4);
  int _readResp(uint8_t expectedCmd, unsigned long timeout);
  void _flush();
};

#endif
