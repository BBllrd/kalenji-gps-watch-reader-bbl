#include "OnMove500HR.h"
#include <cstring>
#include <iomanip>
#include <set>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>

#include <dirent.h>
#include <sys/stat.h>

#ifdef DEBUG
#define DEBUG_CMD(x) x;
#else
#define DEBUG_CMD(x) ;
#endif

namespace device
{
	REGISTER_DEVICE(OnMove500HR);

	int OnMove500HR::bytesToInt2(unsigned char b0, unsigned char b1)
	{
		int Int = b0 | ( (int)b1 << 8 );
		return Int;
	}

	int OnMove500HR::bytesToInt4(unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3)
	{
		int Int = b0 | ( (int)b1 << 8 ) | ( (int)b2 << 16 ) | ( (int)b3 << 24 );
		return Int;
	}

	unsigned char* OnMove500HR::readAllBytes(const std::string& filename, size_t& size)
	{
		std::ifstream fl(filename.c_str());
		fl.seekg( 0, std::ios::end );
		size_t len = fl.tellg();
		auto buffer = new char[len];
		fl.seekg(0, std::ios::beg);
		fl.read(buffer, len);
		fl.close();
		size = len;
		return (unsigned char*)buffer;
	}

	bool OnMove500HR::fileExists(const std::string& filename)
	{
		struct stat fileInfo;
		return stat(filename.c_str(), &fileInfo) == 0;
	}

	void OnMove500HR::dump(unsigned char *data, int length)
	{
		DEBUG_CMD(std::cout << std::hex);
		for(int i = 0; i < length; ++i)
		{
			DEBUG_CMD(std::cout << std::setw(2) << std::setfill('0') << (int) data[i] << " ");
		}
		DEBUG_CMD(std::cout << std::endl);
	}

	void OnMove500HR::init(const DeviceId& deviceId)
	{
		//check if getPath() is a valid path
		DIR* folder = opendir(getPath().c_str());
		if (folder == nullptr)
		{
			std::cout<< "Error: path '" << getPath() << "' does not exist (check option -p <path> on command line or line path=<path> in configuration file)." << std::endl;
			throw std::exception();
		}
		closedir(folder);
	}

	void OnMove500HR::release()
	{
		DEBUG_CMD(std::cout << "OnMove500HR: Release (nothing to do)" << std::endl);
	}

	std::string OnMove500HR::getPath()
	{
		return _configuration["path"];
	}

	void OnMove500HR::getSessionsList(SessionsMap *oSessions)
	{
		LOG_VERBOSE("OnMove500HR: Retrieve sessions list from '" << getPath() << "'");

		DIR* folder = nullptr;
		struct dirent* file = nullptr;
		folder = opendir(getPath().c_str());
		if (folder == nullptr)
		{
			std::cerr << "Couldn't open dir " << getPath() << std::endl;
			throw std::exception();
		}

		std::set<std::string> filenamesPrefix;

		while ((file = readdir(folder)) != nullptr)
		{
			std::string fn = std::string(file->d_name);
			if(strstr(file->d_name,".OMD") || strstr(file->d_name,".OMH"))
			{
				std::string fileprefix = fn.substr(0,fn.length() - 4);
				std::string filepathprefix = getPath() + std::string("/") + fileprefix;
				//check if both files (header and data) exists
				if(fileExists(filepathprefix + ".OMD") && fileExists(filepathprefix + ".OMH"))
				{
					filenamesPrefix.insert(fileprefix);
				}
				else
				{
					std::cout << "Discarding " << fileprefix << std::endl;
				}
			}
		}

		closedir(folder);

		int i = 0;
		std::set<std::string>::iterator iter;
		for (iter=filenamesPrefix.begin(); iter!=filenamesPrefix.end(); ++iter)
		{
			std::string fileprefix = *iter;

			DEBUG_CMD(std::cout << "Decode summary of session " << fileprefix << std::endl);
			// Decoding of basic info about the session
			std::vector<char> _sessionId(fileprefix.begin(), fileprefix.end());
			SessionId id = SessionId(_sessionId);
			uint32_t num = i++; //Just increment by one each time

			tm time;
			memset(&time, 0, sizeof(time));

			double duration = 0;
			uint32_t distance = 0;
			uint32_t nbLaps = 999;

			Session mySession(id, num, time, 0, duration, distance, nbLaps);

			// Properly fill necessary session info (duration, distance, nbLaps)
			std::string ghtFilename = getPath() + std::string("/") + fileprefix + std::string(".OMH");
			size_t size = -1;
			unsigned char* buffer = readAllBytes(ghtFilename, size);
			// TODO: if(size != 60) error !
			parseOMHFile(buffer, &mySession);
			delete buffer;

			oSessions->insert(SessionsMapElement(id, mySession));
		}
	}

