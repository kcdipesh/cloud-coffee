#include "CloudCoffeeMaker.h"

CloudCoffeeMaker::CloudCoffeeMaker(const uint8_t * macAddress, const char * feedId, const char * apiKey) 
	: _cloudIP(64, 94, 18, 120), _lastUpdateTime(0) {
	_macAddress = new uint8_t[6];

	for (int i = 0; i < 6; i++) {
		_macAddress[i] = macAddress[i];
	}

	for (int i = 0; i < 3; i++) {
		strcpy(_trayOwner[i], "^");
	}

	//feedid and apikey
	_feedId = new char[strlen(feedId) + 1];
	strcpy(_feedId, feedId);

	_apiKey = new char[strlen(apiKey) + 1];
	strcpy(_apiKey, apiKey);

	//endpoints
	strcpy(_endpointFeedId[0], EP1_FEED_ID);
	strcpy(_endpointFeedId[1], EP2_FEED_ID);
	strcpy(_endpointFeedId[2], EP3_FEED_ID);
	strcpy(_endpointFeedId[3], EP4_FEED_ID);

	strcpy(_endpointApiKey[0], EP1_API_KEY);
	strcpy(_endpointApiKey[1], EP2_API_KEY);
	strcpy(_endpointApiKey[2], EP3_API_KEY);
	strcpy(_endpointApiKey[3], EP4_API_KEY);
}

void CloudCoffeeMaker::begin() {
	CoffeeMakerHardware::begin();

	//initialize ethernet.
	while (!Ethernet.begin(_macAddress)) {
		Serial.println("DHCP failed");
		delay(1000);
	}

	Serial.println("DHCP Successful.");


	//connect to cloud/Xively
	while (!_ethernetClient.connect(_cloudIP, 8081)) {
		Serial.println("Connection failed");
		delay(1000);
	}

	Serial.println("Cloud connection successful");

	//subscribe to endpoints.
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 3; j++) {
			if (_subscribeEndpoint(i)) {
				Serial.print("Subscribed to (");
				Serial.print(i);
				Serial.println(")");
				break;
			}
		}
	}
}

void CloudCoffeeMaker::maintain() {
	CoffeeMakerHardware::maintain();

	Ethernet.maintain();

	for (int i = 0; i < 3; i++) {
		if (getTraySlotStatus(i) == 0) {
			strcpy(_trayOwner[i], "^");
		}
	}

	//if finished
	if (getCoffeeMakerStatus() == 2) { //finished.
		//dequeue.
		CoffeeOrder c = coffeeOrderList.shift();
		//inform user.
		_informEndpointFinished(c.feedId, c.apiKey, c.reqId, c.tray);
		setCoffeeMakerToAvailableSlot();
	}

	//UPDATE XIVELY EVERY 5S
	unsigned long m = millis();
	if (_lastUpdateTime + CONSTANT_UPDATE_TIME_MS < m) {
		_updateServer();
		Serial.println("Server updated");
		_lastUpdateTime = m;
	}

	//read ethernet if there are available bytes
	_readEthernetIfAvailable();
}

//server communication
boolean CloudCoffeeMaker::_sendToServer(const char * buf) {
	_ethernetClient.print(buf);

	unsigned long startTime = millis();
	char res_buf[500];
	unsigned int cnt = 0;
	boolean res_success = false;
	boolean wait_for_n = false;

	//wait for server response
	while (startTime + 10000 > millis()) {
		if (_ethernetClient.available()) {
			res_buf[cnt] = _ethernetClient.read();

			if (wait_for_n && res_buf[cnt] == 10) {

				res_buf[++cnt] = '\0';
				res_success = true;
				break;
			}

			if (res_buf[cnt] == 13) {
				wait_for_n = true;
			}
			cnt++;
		}
	}

	return res_success;
}

