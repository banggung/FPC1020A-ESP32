# FPC1020A Fingerprint Door Lock

ESP32-based fingerprint door lock system using FPC1020A sensor module with OLED display and rotary encoder interface.

## Features

- 5-menu system: SCAN, ENROLL, DELETE, LIST, CLEAR
- 128x64 OLED display (SSD1306, SPI)
- Rotary encoder for navigation
- LED indicator and buzzer feedback
- Serial debug output (115200 baud)
- Supports up to 100 fingerprint IDs

---

## Hardware Components

```
+----------------------+----------------------------------+
| Component            | Model/Spec                       |
+----------------------+----------------------------------+
| MCU                  | ESP32 DevKit                     |
| Fingerprint Sensor   | FPC1020A Module (Biovo Protocol) |
| Display              | 128x64 OLED SSD1306 (SPI)        |
| Input                | Rotary Encoder with Push Button  |
| Output               | LED, Buzzer Module (3-pin)       |
+----------------------+----------------------------------+
```

---

## Pinout Connections

```
+------------------+----------+------------------+
| Component        | Pin      | ESP32 GPIO       |
+------------------+----------+------------------+
| FPC1020A         | GND      | GND              |
| (6-pin module)   | UART_RX  | GPIO17 (TX2)     |
|                  | UART_TX  | GPIO16 (RX2)     |
|                  | VCC      | 3.3V             |
|                  | TOUCH    | NC (not used)    |
|                  | V-TOUCH  | 3.3V             |
+------------------+----------+------------------+
| OLED SSD1306     | CLK      | GPIO18           |
| (SPI)            | MOSI     | GPIO23           |
|                  | DC       | GPIO19           |
|                  | CS       | GPIO15           |
|                  | RESET    | GPIO5            |
|                  | VCC      | 3.3V             |
|                  | GND      | GND              |
+------------------+----------+------------------+
| Rotary Encoder   | CLK      | GPIO25           |
|                  | DT       | GPIO26           |
|                  | SW       | GPIO27           |
|                  | VCC      | 3.3V             |
|                  | GND      | GND              |
+------------------+----------+------------------+
| LED              | Anode    | GPIO4            |
|                  | Cathode  | GND (via 220Ω)   |
+------------------+----------+------------------+
| Buzzer Module    | VCC      | 3.3V             |
| (3-pin, active)  | GND      | GND              |
|                  | SIG      | GPIO2            |
+------------------+----------+------------------+
```

---

## System Architecture

```
+-------------+      UART 19200      +-------------+
|             | <------------------> |             |
|    ESP32    |      8-byte packets  |  FPC1020A   |
|             |                      |  Sensor     |
+------+------+                      +-------------+
       |
       | SPI
       v
+-------------+
|   SSD1306   |
|    OLED     |
+-------------+
       
+-------------+      +-------------+      +-------------+
|   Rotary    |      |     LED     |      |   Buzzer    |
|   Encoder   |      |             |      |             |
+-------------+      +-------------+      +-------------+
    GPIO25/26/27        GPIO4               GPIO2
```

---

## FPC1020A Communication Protocol

### Serial Configuration

- Baud Rate: 19200
- Data Bits: 8
- Stop Bits: 1
- Parity: None

### Packet Structure (8 bytes)

```
+------+------+------+------+------+------+------+------+------+
| Byte |  0   |  1   |  2   |  3   |  4   |  5   |  6   |  7   |
+------+------+------+------+------+------+------+------+------+
| Name |START | CMD  | P1   | P2   | P3   | P4   | CHK  | END  |
+------+------+------+------+------+------+------+------+------+
| Value| 0xF5 | xx   | xx   | xx   | xx   | xx   | xx   | 0xF5 |
+------+------+------+------+------+------+------+------+------+

CHK = CMD ^ P1 ^ P2 ^ P3 ^ P4 (XOR of bytes 1-5)
```

### Command List

