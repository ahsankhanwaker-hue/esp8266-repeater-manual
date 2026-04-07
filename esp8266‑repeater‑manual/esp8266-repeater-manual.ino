#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>

extern "C" {
  #include "lwip/napt.h"
}

#define AP_SSID "FuckerESP"
#define AP_PASS "espbyrni"

#define ADMIN_USER "rni"
#define ADMIN_PASS "rni@@007"

#define EEPROM_SIZE 96
#define DNS_PORT 53
#define NAPT 1
#define NAPT_PORT 10

ESP8266WebServer server(80);
DNSServer dnsServer;

bool loggedIn = false;

// -------- EEPROM --------
void saveCreds(String s, String p){
  EEPROM.begin(EEPROM_SIZE);
  for(int i=0;i<32;i++) EEPROM.write(i, i<s.length()?s[i]:0);
  for(int i=32;i<96;i++) EEPROM.write(i, i-32<p.length()?p[i-32]:0);
  EEPROM.commit();
}

void loadCreds(String &s, String &p){
  EEPROM.begin(EEPROM_SIZE);
  char ssid[33], pass[65];
  for(int i=0;i<32;i++) ssid[i]=EEPROM.read(i);
  for(int i=32;i<96;i++) pass[i-32]=EEPROM.read(i);
  ssid[32]=0; pass[64]=0;
  s = String(ssid);
  p = String(pass);
}

// -------- Hacker UI --------
String loginPage(){
return R"rawliteral(
<html><head>
<style>
body{background:#000;color:#0f0;font-family:monospace;text-align:center}
input,button{padding:10px;margin:5px;background:#111;color:#0f0;border:1px solid #0f0}
h1{animation:glow 1s infinite alternate}
@keyframes glow{from{text-shadow:0 0 5px #0f0}to{text-shadow:0 0 20px #0f0}}
</style>
</head>
<body>
<h1>ESP Hacker Panel</h1>
<input id=u placeholder=user><br>
<input id=p placeholder=pass type=password><br>
<button onclick="go()">LOGIN</button>
<script>
function go(){
fetch('/login?u='+u.value+'&p='+p.value).then(()=>location.reload())
}
</script>
</body></html>
)rawliteral";
}

String panelPage(){
return R"rawliteral(
<html><head>
<style>
body{background:#000;color:#0f0;font-family:monospace}
button{padding:10px;margin:5px;background:#111;color:#0f0;border:1px solid #0f0}
.card{border:1px solid #0f0;padding:10px;margin:10px}
</style>
</head>
<body>
<h2>ESP8266 Control Panel</h2>

<div class=card>
<button onclick="scan()">Scan WiFi</button>
<div id=list></div>
</div>

<div class=card>
<h3>Signal Strength</h3>
<div id=rssi>Loading...</div>
</div>

<script>
function scan(){
fetch('/scan').then(r=>r.json()).then(d=>{
let h='';
d.forEach(n=>{
h+=`<p>${n.ssid} (${n.rssi}) 
<button onclick="conn('${n.ssid}')">Connect</button></p>`
});
list.innerHTML=h;
});
}

function conn(s){
let p=prompt("Password for "+s);
fetch('/connect?ssid='+s+'&pass='+p);
}

setInterval(()=>{
fetch('/rssi').then(r=>r.text()).then(d=>rssi.innerHTML=d)
},2000);
</script>
</body></html>
)rawliteral";
}

// -------- Routes --------
void handleRoot(){
  if(!loggedIn) server.send(200,"text/html",loginPage());
  else server.send(200,"text/html",panelPage());
}

void handleLogin(){
  if(server.arg("u")==ADMIN_USER && server.arg("p")==ADMIN_PASS)
    loggedIn=true;
  server.send(200,"text/plain","OK");
}

void handleScan(){
  int n=WiFi.scanNetworks();
  String json="[";
  for(int i=0;i<n;i++){
    if(i) json+=",";
    json+="{\"ssid\":\""+WiFi.SSID(i)+"\",\"rssi\":"+String(WiFi.RSSI(i))+"}";
  }
  json+="]";
  server.send(200,"application/json",json);
}

void handleConnect(){
  String s=server.arg("ssid");
  String p=server.arg("pass");
  saveCreds(s,p);
  WiFi.begin(s.c_str(),p.c_str());
  server.send(200,"text/plain","Connecting...");
}

void handleRSSI(){
  if(WiFi.status()==WL_CONNECTED)
    server.send(200,"text/plain",String(WiFi.RSSI()));
  else
    server.send(200,"text/plain","Not connected");
}

// -------- Setup --------
void setup(){
  Serial.begin(115200);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  String ss, pp;
  loadCreds(ss, pp);
  if(ss.length()>0){
    WiFi.begin(ss.c_str(), pp.c_str());
  }

  ip_napt_init(NAPT, NAPT_PORT);
  ip_napt_enable_no(SOFTAP_IF, 1);

  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.on("/scan", handleScan);
  server.on("/connect", handleConnect);
  server.on("/rssi", handleRSSI);

  server.begin();
}

// -------- Loop --------
void loop(){
  dnsServer.processNextRequest();
  server.handleClient();
}