boolean CloudCoffeeMaker::_updateServer() {
	char buf[800];
	char sBuf[15];
	char nBuf[2];
	nBuf[1] = '\0';

	strcpy(buf, "{\"method\":\"put\",\"resource\":\"/feeds/");
	strcat(buf, _feedId);
	strcat(buf, "\",\"headers\":{\"X-ApiKey\":\"");
	strcat(buf, _apiKey);
	strcat(buf, "\"},\"body\":{\"version\":\"1.0.0\",\"datastreams\":[");
	strcat(buf, "{\"id\":\"coffee_tsp\",\"current_value\":");
	snprintf(sBuf, 15, "%d", getCoffeeTspRemaining());
	strcat(buf, sBuf);
	strcat(buf, "},{\"id\":\"creamer_tsp\",\"current_value\":");
	snprintf(sBuf, 15, "%d", getCreamTspRemaining());
	strcat(buf, sBuf);
	strcat(buf, "},{\"id\":\"sugar_tsp\",\"current_value\":");
	snprintf(sBuf, 15, "%d", getSugarTspRemaining());
	strcat(buf, sBuf);
	strcat(buf, "},{\"id\":\"water_cups\",\"current_value\":");
	snprintf(sBuf, 15, "%d", getWaterCupRemaining());
	strcat(buf, sBuf);
	strcat(buf, "},{\"id\":\"error_code\",\"current_value\":");
	snprintf(sBuf, 15, "%d", getErrorCode());
	strcat(buf, sBuf);
	for (int i = 0; i < 3; i++) {
		nBuf[0] = '0' + i;
		strcat(buf, "},{\"id\":\"tray_");
		strcat(buf, nBuf);
		strcat(buf, "_owner\",\"current_value\":\"");
		strcat(buf, _trayOwner[i]);
		strcat(buf, "\"},{\"id\":\"tray_");
		strcat(buf, nBuf);
		strcat(buf, "_status\",\"current_value\":");
		snprintf(sBuf, 15, "%d", getTraySlotStatus(i));
		strcat(buf, sBuf);
	}
	strcat(buf, "}]}}");

	boolean res = false;
	//try 3 times
	for (int i = 0; i < 3; i++) {
		res = _sendToServer(buf);
		if (res)
			break;
		else delay(1000);
	}

	return res;
	//printToServer(buf);
}

boolean CloudCoffeeMaker::_sendErrorCode(const char * feedId, const char * apiKey, unsigned long req_id, int error) {
	char buf[300];
	char sBuf[30];
	char nBuf[2];
	nBuf[1] = '\0';

	strcpy(buf, "{\"method\":\"put\",\"resource\":\"/feeds/");
	strcat(buf, feedId);
	strcat(buf, "\",\"headers\":{\"X-ApiKey\":\"");
	strcat(buf, apiKey);
	strcat(buf, "\"},\"body\":{\"version\":\"1.0.0\",\"datastreams\":[");
	strcat(buf, "{\"id\":\"resp_id\",\"current_value\":");
	snprintf(sBuf, 30, "%lu", req_id);
	strcat(buf, sBuf);
	strcat(buf, "},{\"id\":\"resp_code\",\"current_value\":");
	snprintf(sBuf, 15, "%d", error);
	strcat(buf, sBuf);
	strcat(buf, "}]}}");

	boolean res = false;
	Serial.println(buf);
	//try 3 times
	for (int i = 0; i < 3; i++) {
		res = _sendToServer(buf);
		if (res)
			break;
		else delay(1000);
	}

	return res;
	//printToServer(buf);
}

boolean CloudCoffeeMaker::_informEndpointFinished(const char * feedId, const char * apiKey, unsigned long req_id, int tray) {
	char buf[300];
	char sBuf[30];
	char nBuf[2];
	nBuf[1] = '\0';

	strcpy(buf, "{\"method\":\"put\",\"resource\":\"/feeds/");
	strcat(buf, feedId);
	strcat(buf, "\",\"headers\":{\"X-ApiKey\":\"");
	strcat(buf, apiKey);
	strcat(buf, "\"},\"body\":{\"version\":\"1.0.0\",\"datastreams\":[");
	strcat(buf, "{\"id\":\"resp_finished\",\"current_value\":");
	snprintf(sBuf, 30, "%lu", req_id);
	strcat(buf, sBuf);
	strcat(buf, "},{\"id\":\"resp_code\",\"current_value\":");
	snprintf(sBuf, 30, "%d", tray);
	strcat(buf, sBuf);
	strcat(buf, "}]}}");

	boolean res = false;
	Serial.println(buf);
	//try 3 times
	for (int i = 0; i < 3; i++) {
		res = _sendToServer(buf);
		if (res)
			break;
		else delay(1000);
	}

	return res;
	//printToServer(buf);
}