	void OnMove500HR::getSessionsDetails(SessionsMap *oSessions)
	{
		for(auto& sessionPair : *oSessions)
		{
			Session* session = &(sessionPair.second);
			SessionId sessionId = sessionPair.second.getId();
			std::string filenamePrefix(sessionId.begin(),sessionId.end());
			std::cout << "Retrieve session " << filenamePrefix << std::endl;

			filenamePrefix = getPath() + std::string("/") + filenamePrefix;

			unsigned char* buffer;
			size_t size = -1;

			std::string omdFilename = filenamePrefix + std::string(".OMD");
			buffer = readAllBytes(omdFilename,size);
			parseOMDFile(buffer, size, session);
			delete buffer;
		}
	}

	void OnMove500HR::dumpInt2(std::ostream &oStream, unsigned int iInt)
	{
		oStream << (char)(iInt & 0xFF) << (char)((iInt & 0xFF00) >> 8);
	}

	void OnMove500HR::dumpInt4(std::ostream &oStream, unsigned int iInt)
	{
		oStream << (char)(iInt & 0xFF) << (char)((iInt & 0xFF00) >> 8) << (char)((iInt & 0xFF0000) >> 16) << (char)((iInt & 0xFF000000) >> 24);
	}

	void OnMove500HR::dumpString(std::ostream &oStream, const std::string &iString, size_t iLength)
	{
		size_t toCopy = iString.length();
		if(iLength <= toCopy) toCopy = iLength - 1;
		oStream.write(iString.c_str(), toCopy);
		for(size_t i = toCopy; i < iLength; ++i)
		{
			oStream.put('\0');
		}
	}

	void OnMove500HR::exportSession(const Session *iSession)
	{
		std::cerr << "Unsupported export session for OnMove500HR" << std::endl;
	}

	void OnMove500HR::parseOMHFile(const unsigned char* bytes, Session* session)
	{
		// OMH is 3 lines of 20 bites :
		//line | 00 | 01 | 02 | 03 | 04 | 05 | 06  |  07  |  08  | 09 | 10 | 11 | 12 | 13 | 14 | 15 | 16  |  17  |  18  | 19 | 
		// 1   | Distance          | Duration| Avg Speed  |  Max Speed| Energy  |AvHR|MxHR| Y  | M  | D   |  H   |  M   |FNum|
		// 2   | NbP     | Ascent  | Descent |  ??????                                                             |FNum|0xFA|
		// 3   |                          ?????????                   |MiHR|MxHR| ???????????                      |FNum|0xF0|

		uint32_t distance = bytesToInt4(bytes[0], bytes[1], bytes[2], bytes[3]);
		uint32_t duration = bytesToInt2(bytes[4], bytes[5]);
		uint32_t avgSpeed = bytesToInt2(bytes[6], bytes[7]);
		uint32_t maxSpeed = bytesToInt2(bytes[8], bytes[9]);
		uint32_t energy = bytesToInt2(bytes[10], bytes[11]);
		uint32_t avgHeartRate = static_cast<uint32_t>(bytes[12]);
		uint32_t maxHeartRate = static_cast<uint32_t>(bytes[13]);

		uint32_t year = static_cast<uint32_t>(bytes[14]);
		uint32_t month = static_cast<uint32_t>(bytes[15]);
		uint32_t day = static_cast<uint32_t>(bytes[16]);
		uint32_t hour = static_cast<uint32_t>(bytes[17]);
		uint32_t minute = static_cast<uint32_t>(bytes[18]);
		//uint32_t fileNum = static_cast<uint32_t>(bytes[19]); // 1 more than filename
		uint32_t nb_points = bytesToInt2(bytes[20], bytes[21]);
		uint32_t ascent = bytesToInt2(bytes[24], bytes[25]);
		uint32_t descent = bytesToInt2(bytes[26], bytes[27]);
		//uint32_t UserHRmin = static_cast<uint32_t>(bytes[50]); // Min HR user setting
		//uint32_t UserHRmax = static_cast<uint32_t>(bytes[51]); // Max HR user setting

		tm time;
		memset(&time, 0, sizeof(time));
		time.tm_year  = 100 + year;// In tm, year is year since 1900. GPS returns year since 2000
		time.tm_mon   = month - 1;// In tm, month is between 0 and 11.
		time.tm_mday  = day;
		time.tm_hour  = hour;
		time.tm_min   = minute;
		time.tm_isdst = -1;
		session->setTime(time);
		session->setDistance(distance);
		session->setDuration(duration);
		session->setAvgSpeed(avgSpeed);
		session->setMaxSpeed(maxSpeed);
		session->setCalories(energy);
		session->setAvgHr(avgHeartRate);
		session->setMaxHr(maxHeartRate);
		session->setNbPoints(nb_points);
		session->setAscent(ascent);
		session->setDescent(descent);
	}