```
+------+------------------+------------------------------------------+
| CMD  | Name             | Description                              |
+------+------------------+------------------------------------------+
| 0x01 | ENROLL_STEP1     | Enroll fingerprint - 1st scan            |
| 0x02 | ENROLL_STEP2     | Enroll fingerprint - 2nd/3rd scan        |
| 0x03 | ENROLL_STEP3     | Enroll fingerprint - final scan          |
| 0x04 | DELETE           | Delete user by ID                        |
| 0x05 | CLEAR            | Delete all users                         |
| 0x09 | USER_COUNT       | Get number of enrolled users             |
| 0x0A | GET_PERMISSION   | Get user permission (check if exists)    |
| 0x0B | VERIFY           | 1:1 matching (verify specific ID)        |
| 0x0C | SEARCH           | 1:N matching (find any match)            |
| 0x2B | GET_ALL_USERS    | Get all user IDs (extended format)       |
| 0x2F | DETECT           | Detect finger presence                   |
+------+------------------+------------------------------------------+
```

### Response Codes (Q3 / Byte 4)

```
+------+------------------+------------------------------------------+
| Code | Name             | Description                              |
+------+------------------+------------------------------------------+
| 0x00 | ACK_SUCCESS      | Operation successful                     |
| 0x01 | ACK_FAIL         | Operation failed                         |
| 0x04 | ACK_FULL         | Database full                            |
| 0x05 | ACK_NOUSER       | User does not exist                      |
| 0x06 | ACK_USER_OCCUPIED| User ID already exists                   |
| 0x07 | ACK_USER_EXIST   | Fingerprint already enrolled             |
| 0x08 | ACK_TIMEOUT      | Acquisition timeout                      |
+------+------------------+------------------------------------------+
```

---

## Protocol Details

### Finger Detection (0x2F)

Checks if a finger is present on the sensor surface.

**Request:**
```
TX: F5 2F 00 00 00 00 2F F5
       |  |  |  |  |  |
       |  +--+--+--+  +-- CHK = 0x2F
       |     |
       |     +-- P1-P4: unused (all 0x00)
       +-- CMD: 0x2F (DETECT)
```

**Response:**
```
RX: F5 2F 00 XX 00 00 CHK F5
             |
             +-- Touch value:
                 0xFF = No finger present
                 < 0xFA = Finger detected (lower = stronger contact)
```

**Examples:**
```
No finger:
TX: F5 2F 00 00 00 00 2F F5
RX: F5 2F 00 FF 00 00 D0 F5
             ^^ 0xFF >= 0xFA → No finger

Finger present:
TX: F5 2F 00 00 00 00 2F F5
RX: F5 2F 00 72 00 00 5D F5
             ^^ 0x72 < 0xFA → Finger detected
```

---

### Enrollment Step 1 (0x01)

First step of fingerprint enrollment. Captures initial fingerprint image.

**Request:**
```
TX: F5 01 P1 P2 P3 00 CHK F5
       |  |  |  |
       |  |  |  +-- P3: Permission level (1, 2, or 3)
       |  +--+-- P1,P2: User ID (16-bit, big-endian)
       |         P1 = ID >> 8 (high byte)
       |         P2 = ID & 0xFF (low byte)
       +-- CMD: 0x01 (ENROLL_STEP1)
```

**Response:**
```
RX: F5 01 00 00 Q3 00 CHK F5
                |
                +-- Q3: Result code
                    0x00 = ACK_SUCCESS
                    0x01 = ACK_FAIL
                    0x04 = ACK_FULL (database full)
                    0x06 = ACK_USER_OCCUPIED (ID exists)
                    0x07 = ACK_USER_EXIST (finger enrolled)
                    0x08 = ACK_TIMEOUT
```

**Example (Enroll ID=5, Permission=1):**
```
TX: F5 01 00 05 01 00 05 F5
          |  |  |     |
          +--+  |     +-- CHK = 0x01^0x00^0x05^0x01^0x00 = 0x05
          |     +-- Permission = 1
          +-- ID = 0x0005 = 5

Success:
RX: F5 01 00 00 00 00 01 F5
                ^^ Q3 = 0x00 (Success)

ID already used:
RX: F5 01 00 00 06 00 07 F5
                ^^ Q3 = 0x06 (ACK_USER_OCCUPIED)
```

---

### Enrollment Step 2 (0x02)