boolean CloudCoffeeMaker::_subscribeEndpoint(int endpoint) {
	// initialize jsonBuffer
	StaticJsonBuffer<200> jsonBuffer;

	// create subscription request to feed
	JsonObject& root = jsonBuffer.createObject();
	root["method"] = "subscribe";
	char resource[50];
	strcpy(resource, "/feeds/");
	strcat(resource, _endpointFeedId[endpoint]);
	strcat(resource, "/datastreams/req_id");
	root["resource"] = resource;

	JsonObject& headers = root.createNestedObject("headers");
	headers["X-ApiKey"] = _endpointApiKey[endpoint];

	Serial.println();
	root.printTo(Serial);

	root.printTo(_ethernetClient);

	unsigned long startTime = millis();
	char res_buf[500];
	unsigned int cnt = 0;
	boolean res_success = false;
	boolean wait_for_n = false;

	//wait for server response
	while (startTime + 10000 > millis()) {
		if (_ethernetClient.available()) {
			res_buf[cnt] = _ethernetClient.read();

			if (wait_for_n && res_buf[cnt] == 10) { // \n
													//add '\0'
				res_buf[++cnt] = '\0';
				res_success = true;
				break;
			}

			if (res_buf[cnt] == 13) { // \r
				wait_for_n = true;
			}
			cnt++;
		}
	}

	return res_success;
}

void CloudCoffeeMaker::_readEthernetIfAvailable() {
	if (_ethernetClient.available() > 0) {
		unsigned long startTime = millis();
		char res_buf[500];
		unsigned int cnt = 0;
		boolean res_success = false;
		boolean wait_for_n = false;

		//wait for server response
		//shouldnt take more than 10 secs
		while (startTime + 10000 > millis()) {
			if (_ethernetClient.available()) {
				res_buf[cnt] = _ethernetClient.read();

				if (wait_for_n && res_buf[cnt] == 10) { // \n
														//add '\0'
					res_buf[++cnt] = '\0';
					res_success = true;
					break;
				}

				if (res_buf[cnt] == 13) { // \r
					wait_for_n = true;
				}
				cnt++;
			}
		}

		if (res_success) {
			//Serial.println(res_buf);
			_processAvailableData(res_buf);
			//_prevHeartBeat = millis();
		}
	}
}

void CloudCoffeeMaker::_processAvailableData(char * buf) {
	//check if data has req_id
	StaticJsonBuffer<200> jsonBuffer;

	//decode response
	JsonObject& response = jsonBuffer.parseObject(buf);

	const char * id = response["body"]["id"];
	if (!strcmp(id, "req_id")) {
		time_t req_id = strtoul(response["body"]["current_value"].asString(), NULL, 10);
		//get feed id
		const char * resource = response["resource"].asString();
		char restok[30];
		char * pch;
		strncpy(restok, resource, 30);
		pch = strtok(restok, "/");
		pch = strtok(NULL, "/");

		Serial.print("REQ: ");
		Serial.println(pch);

		//pch contains feedid.
		//get endpoint number
		int endpt = 0;
		for (endpt = 0; endpt < 4; endpt++) {
			if (!strcmp(pch, _endpointFeedId[endpt])) {
				break;
			}
		}

		//check status of coffeemaker
		int traySlot = getAvailableTraySlot();
		if (getErrorCode() == None) { // == None
			//no error, proceed with coffee making
			if (traySlot >= 0) { //>=
				//retrieve ingredients.
				//assert same request id.
				//eliminate assertion in next implementation.
				CoffeeOrder coffeeOrder(_endpointFeedId[endpt], _endpointApiKey[endpt], traySlot);
				while (!_retrieveIngredients(_endpointFeedId[endpt], _endpointApiKey[endpt], coffeeOrder)) {
					Serial.println("Failed to retrieve ingredients.");
					delay(1000);
				}
				
				coffeeOrder.reqId = req_id;
				strcpy(_trayOwner[traySlot], coffeeOrder.username);
				coffeeOrderList.add(coffeeOrder);

				_sendErrorCode(_endpointFeedId[endpt], _endpointApiKey[endpt], req_id, 0); //no error, processing.
			}
			else {
				//send full tray error
				_sendErrorCode(_endpointFeedId[endpt], _endpointApiKey[endpt], req_id, 3); //3 == full.
			}
		}
		else {
			//send error code
			_sendErrorCode(_endpointFeedId[endpt], _endpointApiKey[endpt], req_id, getErrorCode());
		}
	}
}

