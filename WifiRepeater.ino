#if LWIP_FEATURES && !LWIP_IPV6

#include <FS.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <lwip/napt.h>
#include <lwip/dns.h>
#include <dhcpserver.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#define HAVE_NETDUMP 0

#ifndef STASSID
#define STASSID "ESP8266-AP"
#define STAPSK  "12345678"
#endif

#define NAPT 1000
#define NAPT_PORT 10
#define FILE_PATH "/Wifi.txt"
#define WIFI_CONNECT_RETRY 100
#if HAVE_NETDUMP

#include <NetDump.h>

void dump(int netif_idx, const char* data, size_t len, int out, int success) {
	(void)success;
	Serial.print(out ? F("out ") : F(" in "));
	Serial.println(netif_idx);

	// optional filter example: if (netDump_is_ARP(data))
	{
		netDump(Serial, data, len);
		//netDumpHex(Serial, data, len);
	}
}
#endif
const char* ScanPage = "<!DOCTYPE html><html lang='en'><head></head><body> <h1 style='text-align: center;'><span style='text-decoration: underline;'><strong>Welcome to ESP8266 Wifi Extender</strong></span></h1> <p style='text-align: center;'><a title='Scan for networks' href='/scan'><strong>Scan for networks</strong></a></p><p style='text-align: center;'><strong>&copy;<a href='mailto:gulshanjitm@gmail.com'>Gulshan Kumar K</a></strong></p></body></html>";
const char* ScanResultsPageHead = "<!DOCTYPE html><html lang='en'><head></head><body> <h1 style='text-align: center;'>ESP8266 Wifi Extender Configuration</h1> <p style='text-align: center;'><a title='Scan Again' href='/scan'><strong>Scan Again</strong></a></p><hr/> <p style='text-align: center;'><strong>Networks Found : ";
const char* ScanResultsPageFoot = "<hr/> <p style='text-align: center;'>&nbsp;<strong>&copy;<a href='mailto:gulshanjitm@gmail.com'>Gulshan Kumar K</a></strong></p></body></html>";
char* readSsid;
char* readPassword;
char* exSsid;
char* exPassword;
boolean InitializePersistence();
void ReadSsidPassword(char* ssid, char* password);
void WriteToFile();
void SetupNAT();
void SetupAP();
void handleRoot();
void handleScan();
void SetupServer();
void handleConfigure();
void handleFinish();
void ResetDevice();
boolean PreviousResetButtonRead = 1;
char* GetConfigurePage(char* ssid, char* pass, char* strength);
char* FormatScanResultsPage(int totalNetworks, char** scannedSsid, int* scannedRssi);

ESP8266WebServer server(80);

void setup() {
	Serial.begin(115200);
	Serial.println("Setting hardware reset button");
	pinMode(0, INPUT_PULLUP);
	Serial.print("\n\nNAPT Range extender\n");
	Serial.print("Heap on start: %d");
	Serial.println(ESP.getFreeHeap());

	if (InitializePersistence())
	{
		Serial.println("Persistence Initialization successful!!!");
		/*char* ssid = "";
		char* password = "";*/

		Serial.print("After read: SSID: ");
		Serial.println(readSsid);
		Serial.print("Password: ");
		Serial.println(readPassword);
	}
	else
	{
		Serial.println("Persistence Initialization failure!!!");
	}

#if HAVE_NETDUMP
	phy_capture = dump;
#endif

	// first, connect to STA so we can get a proper local DNS server
	WiFi.mode(WIFI_STA);
	if(WiFi.hostname("esp8266gulu"))
	{
		Serial.println("Hostname set");
	}
	WiFi.begin(readSsid, readPassword);
	uint16 i = 0;
	for (i = 0; i < WIFI_CONNECT_RETRY; i++)
	{
		if (WiFi.status() == WL_CONNECTED)
		{
			break;
		}
		Serial.print(".");
		delay(500);
	}
	if (i == WIFI_CONNECT_RETRY)
	{
		SetupAP();
	}
	else
	{
		SetupNAT();
	}
}

#else

void setup() {
	Serial.begin(115200);
	Serial.printf("\n\nNAPT not supported in this configuration\n");
}

#endif

void loop()
{
	if(digitalRead(0) == LOW && PreviousResetButtonRead)
	{
		PreviousResetButtonRead = 0;
		ResetDevice();
	}
	else if(digitalRead(0) == HIGH && !PreviousResetButtonRead)
	{
		PreviousResetButtonRead = 1;
	}
	server.handleClient();
}

void ResetDevice()
{
	if (!LittleFS.begin()) {
		Serial.println("LittleFS mount failed");
		Serial.println("Formatting LittleFS filesystem");
		LittleFS.format();
		Serial.println("Mount LittleFS");
	}

	Serial.printf("Deleting file: %s\n", FILE_PATH);
	if (LittleFS.remove(FILE_PATH))
	{
		Serial.println("File deleted");
	}
	else
	{
		Serial.println("Delete failed");
	}

	ESP.restart();
}