Middle steps of enrollment (can be called 0-4 times). Captures additional fingerprint images.

**Request:**
```
TX: F5 02 P1 P2 P3 00 CHK F5
       |  |  |  |
       |  |  |  +-- P3: Permission level (must match Step 1)
       |  +--+-- P1,P2: User ID (must match Step 1)
       +-- CMD: 0x02 (ENROLL_STEP2)
```

**Response:**
```
RX: F5 02 00 00 Q3 00 CHK F5
                |
                +-- Q3: Result code
                    0x00 = ACK_SUCCESS
                    0x01 = ACK_FAIL
                    0x08 = ACK_TIMEOUT
```

**Example (Continue enrolling ID=5):**
```
TX: F5 02 00 05 01 00 06 F5
RX: F5 02 00 00 00 00 02 F5   (Success)
```

---

### Enrollment Step 3 (0x03)

Final step of enrollment. Merges all captured images and saves to database.

**Request:**
```
TX: F5 03 P1 P2 P3 00 CHK F5
       |  |  |  |
       |  |  |  +-- P3: Permission level (must match previous steps)
       |  +--+-- P1,P2: User ID (must match previous steps)
       +-- CMD: 0x03 (ENROLL_STEP3)
```

**Response:**
```
RX: F5 03 00 00 Q3 00 CHK F5
                |
                +-- Q3: Result code
                    0x00 = ACK_SUCCESS (enrollment complete)
                    0x01 = ACK_FAIL (merge failed)
                    0x08 = ACK_TIMEOUT
```

**Example (Finalize ID=5):**
```
TX: F5 03 00 05 01 00 07 F5
RX: F5 03 00 00 00 00 03 F5   (Success - fingerprint saved)
```

**Complete 4-Step Enrollment Flow:**
```
Step 1: TX: F5 01 00 05 01 00 05 F5   →   RX: F5 01 00 00 00 00 01 F5
Step 2: TX: F5 02 00 05 01 00 06 F5   →   RX: F5 02 00 00 00 00 02 F5
Step 3: TX: F5 02 00 05 01 00 06 F5   →   RX: F5 02 00 00 00 00 02 F5
Step 4: TX: F5 03 00 05 01 00 07 F5   →   RX: F5 03 00 00 00 00 03 F5
```

---

### Delete User (0x04)

Deletes a single user from the database by ID.

**Request:**
```
TX: F5 04 P1 P2 00 00 CHK F5
       |  |  |
       |  +--+-- P1,P2: User ID to delete (16-bit, big-endian)
       +-- CMD: 0x04 (DELETE)
```

**Response:**
```
RX: F5 04 00 00 Q3 00 CHK F5
                |
                +-- Q3: Result code
                    0x00 = ACK_SUCCESS
                    0x05 = ACK_NOUSER (ID not found)
```

**Example (Delete ID=5):**
```
TX: F5 04 00 05 00 00 01 F5
          |  |        |
          +--+        +-- CHK = 0x04^0x00^0x05^0x00^0x00 = 0x01
          +-- ID = 5

Success:
RX: F5 04 00 00 00 00 04 F5
                ^^ Q3 = 0x00 (Deleted)

Not found:
RX: F5 04 00 00 05 00 01 F5
                ^^ Q3 = 0x05 (ACK_NOUSER)
```

---

### Clear All Users (0x05)

Deletes all fingerprints from the database.

**Request:**
```
TX: F5 05 00 00 00 00 05 F5
       |  |  |  |  |  |
       |  +--+--+--+  +-- CHK = 0x05
       |     |
       |     +-- P1-P4: unused (all 0x00)
       +-- CMD: 0x05 (CLEAR)
```

**Response:**
```
RX: F5 05 00 00 Q3 00 CHK F5
                |
                +-- Q3: Result code
                    0x00 = ACK_SUCCESS (all deleted)
                    0x01 = ACK_FAIL
```

**Example:**
```
TX: F5 05 00 00 00 00 05 F5
RX: F5 05 00 00 00 00 05 F5   (Success - database cleared)
```

---

### Get User Count (0x09)

Returns the total number of enrolled fingerprints.