	void OnMove500HR::parseOMDFile(const unsigned char* bytes, int length, Session *session)
	{
		const unsigned char* chunk;
		int numligne = 0;
		time_t startTime = session->getTime();
		// Data structure are organised with 3 lines of 20 bites
		// The last block can have only 2 lines if there is only one point
		// line | 00 | 01 | 02 | 03 | 04 | 05 | 06  |  07  |  08  | 09 | 10 | 11 | 12 | 13 | 14 | 15 | 16  |  17  |  18  | 19 | 
		// 1    | Latitude          | Longitude            |  Distance           | Time    |Fiab| Altitude |  ??  |  00  | F1 |
		// 2    | Latitude          | Longitude            |  Distance           | Time    |Fiab| Altitude |  ??  |  00  | F1 |
		// 3    | Time1   | Speed1  | Energy1 | HR1 | BOL1 | EOT1 | 00 | Time2   | Speed2  | Energy2 | HR2 | BOL2 | EOT2 | F2 |
		for(int i = 0; i < (length+20); i += 20)
		{
			chunk = &bytes[i];
			// position metadatas
			numligne++;
			int endofline = chunk[19];
			int endofnextline = chunk[39];
			int offsetmeta = 0;
			if(numligne % 3 == 1 && endofline == 0xF1 && endofnextline == 0xF1) {
				offsetmeta = 40; // Point line 1 of 2
			} else if(numligne % 3 == 1 && endofline == 0xF1 && endofnextline == 0xF2) {
				offsetmeta = 20; // Point line 1 of 1
			} else if(numligne % 3 == 2 && endofline == 0xF1 && endofnextline == 0xF1) {
				offsetmeta = 30; // Point line 2 of 2
			} else if(numligne % 3 == 2 && endofline == 0xF2) {
				continue; // Metadata for only 1 point // skip metadata lines wich are red with point lines
			} else if(numligne % 3 == 0 && endofline == 0xF2) {
				continue; // Metadata for 2 points // skip metadata lines wich are red with point lines
			} else {
				continue; // not a valid line
			}
			
			double latitude = ((double) bytesToInt4(chunk[0], chunk[1], chunk[2], chunk[3])) / 1000000.;
			double longitude = ((double) bytesToInt4(chunk[4], chunk[5], chunk[6], chunk[7])) / 1000000.;
			uint32_t distance = bytesToInt4(chunk[8], chunk[9], chunk[10], chunk[11]);
			uint32_t time = bytesToInt2(chunk[12], chunk[13]);
			uint16_t fiability = chunk[14]; // almost always 3
			int16_t altitude = bytesToInt2(chunk[15], chunk[16]);
			// chunk[17] seems to be aways 0x00 
			// chunk[18] seems to be aways 0x00 
			// chunk[19] is always 0xF1 for a point line
			// Metadata of first point are always are in third line
			//uint32_t time_meta = bytesToInt2(chunk[offsetmeta + 0], chunk[offsetmeta + 1]);
			double speed = ((double) bytesToInt2(chunk[offsetmeta + 2], chunk[offsetmeta + 3])) / 100.;
			// uint32_t energy = bytesToInt2(chunk[offsetmeta + 4], chunk[offsetmeta + 5]); // point object does not have Energy property
			uint32_t hr = chunk[offsetmeta + 6];
			//uint32_t newlap = chunk[offsetmeta + 7];
			//uint32_t endoftrack = chunk[offsetmeta + 8];
			auto p = new Point(latitude, longitude, altitude, speed, startTime + time, 0, hr, fiability);
			p->setDistance(distance);
			session->addPoint(p);

		}	
	}
}
