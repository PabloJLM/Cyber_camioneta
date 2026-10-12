#pragma once
#include <Print.h>
#include <WString.h>

// Comandos "alu ..." compartidos entre la terminal de Ajustes (serie) y la
// terminal Avanzada remota (TCP). "c" es Serial o un WiFiClient: ambos
// heredan de Print, asi que la misma implementacion sirve para los dos.
//
// Devuelve true si cmd empezaba con "alu" y ya fue atendido (con su propia
// respuesta, incluyendo errores de uso); false si no era un comando de alu.
bool aluTerminalHandle(Print &c, const String &cmd);