**Request:**
```
TX: F5 09 00 00 00 00 09 F5
       |  |  |  |  |  |
       |  +--+--+--+  +-- CHK = 0x09
       |     |
       |     +-- P1-P4: unused (all 0x00)
       +-- CMD: 0x09 (USER_COUNT)
```

**Response:**
```
RX: F5 09 Q1 Q2 Q3 00 CHK F5
          |  |  |
          +--+  +-- Q3: Result code (0x00 = success)
          |
          +-- Q1,Q2: User count (16-bit, big-endian)
              Count = (Q1 << 8) | Q2
```

**Example (3 users enrolled):**
```
TX: F5 09 00 00 00 00 09 F5
RX: F5 09 00 03 00 00 0A F5
          |  |  |
          +--+  +-- Q3 = 0x00 (Success)
          |
          +-- Count = 0x0003 = 3 users
```

---

### Check User / Get Permission (0x0A)

Checks if a user ID exists and returns their permission level.

**Request:**
```
TX: F5 0A P1 P2 00 00 CHK F5
       |  |  |
       |  +--+-- P1,P2: User ID to check (16-bit, big-endian)
       +-- CMD: 0x0A (GET_PERMISSION)
```

**Response:**
```
RX: F5 0A 00 00 Q3 00 CHK F5
                |
                +-- Q3: Result
                    1, 2, 3 = Permission level (user exists)
                    0x05 = ACK_NOUSER (user not found)
```

**Example (Check ID=1):**
```
TX: F5 0A 00 01 00 00 0B F5

User exists (permission 1):
RX: F5 0A 00 00 01 00 0B F5
                ^^ Q3 = 0x01 (Permission level 1)

User not found:
RX: F5 0A 00 00 05 00 0F F5
                ^^ Q3 = 0x05 (ACK_NOUSER)
```

---

### Verify / 1:1 Match (0x0B)

Verifies if the finger on sensor matches a specific user ID.

**Request:**
```
TX: F5 0B P1 P2 00 00 CHK F5
       |  |  |
       |  +--+-- P1,P2: User ID to verify against (16-bit, big-endian)
       +-- CMD: 0x0B (VERIFY)
```

**Response:**
```
RX: F5 0B 00 00 Q3 00 CHK F5
                |
                +-- Q3: Result code
                    0x00 = ACK_SUCCESS (finger matches ID)
                    0x01 = ACK_FAIL (no match)
                    0x05 = ACK_NOUSER (ID not enrolled)
                    0x08 = ACK_TIMEOUT
```

**Example (Verify against ID=3):**
```
TX: F5 0B 00 03 00 00 08 F5

Match:
RX: F5 0B 00 00 00 00 0B F5
                ^^ Q3 = 0x00 (Verified)

No match:
RX: F5 0B 00 00 01 00 0A F5
                ^^ Q3 = 0x01 (Failed)
```

---

### Search / 1:N Match (0x0C)

Searches the entire database for a matching fingerprint.

**Request:**
```
TX: F5 0C 00 00 00 00 0C F5
       |  |  |  |  |  |
       |  +--+--+--+  +-- CHK = 0x0C
       |     |
       |     +-- P1-P4: unused (all 0x00)
       +-- CMD: 0x0C (SEARCH)
```

**Response:**
```
RX: F5 0C Q1 Q2 Q3 00 CHK F5
          |  |  |
          +--+  +-- Q3: Permission level (1,2,3) if match found
          |         or error code (0x05=no match, 0x08=timeout)
          |
          +-- Q1,Q2: Matched User ID (16-bit, big-endian)
              Only valid when Q3 = 1, 2, or 3
```

**Examples:**
```
TX: F5 0C 00 00 00 00 0C F5

Match found (ID=3, Permission=1):
RX: F5 0C 00 03 01 00 0E F5
          |  |  |
          +--+  +-- Q3 = 0x01 (Permission 1 = match found)
          |
          +-- ID = 0x0003 = 3

No match:
RX: F5 0C 00 00 05 00 09 F5
                ^^ Q3 = 0x05 (ACK_NOUSER - no match)

Timeout:
RX: F5 0C 00 00 08 00 04 F5
                ^^ Q3 = 0x08 (ACK_TIMEOUT)
```

