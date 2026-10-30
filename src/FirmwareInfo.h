#pragma once
#include <ArduinoJson.h>
#include <FirmwareBuild.h>
#include "ServiceOrigin.h"
namespace FirmwareInfo {
inline void write(JsonObject o){o["version"]=FirmwareBuild::version;o["commit"]=FirmwareBuild::commit;o["source"]=FirmwareBuild::source;o["dirty"]=FirmwareBuild::dirty;o["local_protocol"]=FirmwareBuild::localProtocol;o["environment"]=ServiceOrigin::mode;o["service_origin"]=ServiceOrigin::url;}
}
