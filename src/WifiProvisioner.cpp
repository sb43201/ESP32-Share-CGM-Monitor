#include "WifiProvisioner.h"
#include <WiFi.h>
#include <WiFiManager.h>

String WifiProvisioner::setupSsid() const {char suffix[7];snprintf(suffix,sizeof(suffix),"%06llX",static_cast<unsigned long long>(ESP.getEfuseMac()&0xFFFFFFULL));return "CGMMonitor-Setup-"+String(suffix);}
bool WifiProvisioner::begin() {
  auto &s=settings_.data(); const bool needsCredentials=!settings_.hasDexcomCredentials();
  char host[33],tz[65],clock[4],off[6],on[6],schedule[4],dexLogin[97],dexPassword[65]{};
  s.hostname.toCharArray(host,sizeof(host));s.timezone.toCharArray(tz,sizeof(tz));
  snprintf(clock,sizeof(clock),"%s",s.clock24Hour?"24":"12");
  DeviceSettings::formatClockMinutes(s.screenOffMinute).toCharArray(off,sizeof(off));
  DeviceSettings::formatClockMinutes(s.screenOnMinute).toCharArray(on,sizeof(on));snprintf(schedule,sizeof(schedule),"%s",s.screenScheduleEnabled?"on":"off");
  s.dexcomLogin.toCharArray(dexLogin,sizeof(dexLogin));
  WiFiManager manager;manager.setConfigPortalTimeout(180);manager.setBreakAfterConfig(true);
  WiFiManagerParameter hostP("host","Device hostname",host,32),tzP("timezone","POSIX timezone / UTC offset rule",tz,64);
  WiFiManagerParameter clockP("clock","Clock format (12 or 24)",clock,3),scheduleP("screenSchedule","Screen schedule (on or off)",schedule,3);
  WiFiManagerParameter offP("screenOff","Screen off time (HH:MM)",off,5),onP("screenOn","Screen on time (HH:MM)",on,5);
  WiFiManagerParameter dexLoginP("dexLogin","Dexcom publisher login",dexLogin,96);
  WiFiManagerParameter dexPasswordP("dexPassword","Dexcom publisher password",dexPassword,64,"type='password'");
  manager.addParameter(&hostP);manager.addParameter(&tzP);manager.addParameter(&clockP);manager.addParameter(&scheduleP);manager.addParameter(&offP);manager.addParameter(&onP);
  manager.addParameter(&dexLoginP);manager.addParameter(&dexPasswordP);
  WiFi.setHostname(s.hostname.c_str());String ap=setupSsid();Serial.printf("[WIFI] Provisioning AP if needed: %s\n",ap.c_str());
  bool connected=manager.autoConnect(ap.c_str());
  if(connected&&needsCredentials&&(!String(dexLoginP.getValue()).length()||!String(dexPasswordP.getValue()).length())){Serial.println("[DEXCOM] Credentials missing; opening setup portal");manager.setConfigPortalBlocking(true);manager.startConfigPortal(ap.c_str());connected=WiFi.status()==WL_CONNECTED;}
  if(!connected){Serial.println("[WIFI] Setup timed out; continuing offline");return false;}
  uint16_t offMin=s.screenOffMinute,onMin=s.screenOnMinute;
  s.hostname=DeviceSettings::cleanHostname(hostP.getValue());if(String(tzP.getValue()).length())s.timezone=tzP.getValue();
  s.clock24Hour=String(clockP.getValue())=="24";s.screenScheduleEnabled=String(scheduleP.getValue()).equalsIgnoreCase("on");
  if(DeviceSettings::parseClockMinutes(offP.getValue(),offMin))s.screenOffMinute=offMin;if(DeviceSettings::parseClockMinutes(onP.getValue(),onMin))s.screenOnMinute=onMin;
  if(String(dexLoginP.getValue()).length())s.dexcomLogin=dexLoginP.getValue();
  if(String(dexPasswordP.getValue()).length())s.dexcomPassword=dexPasswordP.getValue();
  settings_.save();Serial.printf("[WIFI] Connected IP: %s\n",WiFi.localIP().toString().c_str());return true;
}