---

### Get All Users (0x2B) - Extended Format

Returns all enrolled user IDs and permissions in a single response.
This command uses an extended packet format with header + data parts.

**Request:**
```
TX: F5 2B 00 00 00 00 2B F5
       |  |  |  |  |  |
       |  +--+--+--+  +-- CHK = 0x2B
       |     |
       |     +-- P1-P4: unused (all 0x00)
       +-- CMD: 0x2B (GET_ALL_USERS)
```

**Response Header (8 bytes):**
```
RX: F5 2B LEN_H LEN_L Q3 00 CHK F5
          |     |     |
          +-----+     +-- Q3: 0x00 = ACK_SUCCESS
             |
             +-- Data length = 3 × user_count + 2
```

**Response Data Part (LEN + 3 bytes):**
```
+----+--------+--------+------+------+-------+------+------+-------+-----+-----+----+
| F5 |COUNT_H |COUNT_L | ID1_H| ID1_L| PERM1 | ID2_H| ID2_L| PERM2 | ... | CHK | F5 |
+----+--------+--------+------+------+-------+------+------+-------+-----+-----+----+
     |        |        |      |      |
     +--------+        +------+------+
     Total count       User 1: ID (16-bit) + Permission (8-bit)
```

**Data Structure:**
```
+--------+------------------+----------------------------------------+
| Offset | Field            | Description                            |
+--------+------------------+----------------------------------------+
| 0      | 0xF5             | Start byte                             |
| 1-2    | COUNT_H, COUNT_L | Total user count (16-bit, big-endian)  |
| 3-5    | User 1           | ID_H, ID_L, Permission                 |
| 6-8    | User 2           | ID_H, ID_L, Permission                 |
| ...    | ...              | 3 bytes per user                       |
| N-1    | CHK              | XOR checksum of data bytes             |
| N      | 0xF5             | End byte                               |
+--------+------------------+----------------------------------------+
```

**Example with 3 users (IDs: 1, 3, 6):**
```
Request:
TX: F5 2B 00 00 00 00 2B F5

Response Header:
RX: F5 2B 00 0B 00 00 2B F5
          |  |  |
          +--+  +-- Q3 = 0x00 (Success)
          |
          +-- LEN = 0x000B = 11 bytes = 3×3+2

Response Data:
RX: F5 00 03 00 01 01 00 03 01 00 06 01 xx F5
       |  |  |     |     |     |     |
       +--+  +-----+     +-----+     +-----+
       |     |           |           |
       |     User 1      User 2      User 3
       |     ID=1        ID=3        ID=6
       |     Perm=1      Perm=1      Perm=1
       |
       Count = 3

Byte-by-byte breakdown:
+------+-------+----------------------------------+
| Byte | Value | Description                      |
+------+-------+----------------------------------+
| 0    | 0xF5  | Start byte                       |
| 1    | 0x00  | Count high byte                  |
| 2    | 0x03  | Count low byte (3 users)         |
| 3    | 0x00  | User 1 ID high byte              |
| 4    | 0x01  | User 1 ID low byte (ID=1)        |
| 5    | 0x01  | User 1 Permission (1)            |
| 6    | 0x00  | User 2 ID high byte              |
| 7    | 0x03  | User 2 ID low byte (ID=3)        |
| 8    | 0x01  | User 2 Permission (1)            |
| 9    | 0x00  | User 3 ID high byte              |
| 10   | 0x06  | User 3 ID low byte (ID=6)        |
| 11   | 0x01  | User 3 Permission (1)            |
| 12   | 0xXX  | Checksum                         |
| 13   | 0xF5  | End byte                         |
+------+-------+----------------------------------+
```

**Empty Database Response:**
```
Header: F5 2B 00 02 00 00 29 F5   (LEN=2, minimum)
Data:   F5 00 00 00 F5            (Count=0)
```

---

## Software Architecture

### File Structure

```
FPC1020_ESP32/
├── FPC1020_ESP32.ino      # Main application
├── FPC1020_ESP32.h        # Library header
└── FPC1020_ESP32.cpp      # Library implementation
```