void WriteToFile()
{
	if (!LittleFS.begin()) {
		Serial.println("LittleFS mount failed");
		Serial.println("Formatting LittleFS filesystem");
		LittleFS.format();
		Serial.println("Mount LittleFS");
	}

	Serial.printf("Deleting file: %s\n", FILE_PATH);
	if (LittleFS.remove(FILE_PATH))
	{
		Serial.println("File deleted");
	}
	else
	{
		Serial.println("Delete failed");
	}

	Serial.printf("Reading file: %s\n", FILE_PATH);
	File file = LittleFS.open(FILE_PATH, "w");
	if (!file)
	{
		Serial.println("Failed to open file for writing");
	}
	String text = "";
	text = text + readSsid + "\n" + readPassword + "\n" + exSsid + "\n" + exPassword;
	Serial.println("To write in fs: ");
	Serial.println(text);
	if (file.print(text))
	{
		Serial.println("File written");
	}
	else
	{
		Serial.println("Write failed");
	}
	delay(1000); // Make sure the CREATE and LASTWRITE times are different
	file.close();
}
boolean InitializePersistence()
{
	boolean flag = 1;
	Serial.println("Initializing File System!!!");
	if (!LittleFS.begin()) {
		Serial.println("LittleFS mount failed");
		Serial.println("Formatting LittleFS filesystem");
		LittleFS.format();
		Serial.println("Mount LittleFS");
	}

	Serial.printf("Reading file: %s\n", FILE_PATH);
	File file = LittleFS.open(FILE_PATH, "r");
	if (!file)
	{
		Serial.println("Failed to open file for reading");
		flag = 0;
		Serial.printf("Writing file: %s\n", FILE_PATH);

		File file = LittleFS.open(FILE_PATH, "w");
		if (!file) {
			Serial.println("Failed to open file for writing");
			flag = 0;
			return flag;
		}
		if (file.print("ssid\npassword")) {
			Serial.println("File written");
		}
		else {
			Serial.println("Write failed");
		}
		delay(1000); // Make sure the CREATE and LASTWRITE times are different
		file.close();
	}

	Serial.print("Read from file: ");
	/*while (file.available()) {
		Serial.write(file.read());
		Serial.println();
	}*/

	uint x = 0;
	while (file.available()) {
		String buffer("");
		buffer = file.readStringUntil('\n');
		switch (x)
		{
		case 0:
			Serial.print("Network SSID : ");
			Serial.println(buffer);
			Serial.print("Network SSID Buffer Length: ");
			Serial.println(buffer.length());
			readSsid = (char*)malloc(sizeof(char) * (buffer.length() + 1));
			buffer.toCharArray(readSsid, buffer.length() + 1);
			break;
		case 1:
			Serial.print("Network Password : ");
			Serial.println(buffer);
			Serial.print("Password Buffer Length: ");
			Serial.println(buffer.length());
			readPassword = (char*)malloc(sizeof(char) * (buffer.length() + 1));
			buffer.toCharArray(readPassword, buffer.length() + 1);
			break;
		case 2:
			Serial.print("Extended SSID : ");
			Serial.println(buffer);
			Serial.print("Extended SSID Buffer Length: ");
			Serial.println(buffer.length());
			exSsid = (char*)malloc(sizeof(char) * (buffer.length() + 1));
			buffer.toCharArray(exSsid, buffer.length() + 1);
			break;
		case 3:
			Serial.print("Extended Password : ");
			Serial.println(buffer);
			Serial.print("Extended Password Buffer Length: ");
			Serial.println(buffer.length());
			exPassword = (char*)malloc(sizeof(char) * (buffer.length() + 1));
			buffer.toCharArray(exPassword, buffer.length() + 1);
			break;
			break;
		default:
			break;
		}
		x++;
	}

	file.close();

	return flag;
}

