#include "Drivers/ymodem.h"
#include "config.h"
#include <SD.h>

static const uint8_t  YM_SOH = 0x01;
static const uint8_t  YM_STX = 0x02;
static const uint8_t  YM_EOT = 0x04;
static const uint8_t  YM_ACK = 0x06;
static const uint8_t  YM_NAK = 0x15;
static const uint8_t  YM_CAN = 0x18;
static const uint8_t  YM_C   = 0x43;

static const int YM_BLOCK_SIZE  = 1024;
static const int YM_MAX_RETRIES = 10;
static const unsigned long YM_TIMEOUT_MS = 3000;

static int ymReadByte(unsigned long timeoutMs) {
  unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (Serial.available() > 0) return Serial.read();
  }
  return -1;
}

static void ymDiscard(int n) {
  for (int i = 0; i < n; i++) ymReadByte(YM_TIMEOUT_MS);
}

static uint16_t ymCrc16(const uint8_t* data, int len) {
  uint16_t crc = 0;
  for (int i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else crc <<= 1;
    }
  }
  return crc;
}

bool ymodemSendFile(fs::FS &fs, const String &path) {
  if (!SD.begin(PIN_CD)) {
    Serial.println(F("  SD no disponible"));
    return false;
  }

  File f = fs.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    Serial.println(F("  error: no se pudo abrir el archivo"));
    return false;
  }

  String name = path;
  int slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.substring(slash + 1);
  uint32_t fsize = f.size();

  Serial.println(F("  esperando receptor YMODEM... (inicia la recepcion ahora)"));

  int c = -1;
  unsigned long start = millis();
  while (millis() - start < 60000UL) {
    c = ymReadByte(1000);
    if (c == (int)YM_C) break;
    if (c == (int)YM_CAN) {
      f.close();
      Serial.println(F("  cancelado por el receptor"));
      return false;
    }
  }
  if (c != (int)YM_C) {
    f.close();
    Serial.println(F("  error: timeout esperando al receptor"));
    return false;
  }

  uint8_t block[YM_BLOCK_SIZE];
  memset(block, 0, sizeof(block));
  snprintf((char*)block, sizeof(block), "%s", name.c_str());
  snprintf((char*)block + name.length() + 1, sizeof(block) - name.length() - 1, "%lu", (unsigned long)fsize);

  bool ok = false;
  for (int attempt = 0; attempt < YM_MAX_RETRIES && !ok; attempt++) {
    Serial.write(YM_STX);
    Serial.write((uint8_t)0);
    Serial.write((uint8_t)0xFF);
    Serial.write(block, YM_BLOCK_SIZE);
    uint16_t crc = ymCrc16(block, YM_BLOCK_SIZE);
    Serial.write((uint8_t)(crc >> 8));
    Serial.write((uint8_t)(crc & 0xFF));

    int resp = ymReadByte(YM_TIMEOUT_MS);
    if (resp == (int)YM_ACK) ok = true;
    else if (resp == (int)YM_CAN) {
      f.close();
      Serial.println(F("  cancelado por el receptor"));
      return false;
    }
  }
  if (!ok) {
    f.close();
    Serial.println(F("  error: el receptor no confirmo la cabecera"));
    return false;
  }

  c = ymReadByte(YM_TIMEOUT_MS);
  if (c != (int)YM_C) {
    f.close();
    Serial.println(F("  error: el receptor no pidio los datos"));
    return false;
  }

  uint8_t blockNum = 1;
  uint32_t sent = 0;
  while (sent < fsize) {
    uint8_t buf[YM_BLOCK_SIZE];
    memset(buf, 0x1A, sizeof(buf));
    int n = f.read(buf, YM_BLOCK_SIZE);
    if (n <= 0) break;

    ok = false;
    for (int attempt = 0; attempt < YM_MAX_RETRIES && !ok; attempt++) {
      Serial.write(YM_STX);
      Serial.write(blockNum);
      Serial.write((uint8_t)(0xFF - blockNum));
      Serial.write(buf, YM_BLOCK_SIZE);
      uint16_t crc = ymCrc16(buf, YM_BLOCK_SIZE);
      Serial.write((uint8_t)(crc >> 8));
      Serial.write((uint8_t)(crc & 0xFF));

      int resp = ymReadByte(YM_TIMEOUT_MS);
      if (resp == (int)YM_ACK) ok = true;
      else if (resp == (int)YM_CAN) {
        f.close();
        Serial.println(F("  cancelado por el receptor"));
        return false;
      }
    }
    if (!ok) {
      f.close();
      Serial.println(F("  error: fallo el envio de un bloque"));
      return false;
    }

    sent += n;
    blockNum++;
  }
  f.close();

  ok = false;
  for (int attempt = 0; attempt < YM_MAX_RETRIES && !ok; attempt++) {
    Serial.write(YM_EOT);
    int resp = ymReadByte(YM_TIMEOUT_MS);
    if (resp == (int)YM_ACK) ok = true;
  }

  ymReadByte(YM_TIMEOUT_MS); // algunos receptores mandan 'C' antes del bloque final

  memset(block, 0, sizeof(block));
  Serial.write(YM_STX);
  Serial.write((uint8_t)0);
  Serial.write((uint8_t)0xFF);
  Serial.write(block, YM_BLOCK_SIZE);
  uint16_t crc = ymCrc16(block, YM_BLOCK_SIZE);
  Serial.write((uint8_t)(crc >> 8));
  Serial.write((uint8_t)(crc & 0xFF));
  ymReadByte(YM_TIMEOUT_MS);

  Serial.println();
  Serial.println(F("  ok: transferencia completa"));
  return true;
}