### Class: FPC1020_ESP32

```cpp
class FPC1020_ESP32 {
public:
  void begin(int rx, int tx);           // Initialize UART
  bool fingerPresent();                  // Check finger on sensor
  uint8_t Search();                      // 1:N fingerprint match
  uint8_t Enroll1(uint16_t id, uint8_t perm);  // Enroll step 1
  uint8_t Enroll2(uint16_t id, uint8_t perm);  // Enroll step 2-3
  uint8_t Enroll3(uint16_t id, uint8_t perm);  // Enroll final
  uint8_t Delete(uint16_t id);           // Delete single user
  uint8_t Clear();                       // Delete all users
  uint8_t GetUserCount(uint16_t *count); // Get enrolled count
  uint8_t CheckUser(uint16_t id);        // Check if user exists
  uint8_t GetAllUsers(uint16_t *ids, uint8_t *perms, 
                      uint16_t *count, uint8_t maxUsers);  // Get all user IDs
};

// Global variables set by Search()
extern uint16_t g_matchedId;    // Matched user ID
extern uint8_t g_matchedPerm;   // Matched user permission
```

### Menu System

```
+------------------+------------------------------------------+
| Menu             | Function                                 |
+------------------+------------------------------------------+
| SCAN             | Search fingerprint (1:N match)           |
| ENROLL           | Register new fingerprint (4 steps)       |
| DELETE           | Remove specific user ID                  |
| LIST             | Show total count and registered IDs      |
| CLEAR            | Delete all fingerprints (with confirm)   |
+------------------+------------------------------------------+
```

### Operation Flow

**SCAN Flow:**
```
                      +--------+
                      |  SCAN  |
                      +---+----+
                          |
                    Place finger
                          |
                   Send 0x0C Search
                          |
            +-------------+-------------+
            |             |             |
       [Q3=1,2,3]    [Q3=0x05]     [Q3=0x08]
       Match found    No match      Timeout
            |             |             |
       "Access OK"    "Denied"     "Timeout"
         ID: X        No match
```

**ENROLL Flow:**
```
                      +--------+
                      | ENROLL |
                      +---+----+
                          |
                     Select ID
                          |
                          v
                   +------+------+
                   |   Step 1    |
                   |   (0x01)    |
                   +------+------+
                          |
              +-----------+-----------+
              |                       |
         [Q3=0x00]              [Q3=error]
          Success                 Fail
              |                       |
              v                  "ID in use"
       +------+------+           "Finger exists"
       |   Step 2    |           "Timeout"
       |   (0x02)    |
       +------+------+
              |
              +---> (repeat Step 2 once more)
              |
              v
       +------+------+
       |   Step 4    |
       |   (0x03)    |
       +------+------+
              |
    +---------+---------+
    |                   |
[Q3=0x00]          [Q3=0x01]
 Success              Fail
    |                   |
"Enrolled!"        "Step Fail"
  ID: X            (retry needed)
```

**DELETE Flow:**
```
                      +--------+
                      | DELETE |
                      +---+----+
                          |
                     Select ID
                          |
                   Send 0x04 Delete
                          |
              +-----------+-----------+
              |                       |
         [Q3=0x00]              [Q3=0x05]
          Success               Not found
              |                       |
         "Deleted"              "Not found"
           ID: X
```

---

## Timing Requirements

```
+----------------------+-------------+
| Operation            | Timeout     |
+----------------------+-------------+
| Finger detect        | 300 ms      |
| Search (1:N)         | 10000 ms    |
| Enroll step          | 10000 ms    |
| Delete               | 2000 ms     |
| Clear all            | 3000 ms     |
| Check user           | 500 ms      |
| Get all users        | 2000 ms     |
| Get all users data   | 3000 ms     |
| Between commands     | 50 ms       |
+----------------------+-------------+
```

---

## User Interface

### Display Layout

```
Main Menu:
+------------------+
|     SCAN         |  <- Title (size 2)
|                  |
| Rotate: MENU     |  <- Line 2 (size 1)
| Press: OK        |  <- Line 3
+------------------+

Submenu (ENROLL/DELETE):
+------------------+
|    ENROLL        |
|                  |
| ID: 5            |
| Rotate: ID       |
| Press: OK        |
+------------------+

Scan Result:
+------------------+
|   Access OK      |
|                  |
| ID: 3            |
|                  |
+------------------+
```

