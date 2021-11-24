#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/* ---------------------------CHANGE ME--------------------------------- */

// Wifi Parameters
#define WIFI_SSID "Wifi_Network"
#define WIFI_PASSWORD "Password"

// MQTT Parameter
#define MQTT_BROKER "192.168.x.x"
#define MQTT_CLIENTID "garage-cover"
#define MQTT_USERNAME "my-username"
#define MQTT_PASSWORD "my-password"


/* ---------DONT CHANGE BELOW, unless you know what you are doing------------ */

// ESP8266 PINs
#define DOOR_OPEN_PIN 4     // D2
#define DOOR_CLOSE_PIN 4    // D2
#define DOOR_SWITCH_PIN 14  // D5
#define DOOR_STATUS_PIN 2   // D4

// Static IP Parameters
#define STATIC_IP false
#define IP 192,168,0,0
#define GATEWAY 192,168,0,0
#define SUBNET 255,255,255,0

// Relay Parameters
#define ACTIVE_HIGH_RELAY false

// Door  Parameters
#define DOOR_ALIAS "Door"
#define MQTT_DOOR_ACTION_TOPIC "garage-cover/door/action"
#define MQTT_DOOR_STATUS_TOPIC "garage-cover/door/status"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const boolean static_ip = STATIC_IP;
IPAddress ip(IP);
IPAddress gateway(GATEWAY);
IPAddress subnet(SUBNET);

const char* mqtt_broker = MQTT_BROKER;
const char* mqtt_clientId = MQTT_CLIENTID;
const char* mqtt_username = MQTT_USERNAME;
const char* mqtt_password = MQTT_PASSWORD;

const boolean activeHighRelay = ACTIVE_HIGH_RELAY;

const char* door_alias = DOOR_ALIAS;
const char* mqtt_door_action_topic = MQTT_DOOR_ACTION_TOPIC;
const char* mqtt_door_status_topic = MQTT_DOOR_STATUS_TOPIC;
const int door_openPin = DOOR_OPEN_PIN;
const int door_closePin = DOOR_CLOSE_PIN;
const int door_switchPin = DOOR_SWITCH_PIN;
const int door_statusPin = DOOR_STATUS_PIN;

const int relayActiveTime = 500;
int debounceTime = 750;
int door_lastStatusValue = 2;
unsigned long door_lastSwitchTime = 0;
unsigned long door_lastButtonTime = 0;
int buttonState = 0;
boolean buttonPressed = false;
boolean doorStatus = false;
boolean doorLastStatus = false;

String availabilityBase = MQTT_CLIENTID;
String availabilitySuffix = "/availability";
String availabilityTopicStr = availabilityBase + availabilitySuffix;
const char* availabilityTopic = availabilityTopicStr.c_str();
const char* birthMessage = "online";
const char* lwtMessage = "offline";

WiFiClient espClient;
PubSubClient client(espClient);


void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (static_ip) {
    WiFi.config(ip, gateway, subnet);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(" WiFi connected - IP address: ");
  Serial.println(WiFi.localIP());
}


// Callback when MQTT message is received; calls triggerDoorAction(), passing topic and payload as parameters
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String topicToProcess = topic;
  payload[length] = '\0';
  String payloadToProcess = (char*)payload;
  triggerDoorAction(topicToProcess, payloadToProcess);
}

// Functions that check door status and publish an update when called
void publish_door_status() {
  if (!doorStatus) {
      Serial.print(door_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_door_status_topic);
      Serial.println("...");
      client.publish(mqtt_door_status_topic, "closed", true);
  }
  else {
      Serial.print(door_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_door_status_topic);
      Serial.println("...");
      client.publish(mqtt_door_status_topic, "open", true);
  }
}


// Functions that run in loop() to check each loop if door status (open/closed) has changed and call publish_doorX_status() to publish any change if so
void check_door_status() {
  if (doorStatus != doorLastStatus){
    unsigned int currentTime = millis();
    if (currentTime - door_lastSwitchTime >= debounceTime) {
      publish_door_status();
      doorLastStatus = doorStatus;
      door_lastSwitchTime = currentTime;
    }
  }

  //int currentStatusValue = digitalRead(door_statusPin);
  //if (currentStatusValue != door_lastStatusValue) {
  //  unsigned int currentTime = millis();
  //  if (currentTime - door_lastSwitchTime >= debounceTime) {
  //    publish_door_status();
  //    //door_lastStatusValue = currentStatusValue;
  //    door_lastSwitchTime = currentTime;
  //  }
  //}
}


