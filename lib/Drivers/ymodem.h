#pragma once
#include <Arduino.h>
#include <FS.h>

// Envia un archivo de la SD por YMODEM sobre Serial (USB).
// El PC debe iniciar la recepcion (TeraTerm: File->Transfer->YMODEM->Receive,
// o en Linux: rz -y) antes de que expire el timeout de espera.
bool ymodemSendFile(fs::FS &fs, const String &path);

// Recibe un archivo por YMODEM sobre Serial (USB) y lo guarda en destDir.
// El PC debe iniciar el envio (TeraTerm: File->Transfer->YMODEM->Send,
// o en Linux: sz -y archivo) antes de que expire el timeout de espera.
// Devuelve el nombre del archivo recibido, o "" si fallo/no llego nada.
String ymodemReceiveFile(fs::FS &fs, const String &destDir);