void SetupNAT()
{
	Serial.print("\nSTA: ");
	Serial.println(WiFi.localIP().toString().c_str());
	Serial.print("dns1: ");
	Serial.println(WiFi.dnsIP(0).toString().c_str());
	Serial.print("dns2: ");
	Serial.println(WiFi.dnsIP(1).toString().c_str());

	// give DNS servers to AP side
	dhcps_set_dns(0, WiFi.dnsIP(0));
	dhcps_set_dns(1, WiFi.dnsIP(1));

	WiFi.softAPConfig(  // enable AP, with android-compatible google domain
		IPAddress(172, 217, 28, 254),
		IPAddress(172, 217, 28, 254),
		IPAddress(255, 255, 255, 0));
	WiFi.softAP(exSsid, exPassword);
	Serial.print("AP: ");
	Serial.println(WiFi.softAPIP().toString().c_str());

	Serial.print("Heap before: ");
	Serial.println(ESP.getFreeHeap());
	err_t ret = ip_napt_init(NAPT, NAPT_PORT);
	Serial.print("ip_napt_init(");
	Serial.print(NAPT);
	Serial.print(",");
	Serial.print(NAPT_PORT);
	Serial.print(": ret=");
	Serial.println((int)ret);
	//Serial.print("OK=");
	//Serial.println(ERR_OK));

	if (ret == ERR_OK) {
		ret = ip_napt_enable_no(SOFTAP_IF, 1);
		Serial.print("ip_napt_enable_no(SOFTAP_IF): ret");
		Serial.println((int)ret);
		Serial.print("OK=");
		Serial.println((int)ERR_OK);

		if (ret == ERR_OK) {
			Serial.print("WiFi Network '");
			Serial.print(readSsid);
			Serial.print("' with password ");
			Serial.print(exPassword);
			Serial.print(" is now NATed behind '");
			Serial.print(exSsid);
		}
	}
	Serial.print("Heap after napt init: ");
	Serial.print(ESP.getFreeHeap());
	if (ret != ERR_OK) {
		Serial.println("NAPT initialization failed");
	}

	SetupServer();
}

void handleRoot()
{
	Serial.println("Got request from :");
	server.send(200, "text/html", ScanPage);
}


void handleScan()
{
	Serial.println("Got scan request from :");

	// WiFi.scanNetworks will return the number of networks found
	int n = WiFi.scanNetworks();
	char** scannedSsid = (char**)malloc(sizeof(char*) * n);
	int* scannedRssi = (int*)malloc(sizeof(int) * n);

	Serial.println("scan done");
	if (n == 0)
	{
		Serial.println("no networks found");
	}
	else
	{
		Serial.print(n);
		Serial.println(" networks found");
		for (int i = 0; i < n; ++i)
		{
			// Print SSID and RSSI for each network found
			Serial.print(i + 1);
			Serial.print(": ");
			scannedSsid[i] = (char*)malloc(sizeof(char) * (WiFi.SSID(i).length() + 1));
			WiFi.SSID(i).toCharArray(scannedSsid[i], WiFi.SSID(i).length() + 1);
			Serial.print(scannedSsid[i]);
			Serial.print(" (");

			scannedRssi[i] = WiFi.RSSI(i);
			Serial.print(scannedRssi[i]);
			Serial.print(")");
			Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
			delay(10);
		}
	}

	char* scanResultsPage = FormatScanResultsPage(n, scannedSsid, scannedRssi);
	server.send(200, "text/html", scanResultsPage);
}

void handleConfigure()
{
	String message = "Number of args received :";
	message += server.args(); //Get number of parameters
	Serial.println(message);

	String ssid = server.arg(0);
	Serial.println(ssid);
	String pass = server.arg(1);
	Serial.println(pass);
	String strength = server.arg(2);
	Serial.println(strength);
	char* chssid = (char*)malloc(sizeof(char) * (ssid.length() + 1));
	ssid.toCharArray(chssid, ssid.length() + 1);
	char* chpass = (char*)malloc(sizeof(char) * (pass.length() + 1));
	pass.toCharArray(chpass, pass.length() + 1);
	char* chstrength = (char*)malloc(sizeof(char) * (strength.length() + 1));
	strength.toCharArray(chstrength, strength.length() + 1);
	char* str = GetConfigurePage(chssid, chpass, chstrength);
	server.send(200, "text/html", str);
}

void handleFinish()
{
	String message = "Number of args received :";
	message += server.args(); //Get number of parameters
	Serial.println(message);

	String nwssid = server.arg(0);
	Serial.println(nwssid);
	String nwpass = server.arg(1);
	Serial.println(nwpass);
	String exssid = server.arg(2);
	Serial.println(exssid);
	String expass = server.arg(3);
	Serial.println(expass);

	readSsid = (char*)malloc(sizeof(char) * (nwssid.length() + 1));
	nwssid.toCharArray(readSsid, nwssid.length() + 1);
	readPassword = (char*)malloc(sizeof(char) * (nwpass.length() + 1));
	nwpass.toCharArray(readPassword, nwpass.length() + 1);
	exSsid = (char*)malloc(sizeof(char) * (exssid.length() + 1));
	exssid.toCharArray(exSsid, exssid.length() + 1);
	exPassword = (char*)malloc(sizeof(char) * (expass.length() + 1));
	expass.toCharArray(exPassword, expass.length() + 1);

	server.send(200, "text/html", "<h1>Successful... Device will restart in 2 seconds...</h1>");

	WriteToFile();
	delay(2000);
	ESP.restart();
}

