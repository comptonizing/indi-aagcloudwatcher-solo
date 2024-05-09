/*
  This file is part of the Pollux Astro Cloudwatcher software
 
  Created by Philipp Weber
  Copyright (c) 2023 Philipp Weber
  All rights reserved.
 
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cw.h>
#include <indiweather.h>
#include <stdexcept>

std::unique_ptr<CloudwatcherSolo> solo(new CloudwatcherSolo());

static size_t WriteCB(void *contents, size_t size, size_t nmemb, void *userp) {
	((std::string *)userp)->append((char *)contents, size * nmemb);
	return size * nmemb;
}

CloudwatcherData::CloudwatcherData(const std::string &data) {
	std::stringstream s(data);
	std::string line;
	int valInt;
	char valString[256];
	while ( std::getline(s, line, '\n') ) {
		const char *str = line.c_str();
		if ( sscanf(str, "dataGMTTime=%[^\n]", valString) == 1 ) {
			date = std::string(valString);
			continue;
		}
		if ( sscanf(str, "cwinfo=%[^\n]", valString) == 1 ) {
			cwinfo = std::string(valString);
			continue;
		}
		if ( sscanf(str, "clouds=%lf", &clouds) == 1 ) {
			continue;
		}
		if ( sscanf(str, "temp=%lf", &temp) == 1 ) {
			continue;
		}
		if ( sscanf(str, "wind=%lf", &wind) == 1 ) {
			continue;
		}
		if ( sscanf(str, "gust=%lf", &gust) == 1 ) {
			continue;
		}
		if ( sscanf(str, "rain=%lf", &rain) == 1 ) {
			continue;
		}
		if ( sscanf(str, "lightmpsas=%lf", &lightmpsas) == 1 ) {
			continue;
		}
		if ( sscanf(str, "switch=%d", &valInt) == 1 ) {
			sw = static_cast<SwitchState>(valInt);
			continue;
		}
		if ( sscanf(str, "safe=%d", &valInt) == 1 ) {
			safe = valInt ? true : false;
			continue;
		}
		if ( sscanf(str, "hum=%lf", &hum) == 1 ) {
			continue;
		}
		if ( sscanf(str, "dewp=%lf", &dewp) == 1 ) {
			continue;
		}
		if ( sscanf(str, "rawir=%lf", &rawir) == 1 ) {
			continue;
		}
		if ( sscanf(str, "abspress=%lf", &abspress) == 1 ) {
			continue;
		}
		if ( sscanf(str, "relpress=%lf", &relpress) == 1 ) {
			continue;
		}
		LOGF_WARN("Did not understand value: %s", str);
	}
	if ( date == "" ) {
		throw std::runtime_error("Required field date not found");
	}
	if ( cwinfo == "" ) {
		throw std::runtime_error("Required field cwinfo not found");
	}
	if ( clouds == NAN ) {
		throw std::runtime_error("Required field clouds not found");
	}
	if ( lightmpsas == NAN ) {
		throw std::runtime_error("Required filed lightmpsas not found");
	}
	if ( temp == NAN ) {
		throw std::runtime_error("Required field temp not found");
	}
}

CloudwatcherSolo::CloudwatcherSolo() {
	setVersion(0, 1);
	setWeatherConnection(CONNECTION_NONE);
	curl_global_init(CURL_GLOBAL_DEFAULT);
}

CloudwatcherSolo::~CloudwatcherSolo() {
	curl_global_cleanup();
}

const char *CloudwatcherSolo::getDefaultName() {
	return "Cloudwatcher Solo";
}

void CloudwatcherSolo::ISGetProperties(const char *dev) {
	INDI::Weather::ISGetProperties(dev);
	defineProperty(addressTP);
}

bool CloudwatcherSolo::Connect() {
	if ( addressTP[0].getText() == nullptr || strcmp(addressTP[0].getText(), "") == 0 ) {
		LOG_ERROR("You must set the address first!");
        INDI::Weather::Disconnect();
		return false;
	}
	if ( ! updateRaw() ) {
		return false;
	}
    if ( ! updateWeather() ) {
        return false;
    }
	return true;
}

bool CloudwatcherSolo::Disconnect() {
	return true;
}

bool CloudwatcherSolo::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) {
	if (dev != nullptr && strcmp(dev, getDeviceName()) == 0) {
		if (addressTP.isNameMatch(name)) {
			addressTP.update(texts, names, n);
			addressTP.setState(IPS_OK);
			addressTP.apply();
			saveConfig(true, addressTP.getName());
			return true;
		}
	    return INDI::Weather::ISNewText(dev, name, texts, names, n);
	}
	return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

bool CloudwatcherSolo::saveConfigItems(FILE *fp) {
	INDI::Weather::saveConfigItems(fp);
	addressTP.save(fp);
	return true;
}

bool CloudwatcherSolo::readRaw() {
	char curlErrorBuff[CURL_ERROR_SIZE] = ""; // Necessary, see curl docs
	std::string buff;

	if ( addressTP[0].getText() == nullptr ) {
		LOG_ERROR("Address not defined!");
		return false;
	}

	CURL *curl = curl_easy_init();
	if ( curl == NULL ) {
		LOG_ERROR("Could not initialize curl!");
		curl_easy_cleanup(curl);
		return false;
	}

	if ( CURLE_OK != curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrorBuff) ) {
		LOG_ERROR("Could not set curl error buffer!");
		curl_easy_cleanup(curl);
		return false;
	}

	if ( CURLE_OK != curl_easy_setopt(curl, CURLOPT_URL, addressTP[0].getText()) ) {
		LOGF_ERROR("Could not use specified URL: %s",
				strlen(curlErrorBuff) ? curlErrorBuff : "Unknown error");
		curl_easy_cleanup(curl);
		return false;
	}

	if ( CURLE_OK != curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCB) ) {
		LOGF_ERROR("Could not set curl write callback: %s",
				strlen(curlErrorBuff) ? curlErrorBuff : "Unknown error");
		curl_easy_cleanup(curl);
		return false;
	}

	if ( CURLE_OK != curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buff) ) {
		LOGF_ERROR("Could not set curl data buffer: %s",
				strlen(curlErrorBuff) ? curlErrorBuff : "Unknown error");
		curl_easy_cleanup(curl);
		return false;
	}

	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	
	if ( CURLE_OK != res ) {
		LOGF_ERROR("Could not read data from Cloudwatcher: %s",
				strlen(curlErrorBuff) ? curlErrorBuff : curl_easy_strerror(res));
		return false;
	}

	try {
		m_lastData = std::make_unique<CloudwatcherData>(buff);
	} catch (const std::exception& e) {
		LOGF_ERROR("Could not decode values from device: %s", e.what());
	}

	return true;
}

bool CloudwatcherSolo::updateRaw() {
	RawTP.s = IPS_BUSY;
	IDSetText(&RawTP, nullptr);
	RawNP.s = IPS_BUSY;
	IDSetNumber(&RawNP, nullptr);
	if ( ! readRaw() ) {
		RawTP.s = IPS_ALERT;
		IDSetText(&RawTP, nullptr);
		RawNP.s = IPS_ALERT;
		IDSetNumber(&RawNP, nullptr);
		return false;
	}

	static char dateBuff[256];
	strncpy(dateBuff, m_lastData->date.c_str(), 255);
	RawT[DATE].text = dateBuff;
	static char cwinfoBuff[256];
	strncpy(cwinfoBuff, m_lastData->cwinfo.c_str(), 255);
	RawT[CWINFO].text = cwinfoBuff;
	RawTP.s = IPS_OK;
	IDSetText(&RawTP, nullptr);

	RawN[CLOUDS].value = m_lastData->clouds;
	RawN[TEMP].value = m_lastData->temp;
	RawN[WIND].value = m_lastData->wind;
	RawN[GUST].value = m_lastData->gust;
	RawN[RAIN].value = m_lastData->rain;
	RawN[LIGHTMPSAS].value = m_lastData->lightmpsas;
	RawN[SWITCH].value = m_lastData->sw;
	RawN[SAFE].value = m_lastData->safe;
	RawN[HUM].value = m_lastData->hum;
	RawN[DEWP].value = m_lastData->dewp;
	RawN[RAWIR].value = m_lastData->rawir;
	RawN[ABSPRESS].value = m_lastData->abspress;
	RawN[RELPRESS].value = m_lastData->relpress;
	RawNP.s = IPS_OK;
	IDSetNumber(&RawNP, nullptr);

	return true;
}

bool CloudwatcherSolo::initProperties() {
	INDI::Weather::initProperties();
	char address[1024] = "";
	IUGetConfigText(getDeviceName(), "CWS_ADDRESS", "ADDRESS", address, 1024);
	addressTP[0].fill("ADDRESS", "Address", address);
	addressTP.fill(getDeviceName(), "CWS_ADDRESS", "Cloudwatcher", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

	IUFillText(&RawT[DATE], "RAW_DATE", "dataGMTTime", "n/a");
	IUFillText(&RawT[CWINFO], "RAW_CWINFO", "cwinfo", "n/a");
	IUFillTextVector(&RawTP, RawT, 2, getDeviceName(), "RAW_STRING", "Raw", "Raw", IP_RO, 2, IPS_IDLE);

	IUFillNumber(&RawN[CLOUDS], "RAW_CLOUDS", "clouds", "%.6f", -100, 100, 0.000001, NAN);
	IUFillNumber(&RawN[TEMP], "RAW_TEMP", "temp", "%.6f", -100, 100, 0.000001, NAN);
	IUFillNumber(&RawN[WIND], "RAW_WIND", "wind", "%.0f", 0, 200, 1.0, NAN);
	IUFillNumber(&RawN[GUST], "RAW_GUST", "gust", "%.0f", 0, 200, 1.0, NAN);
	IUFillNumber(&RawN[RAIN], "RAW_RAIN", "rain", "%.0f", 0, 65535, 1.0, NAN);
	IUFillNumber(&RawN[LIGHTMPSAS], "RAW_LIGHTMPSAS", "lightmpsas", "%.2f", 0.0, 30.0, 0.01, NAN);
	IUFillNumber(&RawN[SWITCH], "RAW_SWITCH", "switch", "%.0f", 0, 1, 1.0, NAN);
	IUFillNumber(&RawN[SAFE], "RAW_SAFE", "safe", "%.0f", 0, 1, 1.0, NAN);
	IUFillNumber(&RawN[HUM], "RAW_HUM", "hum", "%.0f", 0., 100., 1.0, NAN);
	IUFillNumber(&RawN[DEWP], "RAW_DEWP", "dewp", "%.6f", -100., 100., 0.000001, NAN);
	IUFillNumber(&RawN[RAWIR], "RAW_IR", "ir", "%.6f", -100.0, 100.0, 0.000001, NAN);
	IUFillNumber(&RawN[ABSPRESS], "RAW_ABSPRESS", "abspress", "%.6f", 0.0, 2000.0, 0.000001, NAN);
	IUFillNumber(&RawN[RELPRESS], "RAW_RELPRESS", "relpress", "%.6f", 0.0, 2000.0, 0.000001, NAN);
	IUFillNumberVector(&RawNP, RawN, 13, getDeviceName(), "RAW_FLOAT", "Raw", "Raw", IP_RO, 2, IPS_IDLE);

	updateRaw();
	if ( m_lastData == nullptr ) {
		LOG_ERROR("Data not read yet!");
		return false;
	}

	addParameter("WEATHER_SAFE", "Safe", 1, 1, 0);
	addParameter("WEATHER_SWITCH", "Switch", 1, 1, 0);
	addParameter("WEATHER_SKYTEMP", "Sky Temperature [°C]", -100, -20, 10);
	addParameter("WEATHER_TEMP", "Temperature [°C]", -30, 50, 10);
	addParameter("WEATHER_SKY_QUALITY", "Sky Brightness [mag/arcsec^2]", 15, 23, 10);
	if ( m_lastData->wind != NAN ) {
		addParameter("WEATHER_WIND", "Wind [km/h]", 0, 40, 10);
	}
	if ( m_lastData->gust != NAN ) {
		addParameter("WEATHER_GUST", "Gust [km/h]", 0, 40, 10);
	}
	if ( m_lastData->rain != NAN ) {
		addParameter("WEATHER_RAIN", "Rain [a.u.]", 2900, 3200, 10);
	}
	if ( m_lastData->hum != NAN ) {
		addParameter("WEATHER_HUMIDITY", "Humidity [%]", 0, 100, 0);
	}
	if ( m_lastData->dewp != NAN ) {
		addParameter("WEATHER_DEWPOINT", "Dewpoint [°C]", -30, 50, 0);
	}
	if ( m_lastData->abspress != NAN ) {
		addParameter("WEATHER_ABSPRESS", "Absolute Pressure [mbar]", 500, 1500, 0);
	}
	if ( m_lastData->relpress != NAN ) {
		addParameter("WEATHER_RELPRESS", "Relative Pressure [mbar]", 500, 1500, 0);
	}

	setCriticalParameter("WEATHER_SAFE");
	setCriticalParameter("WEATHER_SKYTEMP");
	if ( m_lastData->wind != NAN ) {
		setCriticalParameter("WEATHER_WIND");
	}
	if ( m_lastData->gust != NAN ) {
		setCriticalParameter("WEATHER_GUST");
	}
	if ( m_lastData->rain != NAN ) {
		setCriticalParameter("WEATHER_RAIN");
	}

	addDebugControl();
	return true;
}

IPState CloudwatcherSolo::updateWeather() {
	if ( ! updateRaw() ) {
		return IPS_ALERT;
	}
	setParameterValue("WEATHER_SAFE", m_lastData->safe);
	setParameterValue("WEATHER_SWITCH", m_lastData->sw);
	setParameterValue("WEATHER_SKYTEMP", m_lastData->clouds);
	setParameterValue("WEATHER_TEMP", m_lastData->temp);
	setParameterValue("WEATHER_SKY_QUALITY", m_lastData->lightmpsas);
	if ( m_lastData->wind != NAN ) {
		setParameterValue("WEATHER_WIND", m_lastData->wind);
	}
	if ( m_lastData->gust != NAN ) {
		setParameterValue("WEATHER_GUST", m_lastData->gust);
	}
	if ( m_lastData->rain != NAN ) {
		setParameterValue("WEATHER_RAIN", m_lastData->rain);
	}
	if ( m_lastData->hum != NAN ) {
		setParameterValue("WEATHER_HUMIDITY", m_lastData->hum);
	}
	if ( m_lastData->dewp != NAN ) {
		setParameterValue("WEATHER_DEWPOINT", m_lastData->dewp);
	}
	if ( m_lastData->abspress != NAN ) {
		setParameterValue("WEATHER_ABSPRESS", m_lastData->abspress);
	}
	if ( m_lastData->relpress != NAN ) {
		setParameterValue("WEATHER_RELPRESS", m_lastData->relpress);
	}
	return IPS_OK;
}

bool CloudwatcherSolo::updateProperties() {
	INDI::Weather::updateProperties();
	if ( isConnected() ) {
		defineProperty(&RawTP);
		defineProperty(&RawNP);
	} else {
		deleteProperty(RawTP.name);
		deleteProperty(RawNP.name);
	}
	return true;
}