// Function that publishes birthMessage
void publish_birth_message() {
  Serial.print("Publishing birth message \"");
  Serial.print(birthMessage);
  Serial.print("\" to ");
  Serial.print(availabilityTopic);
  Serial.println("...");
  client.publish(availabilityTopic, birthMessage, true);
}


// Function that toggles the relevant relay-connected output pin
void toggleRelay(int pin) {
  if (activeHighRelay) {
    digitalWrite(pin, HIGH);
    doorStatus = !doorStatus;
    delay(relayActiveTime);
    digitalWrite(pin, LOW);
  }
  else {
    digitalWrite(pin, LOW);
    doorStatus = !doorStatus;
    delay(relayActiveTime);
    digitalWrite(pin, HIGH);
  }
}

// Function that will debounce and trigger the relay based on wall mounted button
void doorSwitch() {
    unsigned int currentTime = millis();
    if (currentTime - door_lastButtonTime >= debounceTime) {
      toggleRelay(door_openPin);
      door_lastButtonTime = currentTime;
      buttonPressed = true;
    }
}


void triggerDoorAction(String requestedDoor, String requestedAction) {
  //int currentStatusValue = digitalRead(door_statusPin);
  if (requestedDoor == mqtt_door_action_topic && requestedAction == "OPEN") {
    Serial.print("Triggering ");
    Serial.print(door_alias);
    Serial.println(" OPEN relay!");
    toggleRelay(door_openPin);
  }
  else if (requestedDoor == mqtt_door_action_topic && requestedAction == "CLOSE" ) {
    Serial.print("Triggering ");
    Serial.print(door_alias);
    Serial.println(" CLOSE relay!");
    toggleRelay(door_closePin);
  }
  else if (requestedDoor == mqtt_door_action_topic && requestedAction == "STATE") {
    Serial.print("Publishing on-demand status update for ");
    Serial.print(door_alias);
    Serial.println("!");
    publish_birth_message();
    publish_door_status();
  }
  else { Serial.println("taking no action!");
  }
}


// Function that runs in loop() to connect/reconnect to the MQTT broker, and publish the current door statuses on connect
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientId, mqtt_username, mqtt_password, availabilityTopic, 0, true, lwtMessage)) {
      Serial.println("Connected!");
      publish_birth_message();
      // Subscribe to the action topics to listen for action messages
      Serial.print("Subscribing to ");
      Serial.print(mqtt_door_action_topic);
      Serial.println("...");
      client.subscribe(mqtt_door_action_topic);

      // Publish the current door status on connect/reconnect to ensure status is synced with whatever happened while disconnected
      publish_door_status();
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  pinMode(door_openPin, OUTPUT);
  pinMode(door_closePin, OUTPUT);
  pinMode(door_switchPin, INPUT);
  
  // Set output pins LOW with an active-high relay
  if (activeHighRelay) {
    digitalWrite(door_openPin, LOW);
    digitalWrite(door_closePin, LOW);
  }
  // Set output pins HIGH with an active-low relay
  else {
    digitalWrite(door_openPin, HIGH);
    digitalWrite(door_closePin, HIGH);
  }
  // Set input pin to use internal pullup resistor
  //pinMode(door_statusPin, INPUT_PULLUP);
  //door_lastStatusValue = digitalRead(door_statusPin);

  Serial.begin(115200);
  Serial.println("Starting Garage...");
  if (activeHighRelay) {
    Serial.println("Relay: Active-High");
  }
  else {
    Serial.println("Relay: Active-Low");
  }

  setup_wifi();
  client.setServer(mqtt_broker, 1883);
  client.setCallback(callback);
}

void loop() { 
  buttonState = digitalRead(door_switchPin);
  if (buttonState == LOW && !buttonPressed){
    doorSwitch();
  }
  else if (buttonState == HIGH) {
    buttonPressed = false;
  }

  // Connect/reconnect to the MQTT broker and listen for messages
  //if (!client.connected()) {
  //  reconnect();
  //}
  //client.loop();
  // Check door open/closed status each loop and publish changes
  check_door_status();
}
