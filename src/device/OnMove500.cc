#include "OnMove500.h"
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
	REGISTER_DEVICE(OnMove500);

	int OnMove500::bytesToInt2(unsigned char b0, unsigned char b1)
	{
		int Int = b0 | ( (int)b1 << 8 );
		return Int;
	}

	int OnMove500::bytesToInt4(unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3)
	{
		int Int = b0 | ( (int)b1 << 8 ) | ( (int)b2 << 16 ) | ( (int)b3 << 24 );
		return Int;
	}

	unsigned char* OnMove500::readAllBytes(const std::string& filename, size_t& size)
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

	bool OnMove500::fileExists(const std::string& filename)
	{
		struct stat fileInfo;
		return stat(filename.c_str(), &fileInfo) == 0;
	}

	void OnMove500::dump(unsigned char *data, int length)
	{
		DEBUG_CMD(std::cout << std::hex);
		for(int i = 0; i < length; ++i)
		{
			DEBUG_CMD(std::cout << std::setw(2) << std::setfill('0') << (int) data[i] << " ");
		}
		DEBUG_CMD(std::cout << std::endl);
	}

	void OnMove500::init(const DeviceId& deviceId)
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

	void OnMove500::release()
	{
		DEBUG_CMD(std::cout << "OnMove500: Release (nothing to do)" << std::endl);
	}

	std::string OnMove500::getPath()
	{
		return _configuration["path"];
	}

	void OnMove500::getSessionsList(SessionsMap *oSessions)
	{
		LOG_VERBOSE("OnMove500: Retrieve sessions list from '" << getPath() << "'");

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

	void OnMove500::getSessionsDetails(SessionsMap *oSessions)
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

	void OnMove500::dumpInt2(std::ostream &oStream, unsigned int iInt)
	{
		oStream << (char)(iInt & 0xFF) << (char)((iInt & 0xFF00) >> 8);
	}

	void OnMove500::dumpInt4(std::ostream &oStream, unsigned int iInt)
	{
		oStream << (char)(iInt & 0xFF) << (char)((iInt & 0xFF00) >> 8) << (char)((iInt & 0xFF0000) >> 16) << (char)((iInt & 0xFF000000) >> 24);
	}

	void OnMove500::dumpString(std::ostream &oStream, const std::string &iString, size_t iLength)
	{
		size_t toCopy = iString.length();
		if(iLength <= toCopy) toCopy = iLength - 1;
		oStream.write(iString.c_str(), toCopy);
		for(size_t i = toCopy; i < iLength; ++i)
		{
			oStream.put('\0');
		}
	}

	void OnMove500::exportSession(const Session *iSession)
	{
		std::cerr << "Unsupported export session for OnMove500" << std::endl;
	}

	void OnMove500::parseOMHFile(const unsigned char* bytes, Session* session)
	{
		uint32_t distance = bytesToInt4(bytes[0], bytes[1], bytes[2], bytes[3]); // m
		uint32_t duration = bytesToInt2(bytes[4], bytes[5]); // sec
		uint32_t avgSpeed = bytesToInt2(bytes[6], bytes[7]); // dm/h
		uint32_t maxSpeed = bytesToInt2(bytes[8], bytes[9]); // dm/h
		uint32_t energy = bytesToInt2(bytes[10], bytes[11]); // kCal
		uint32_t avgHeartRate = static_cast<uint32_t>(bytes[12]); //bpm
		uint32_t maxHeartRate = static_cast<uint32_t>(bytes[13]); // bpm
		uint32_t year = static_cast<uint32_t>(bytes[14]); // int since 2000
		uint32_t month = static_cast<uint32_t>(bytes[15]); // int
		uint32_t day = static_cast<uint32_t>(bytes[16]); // int
		uint32_t hour = static_cast<uint32_t>(bytes[17]); // int
		uint32_t minute = static_cast<uint32_t>(bytes[18]); // int
		uint32_t fileNum = static_cast<uint32_t>(bytes[19]); // int +1
		uint32_t nbPoints = bytesToInt2(bytes[20], bytes[21]); // int
		uint32_t ascend = bytesToInt4(bytes[24], bytes[25], bytes[26], bytes[27]); // m
		uint32_t descend = bytesToInt4(bytes[28], bytes[29], bytes[30], bytes[31]); // m
		//uint32_t sport = static_cast<uint32_t>(bytes[32]); // 0=run, 1=bike, 2=walk, 3=trek, 4=other
		//uint32_t minHeartRateRange = static_cast<uint32_t>(bytes[50]); // bpm
		//uint32_t maxHeartRateRange = static_cast<uint32_t>(bytes[51]); // bpm


		tm time;
		memset(&time, 0, sizeof(time));
		time.tm_year  = 100 + year;// In tm, year is year since 1900. GPS returns year since 2000
		time.tm_mon   = month - 1;// In tm, month is between 0 and 11.
		time.tm_mday  = day;
		time.tm_hour  = hour;
		time.tm_min   = minute;
		time.tm_isdst = -1;
		session->setTime(time);
		session->setNum(fileNum);
		session->setNbPoints(nbPoints);
		session->setDistance(distance);
		session->setDuration(duration);
		session->setAvgSpeed(avgSpeed);
		session->setMaxSpeed(maxSpeed);
		session->setCalories(energy);
		session->setAvgHr(avgHeartRate);
		session->setMaxHr(maxHeartRate);
		session->setAscent(ascend);
		session->setDescent(descend);
	}

	void OnMove500::parseOMDFile(const unsigned char* bytes, int length, Session *session)
	{
		const unsigned char* chunk;
		int numPoints = -1;
		int twoPoints = 1;
		int metaoffset = 20;
		time_t startTime = session->getTime();
		for(int i = 0; i < length-40; i += 60)
		{
			numPoints += 2;
			chunk = &bytes[i];
			if (i > length-60) twoPoints = 0 ;
			// Coordinate and data of point 1
			double p1latitude = ((double) bytesToInt4(chunk[0], chunk[1], chunk[2], chunk[3])) / 1000000.;
			double p1longitude = ((double) bytesToInt4(chunk[4], chunk[5], chunk[6], chunk[7])) / 1000000.;
			uint32_t p1distance = bytesToInt4(chunk[8], chunk[9], chunk[10], chunk[11]); // m
			uint32_t p1time = bytesToInt2(chunk[12], chunk[13]); // sec
			uint16_t p1fiability = static_cast<uint16_t>(bytes[14]);
			int16_t p1alt = ((int16_t) bytesToInt2(chunk[15], chunk[16])); // m
			// Metadata of point 1
			metaoffset = 20 + 20 * twoPoints;
			//uint32_t p1time = bytesToInt2(chunk[metaoffset], chunk[metaoffset +1]); // sec
			double p1speed = ((double) bytesToInt2(chunk[metaoffset + 2], chunk[metaoffset + 3])) / 100.; // km/h
			//double p1energy = ((double bytesToInt2(chunk[metaoffset + 4], chunk[metaoffset + 5])); // kCal
			uint16_t p1hr = ((uint16_t) bytesToInt2(chunk[metaoffset + 6], chunk[metaoffset + 7]));
			//uint32_t p1newLap = static_cast<uint32_t>(bytes[8]);
			//uint32_t p1endOfTrack = static_cast<uint32_t>(bytes[9]);
			auto p1 = new Point(p1latitude, p1longitude, p1alt, p1speed, startTime + p1time, 0, p1hr, p1fiability);
			p1->setDistance(p1distance);
			session->addPoint(p1);
			if (twoPoints) {
				// Coordinate and data of point 2
				double p2latitude = ((double) bytesToInt4(chunk[20], chunk[21], chunk[22], chunk[23])) / 1000000.;
				double p2longitude = ((double) bytesToInt4(chunk[24], chunk[25], chunk[26], chunk[27])) / 1000000.;
				uint32_t p2distance = bytesToInt4(chunk[28], chunk[29], chunk[30], chunk[31]); // m
				uint32_t p2time = bytesToInt2(chunk[32], chunk[33]); // sec
				uint16_t p2fiability = static_cast<uint16_t>(bytes[34]);
				int16_t p2alt = ((int16_t) bytesToInt2(chunk[35], chunk[36])); // m
				// metadata of point 2
				metaoffset += 10;
				//uint32_t p2time = bytesToInt2(chunk[metaoffset], chunk[metaoffset +1]); // sec
				double p2speed = ((double) bytesToInt2(chunk[metaoffset + 2], chunk[metaoffset + 3])) / 100.; // km/h
				//double p2energy = ((double bytesToInt2(chunk[metaoffset + 4], chunk[metaoffset + 5])); // kCal
				uint16_t p2hr = ((uint16_t) bytesToInt2(chunk[metaoffset + 6], chunk[metaoffset + 7]));
				//uint32_t p2newLap = static_cast<uint32_t>(bytes[8]);
				//uint32_t p2endOfTrack = static_cast<uint32_t>(bytes[9]);
				auto p2 = new Point(p2latitude, p2longitude, p2alt, p2speed, startTime + p2time, 0, p2hr, p2fiability);
				p2->setDistance(p2distance);
				session->addPoint(p2);
			}
		}
	}
}
