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

#include <memory>

#include <curl/curl.h>

#include <indipropertytext.h>
#include <indiweather.h>

enum SwitchState {
	CLOSED = 0,
	OPEN = 1
};

class CloudwatcherData {
	public:
		const char *getDeviceName() {
			return "Decoder";
		}
		CloudwatcherData(const std::string &data);
		std::string date = "";
		std::string cwinfo = "";
		SwitchState sw = CLOSED;
		bool safe = false;
		double clouds = NAN;
		double temp = NAN;
		double lightmpsas = NAN;
		double rawir = NAN;
		double wind = NAN;
		double gust = NAN;
		double rain = NAN;
		double hum = NAN;
		double dewp = NAN;
		double abspress = NAN;
		double relpress = NAN;
};

class CloudwatcherSolo : INDI::Weather {
	public:
		CloudwatcherSolo();
		virtual ~CloudwatcherSolo();

		bool Connect() override;
		bool Disconnect() override;
		const char *getDefaultName() override;

		virtual bool initProperties() override;
		virtual void ISGetProperties(const char *dev) override;
		virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

		enum {
			DATE = 0,
			CWINFO = 1
		} RAW_STRING;

		enum {
			CLOUDS = 0,
			TEMP = 1,
			WIND = 2,
			GUST = 3,
			RAIN = 4,
			LIGHTMPSAS = 5,
			SWITCH = 6,
			SAFE = 7,
			HUM = 8,
			DEWP = 9,
			RAWIR = 10,
			ABSPRESS = 11,
			RELPRESS = 12
		} RAW_FLOAT;

	protected:
		virtual IPState updateWeather() override;
		virtual bool saveConfigItems(FILE *fp) override;
		virtual bool updateProperties() override;

	private:
		std::unique_ptr<CloudwatcherData> m_lastData = nullptr;

		INDI::PropertyText addressTP{1};

		IText RawT[2];
		ITextVectorProperty RawTP;
		INumber RawN[13];
		INumberVectorProperty RawNP;

		bool readRaw();
		bool updateRaw();
		void setupRaw();
};