char* GetConfigurePage(char* ssid, char* pass, char* strength)
{
	String final = "";
	final = final +
		"<!DOCTYPE html><html lang='en'><head></head><body> <h1 style='text-align: center;'>ESP8266 Wifi Extender Additional Configuration</h1> <p style='text-align: center;'><a title='Home' href='//'><strong>Home</strong></a></p><hr/> <p>Connect to Network:<strong><label id='nwssid'>"
		+ ssid + "</label></strong></p><input id='nwpass' type='hidden' value='" + pass + "'/><input id='defssid' type='hidden' value='"
		+ ssid + "extended'/><p>Signal Strength:<strong><label id='strength'>" + strength
		+ "</label></strong> dBm</p><hr/> <p>Extended network SSID:<input id='exssid' type='textbox' value=''/></p><p>Password:<input id='expass' type='textbox' value=''/></p><p><input type='button' value='Use Default' onclick='OnDefault()'/></p>"
		+ "<p><input type='button' value='Finish Configuration' onclick='FinishConfig()'/></p><hr//> <p style='text-align: center;'>&nbsp;<strong>&copy;<a href='mailto:gulshanjitm@gmail.com'>Gulshan Kumar K</a></strong></p>"
		+ "<script type='text/javascript'> function OnDefault(){document.getElementById('exssid').value=document.getElementById('defssid').value; document.getElementById('expass').value=document.getElementById('nwpass').value;}function FinishConfig(){nwssid=document.getElementById('nwssid').innerHTML; nwpass=document.getElementById('nwpass').value; exssid=document.getElementById('exssid').value; expass=document.getElementById('expass').value; location.replace('/finish?nwssid=' + nwssid + '&nwpass=' + nwpass + '&exssid=' + exssid + '&expass=' + expass);}</script></body></html>";

	char* res = (char*)malloc(sizeof(char) * (final.length() + 1));
	final.toCharArray(res, final.length() + 1);

	return res;
}

void SetupAP()
{
	Serial.println("Setting up as Wifi Access Point to be initially configured");
	WiFi.mode(WIFI_AP);
	WiFi.softAP(STASSID, STAPSK);

	//TODO: DNS Code here
	//if (MDNS.begin("esp8266", WiFi.softAPIP())) 
	//{  //Start mDNS with name esp8266
	//	Serial.println("MDNS started");
	//}
	//MDNS.addService("http", "tcp", 80);

	Serial.print("AP IP address: ");
	Serial.println(WiFi.softAPIP());

	SetupServer();
}

void SetupServer()
{
	server.on("/", handleRoot);
	server.on("/scan", handleScan);
	server.on("/configure", handleConfigure);
	server.on("/finish", handleFinish);
	server.begin();
	Serial.println("HTTP server started");
}

char* FormatScanResultsPage(int totalNetworks, char** scannedSsid, int* scannedRssi)
{
	String final("");
	final = final + ScanResultsPageHead + totalNetworks + "</strong></p><hr/>";
	for (int x = 0; x < totalNetworks; x++)
	{
		final = final + "<p>SSID: <b><label id='ssid" + (x + 1) + "'>" + scannedSsid[x]
			+ "</label></b></p><p>Signal Strength:<b><label id='strength" + (x + 1) + "'>" + scannedRssi[x] + "</label></b> dBm</p><p>Password:&nbsp; <input id='pass"
			+ (x + 1) + "' type='textbox'/></p><p><input type='button' value='Connect' onclick='redirectToOther(" + (x + 1) + ")'></p><hr />";
		/*sprintf(final, "%s<p>SSID: <label id='ssid%d'>%s</label></p><p>Signal Strength:<label id='strength%d'>%d</label> dBm</p><p>Password:&nbsp; <input id='pass%d' type='textbox'/></p><p><span style='color: #0000ff;'><a style='background-color: #ffffff;' title='Connect' href='/connect'>Connect</a></span></p>",
			final,x + 1, scannedSsid[x], x + 1, (int)scannedRssi[x], x + 1);*/
	}
	//sprintf(final, "%s%s", final, ScanResultsPageFoot);
	final = final + ScanResultsPageFoot;
	final = final + "<script type='text/javascript'> function redirectToOther(id){ssid=document.getElementById('ssid' + id).innerHTML; password=document.getElementById('pass' + id).value; strength=document.getElementById('strength'+id).innerHTML; location.replace('/configure?ssid=' + ssid + '&password=' + password + '&strength=' + strength);}</script>";
	char* res = (char*)malloc(sizeof(char) * (final.length() + 1));
	final.toCharArray(res, final.length() + 1);
	Serial.print("Length: ");
	Serial.println(final.length());
	return res;
}