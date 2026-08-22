#include "DeviceSettings.h"
#include <Preferences.h>

void DeviceSettings::load() {
  Preferences p; p.begin("dexmon", false);
  data_.hostname = p.getString("hostname", data_.hostname);
  if (data_.hostname == "dexcom-g7-monitor") {
    data_.hostname = "esp32-cgm-monitor";
    p.putString("hostname", data_.hostname);
  }
  data_.timezone = p.getString("timezone", data_.timezone);
  data_.clock24Hour = p.getBool("clock24", false);
  data_.screenScheduleEnabled = p.getBool("screenSched", false);
  data_.screenOffMinute = min<uint16_t>(p.getUShort("screenOffMin", 0), 1439);
  data_.screenOnMinute = min<uint16_t>(p.getUShort("screenOnMin", 420), 1439);
  data_.dexcomLogin = p.getString("dexLogin", "");
  data_.dexcomPassword = p.getString("dexPassword", "");
  p.end();
}
void DeviceSettings::save() const {
  Preferences p; p.begin("dexmon", false);
  p.putString("hostname", data_.hostname); p.putString("timezone", data_.timezone);
  p.putBool("clock24", data_.clock24Hour); p.putBool("screenSched", data_.screenScheduleEnabled);
  p.putUShort("screenOffMin", data_.screenOffMinute); p.putUShort("screenOnMin", data_.screenOnMinute);
  p.putString("dexLogin", data_.dexcomLogin); p.putString("dexPassword", data_.dexcomPassword); p.end();
}
bool DeviceSettings::parseClockMinutes(const String &v, uint16_t &result) {
  if(v.length()!=5||v[2]!=':'||!isDigit(v[0])||!isDigit(v[1])||!isDigit(v[3])||!isDigit(v[4]))return false;
  int h=v.substring(0,2).toInt(),m=v.substring(3).toInt();if(h>23||m>59)return false;result=h*60+m;return true;
}
String DeviceSettings::formatClockMinutes(uint16_t m){char b[6];snprintf(b,sizeof(b),"%02u:%02u",m/60,m%60);return b;}
String DeviceSettings::cleanHostname(String v){v.toLowerCase();String out;for(char c:v)if(isAlphaNumeric(c)||c=='-')out+=c;return out.length()?out:String("esp32-cgm-monitor");}