String ymodemReceiveFile(fs::FS &fs, const String &destDir) {
  if (!SD.begin(PIN_CD)) {
    Serial.println(F("  SD no disponible"));
    return "";
  }

  Serial.println(F("  esperando envio YMODEM... (inicia el envio ahora)"));

  uint8_t block[YM_BLOCK_SIZE];
  String filename = "";
  uint32_t filesize = 0;
  bool gotHeader = false;

  for (int attempt = 0; attempt < 30 && !gotHeader; attempt++) {
    Serial.write(YM_C);
    int type = ymReadByte(2000);
    if (type == (int)YM_SOH || type == (int)YM_STX) {
      int blockSize = (type == (int)YM_STX) ? YM_BLOCK_SIZE : 128;
      int blkNum = ymReadByte(YM_TIMEOUT_MS);
      ymReadByte(YM_TIMEOUT_MS); // ~blkNum

      if (blkNum != 0) {
        ymDiscard(blockSize + 2);
        Serial.write(YM_NAK);
        continue;
      }

      for (int i = 0; i < blockSize; i++) {
        int b = ymReadByte(YM_TIMEOUT_MS);
        block[i] = (b < 0) ? 0 : (uint8_t)b;
      }
      ymDiscard(2); // crc

      if (block[0] == 0) {
        Serial.write(YM_ACK);
        return "";
      }

      filename = String((char*)block);
      char* sizeStr = (char*)block + filename.length() + 1;
      filesize = strtoul(sizeStr, NULL, 10);

      Serial.write(YM_ACK);
      gotHeader = true;
    } else if (type == (int)YM_CAN) {
      return "";
    }
  }
  if (!gotHeader) {
    Serial.println(F("  error: timeout esperando el envio"));
    return "";
  }

  int slash = filename.lastIndexOf('/');
  if (slash >= 0) filename = filename.substring(slash + 1);
  String fullPath = destDir;
  if (!fullPath.endsWith("/")) fullPath += "/";
  fullPath += filename;

  File out = fs.open(fullPath, FILE_WRITE);
  if (!out) {
    Serial.println(F("  error: no se pudo crear el archivo en la SD"));
    Serial.write(YM_CAN);
    Serial.write(YM_CAN);
    return "";
  }

  Serial.write(YM_C);

  uint8_t expectedBlk = 1;
  uint32_t written = 0;
  bool done = false;

  while (!done) {
    int type = ymReadByte(YM_TIMEOUT_MS);
    if (type == (int)YM_EOT) {
      Serial.write(YM_ACK);
      done = true;
      break;
    }
    if (type == (int)YM_CAN) {
      out.close();
      fs.remove(fullPath);
      return "";
    }
    if (type != (int)YM_SOH && type != (int)YM_STX) {
      continue;
    }

    int blockSize = (type == (int)YM_STX) ? YM_BLOCK_SIZE : 128;
    int blkNum = ymReadByte(YM_TIMEOUT_MS);
    ymReadByte(YM_TIMEOUT_MS); // ~blkNum

    uint8_t data[YM_BLOCK_SIZE];
    for (int i = 0; i < blockSize; i++) {
      int b = ymReadByte(YM_TIMEOUT_MS);
      data[i] = (b < 0) ? 0 : (uint8_t)b;
    }
    ymDiscard(2); // crc

    if (blkNum != (int)expectedBlk) {
      Serial.write(YM_ACK);
      continue;
    }

    uint32_t toWrite = blockSize;
    if (filesize > 0 && written + toWrite > filesize) {
      toWrite = filesize - written;
    }
    out.write(data, toWrite);
    written += toWrite;
    expectedBlk++;

    Serial.write(YM_ACK);
  }

  out.close();

  int type = ymReadByte(YM_TIMEOUT_MS);
  if (type == (int)YM_SOH || type == (int)YM_STX) {
    int blockSize = (type == (int)YM_STX) ? YM_BLOCK_SIZE : 128;
    ymDiscard(blockSize + 2);
    Serial.write(YM_ACK);
  }

  Serial.println();
  Serial.print(F("  ok: recibido "));
  Serial.print(filename);
  Serial.print(F(" ("));
  Serial.print(written);
  Serial.println(F(" bytes)"));

  return filename;
}