boolean CloudCoffeeMaker::_retrieveIngredients(const char * feedId, const char * apiKey, CoffeeOrder& order) {
	StaticJsonBuffer<400> jsonBuffer;
	char ds[] = "\"datastreams\":";

	char resource[50] = "/feeds/";
	strcat(resource, feedId);
	JsonObject& request = jsonBuffer.createObject();
	request["method"] = "get";
	request["resource"] = resource;
	JsonObject& params = request.createNestedObject("params");
	params["datastreams"] = "req_coffee,req_creamer,req_sugar,username";

	JsonObject& reqHead = request.createNestedObject("headers");
	reqHead["X-ApiKey"] = apiKey;

	unsigned long startTime = millis();
	char res_buf[500];
	unsigned int cnt = 15;
	boolean datastream_start = false;
	boolean datastream_end = false;
	boolean res_success = false;
	boolean wait_for_n = false;

	_ethernetClient.flush();
	request.printTo(_ethernetClient);

	unsigned int tc = 0;

	strcpy(res_buf, "{\"datastreams\":");
	//wait for server response
	//shouldnt take more than 10 secs
	while (startTime + 10000 > millis()) {
		if (_ethernetClient.available()) {
			char t = _ethernetClient.read();
			//Serial.write(t);
			//retrieve only datastream portion
			if (!datastream_start) {
				if (ds[tc] == t) {
					tc++;
					if (tc == 14) {
						datastream_start = true;
					}
				}
				else tc = 0;
			}
			else if (!datastream_end) {
				res_buf[cnt] = t;
				if (t == ']') {
					datastream_end = true;
					res_buf[++cnt] = '}';
					res_buf[++cnt] = '\0';
				}
				cnt++;
			}

			if (wait_for_n && t == 10) { // \n
				res_success = true;
				res_buf[++cnt] = '\0';
				break;
			}

			if (t == 13) { // \r
				wait_for_n = true;
			}

		}
	}

	if (!res_success) return false;
	
	jsonBuffer = StaticJsonBuffer<400>();

	JsonObject& response = jsonBuffer.parseObject(res_buf);

	//set status
	JsonArray& datastreams = response["datastreams"];

	//req_coffee,req_creamer,req_sugar,username

	for (int i = 0; i < datastreams.size(); i++) {
		if (!strcmp(datastreams[i]["id"], "req_coffee")) {
			order.coffeeTsp = strtoul(datastreams[i]["current_value"].asString(), NULL, 10);
		}
		else if (!strcmp(datastreams[i]["id"], "req_sugar")) {
			order.sugarTsp = strtoul(datastreams[i]["current_value"].asString(), NULL, 10);
		}
		else if (!strcmp(datastreams[i]["id"], "req_creamer")) {
			order.creamTsp = strtoul(datastreams[i]["current_value"].asString(), NULL, 10);
		}
		else if (!strcmp(datastreams[i]["id"], "username")) {
			//_requestTime = strtoul(datastreams[i]["current_value"].asString(), NULL, 10);
			strcpy(order.username, datastreams[i]["current_value"].asString());
		}
	}

	return true;
}

CloudCoffeeMaker::~CloudCoffeeMaker() {
	delete[] _macAddress;
	delete[] _feedId;
	delete[] _apiKey;
}