### LED & Buzzer Feedback

```
+------------------+------------------+------------------+
| Event            | LED              | Buzzer           |
+------------------+------------------+------------------+
| Startup          | Off              | 4-tone melody    |
| Scanning         | On               | Short beep       |
| Success          | On (2 sec)       | Rising melody    |
| Denied/Fail      | Blink            | Falling melody   |
| Menu navigation  | Off              | Short beep       |
+------------------+------------------+------------------+
```

---

## Dependencies

- Arduino ESP32 Board Package
- Adafruit SSD1306 Library
- Adafruit GFX Library

## Installation

1. Install Arduino IDE and ESP32 board support
2. Install required libraries via Library Manager
3. Copy `FPC1020_ESP32.h` and `FPC1020_ESP32.cpp` to Arduino libraries folder
4. Open `FPC1020_ESP32.ino` and upload to ESP32

## Serial Monitor Output

```
=== FPC1020A Door Lock ===
Sensor initialized (19200 baud)
Ready! Menu: SCAN

=== SCAN ===
TX: F5 0C 00 00 00 00 0C F5 
RX: F5 0C 00 03 01 00 0E F5 (len=8)
Search Q3=0x01
MATCH: ID=3, Perm=1

=== ENROLL ID 5 ===
Step 1: TX: F5 01 00 05 01 00 05 F5 
RX: F5 01 00 00 00 00 01 F5 (len=8)
OK
Step 2: TX: F5 02 00 05 01 00 06 F5 
RX: F5 02 00 00 00 00 02 F5 (len=8)
OK
Step 3: TX: F5 02 00 05 01 00 06 F5 
RX: F5 02 00 00 00 00 02 F5 (len=8)
OK
Step 4: TX: F5 03 00 05 01 00 07 F5 
RX: F5 03 00 00 00 00 03 F5 (len=8)
OK
=== ENROLL SUCCESS ID=5 ===

=== LIST ===
TX: F5 2B 00 00 00 00 2B F5 
RX: F5 2B 00 14 00 00 3F F5 (len=8)
GetAllUsers dataLen=20
Data RX: F5 00 06 00 05 01 00 04 01 00 06 01 00 01 01 00 03 01 00 02 01 ... (len=23)
User count: 6
IDs: 5 4 6 1 3 2 
Sorted: 1 2 3 4 5 6

=== DELETE ID 3 ===
TX: F5 04 00 03 00 00 07 F5 
RX: F5 04 00 00 00 00 04 F5 (len=8)
Result: OK (0x00)

=== CLEAR ===
TX: F5 05 00 00 00 00 05 F5 
RX: F5 05 00 00 00 00 05 F5 (len=8)
Clear result: OK (0x00)
```

---

## Troubleshooting

```
+------------------------------+-----------------------------+------------------------------------------+
| Problem                      | Possible Cause              | Solution                                 |
+------------------------------+-----------------------------+------------------------------------------+
| No response from sensor      | Wrong wiring or baud rate   | Check TX/RX connections, verify 19200    |
| Enrollment fails final step  | Inconsistent finger place   | Keep finger steady, same position        |
| Search always times out      | Finger not detected         | Press firmly, clean sensor surface       |
| "ID in use" error            | ID already registered       | Use different ID or delete existing      |
| "Finger exists" error        | Finger already enrolled     | Use different finger or clear database   |
+------------------------------+-----------------------------+------------------------------------------+
```

---

## References

- [FPC1020A Biovo Protocol Documentation (Japanese)](http://bibohlog.seesaa.net/article/483693537.html)
- [Biovo Fingerprint Protocol](https://drive.google.com/file/d/1nP1yN7dACwIGUBrRUJXHOlDvjmE0nlT0/view?usp=drive_link)

**Note:** This project uses the Biovo protocol fingerprint module with FPC1020A sensor chip. This is different from the M5Stack Finger Unit which uses a different protocol.

---

## License

MIT License
