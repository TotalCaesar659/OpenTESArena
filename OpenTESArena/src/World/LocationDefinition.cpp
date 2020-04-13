#include <cstring>

#include "Location.h"
#include "LocationDefinition.h"
#include "LocationUtils.h"
#include "../Assets/MiscAssets.h"

#include "components/debug/Debug.h"

void LocationDefinition::CityDefinition::MainQuestTempleOverride::init(int modelIndex,
	int suffixIndex, int menuNamesIndex)
{
	this->modelIndex = modelIndex;
	this->suffixIndex = suffixIndex;
	this->menuNamesIndex = menuNamesIndex;
}

void LocationDefinition::CityDefinition::init(CityDefinition::Type type, const char *typeDisplayName,
	uint32_t citySeed, uint32_t wildSeed, uint32_t provinceSeed, uint32_t rulerSeed,
	uint32_t distantSkySeed, ClimateType climateType,
	const MainQuestTempleOverride *mainQuestTempleOverride, int cityBlocksPerSide, bool coastal,
	bool premade)
{
	this->type = type;
	std::snprintf(this->typeDisplayName, std::size(this->typeDisplayName), "%s", typeDisplayName);

	this->citySeed = citySeed;
	this->wildSeed = wildSeed;
	this->provinceSeed = provinceSeed;
	this->rulerSeed = rulerSeed;
	this->distantSkySeed = distantSkySeed;
	this->climateType = climateType;

	if (mainQuestTempleOverride != nullptr)
	{
		this->hasMainQuestTempleOverride = true;
		this->mainQuestTempleOverride = *mainQuestTempleOverride;
	}
	else
	{
		this->hasMainQuestTempleOverride = false;
	}

	this->cityBlocksPerSide = cityBlocksPerSide;
	this->coastal = coastal;
	this->premade = premade;
}

uint32_t LocationDefinition::CityDefinition::getWildDungeonSeed(int wildBlockX, int wildBlockY) const
{
	return (this->provinceSeed + (((wildBlockY << 6) + wildBlockX) & 0xFFFF)) & 0xFFFFFFFF;
}

void LocationDefinition::DungeonDefinition::init()
{
	// Do nothing
}

void LocationDefinition::MainQuestDungeonDefinition::init(MainQuestDungeonDefinition::Type type)
{
	this->type = type;
}

void LocationDefinition::init(LocationDefinition::Type type, const std::string &name,
	int x, int y, double latitude)
{
	this->name = name;
	this->x = x;
	this->y = y;
	this->latitude = latitude;
	this->visibleByDefault = (type == LocationDefinition::Type::City) && (name.size() > 0);
	this->type = type;
}

void LocationDefinition::initCity(int localCityID, int provinceID, bool coastal, bool premade,
	CityDefinition::Type type, const MiscAssets &miscAssets)
{
	const auto &cityData = miscAssets.getCityDataFile();
	const auto &provinceData = cityData.getProvinceData(provinceID);
	const auto &locationData = provinceData.getLocationData(localCityID);
	const Int2 localPoint(locationData.x, locationData.y);
	const Rect provinceRect = provinceData.getGlobalRect();
	const double latitude = [&provinceData, &locationData, &localPoint, &provinceRect]()
	{
		const Int2 globalPoint = LocationUtils::getGlobalPoint(localPoint, provinceRect);
		return LocationUtils::getLatitude(globalPoint);
	}();

	this->init(LocationDefinition::Type::City, locationData.name,
		locationData.x, locationData.y, latitude);

	const std::string &typeDisplayName = [type, &miscAssets]() -> const std::string&
	{
		const int typeNameIndex = [type]()
		{
			switch (type)
			{
			case CityDefinition::Type::CityState:
				return 0;
			case CityDefinition::Type::Town:
				return 1;
			case CityDefinition::Type::Village:
				return 2;
			default:
				DebugUnhandledReturnMsg(int, std::to_string(static_cast<int>(type)));
			}
		}();

		const auto &exeData = miscAssets.getExeData();
		const auto &locationTypeNames = exeData.locations.locationTypes;
		DebugAssertIndex(locationTypeNames, typeNameIndex);
		return locationTypeNames[typeNameIndex];
	}();

	const uint32_t citySeed = LocationUtils::getCitySeed(localCityID, provinceData);
	const uint32_t wildSeed = LocationUtils::getWildernessSeed(localCityID, provinceData);
	const uint32_t provinceSeed = LocationUtils::getProvinceSeed(provinceID, provinceData);
	const uint32_t rulerSeed = LocationUtils::getRulerSeed(localPoint, provinceRect);
	const uint32_t distantSkySeed = LocationUtils::getDistantSkySeed(localPoint, provinceID, provinceRect);
	const ClimateType climateType = LocationUtils::getCityClimateType(localCityID, provinceID, miscAssets);
	const int cityBlocksPerSide = [type]()
	{
		switch (type)
		{
		case CityDefinition::Type::CityState:
			return 6;
		case CityDefinition::Type::Town:
			return 5;
		case CityDefinition::Type::Village:
			return 4;
		default:
			DebugUnhandledReturnMsg(int, std::to_string(static_cast<int>(type)));
		}
	}();

	CityDefinition::MainQuestTempleOverride mainQuestTempleOverride;
	const CityDefinition::MainQuestTempleOverride *mainQuestTempleOverridePtr = &mainQuestTempleOverride;
	const int globalCityID = LocationUtils::getGlobalCityID(localCityID, provinceID);
	if (globalCityID == 2)
	{
		mainQuestTempleOverride.init(1, 7, 23);
	}
	else if (globalCityID == 224)
	{
		mainQuestTempleOverride.init(2, 8, 32);
	}
	else
	{
		mainQuestTempleOverridePtr = nullptr;
	}

	this->city.init(type, typeDisplayName.c_str(), citySeed, wildSeed, provinceSeed, rulerSeed,
		distantSkySeed, climateType, mainQuestTempleOverridePtr, cityBlocksPerSide,
		coastal, premade);
}

void LocationDefinition::initDungeon(const CityDataFile::ProvinceData::LocationData &locationData,
	const CityDataFile::ProvinceData &provinceData)
{
	const double latitude = [&locationData, &provinceData]()
	{
		const Int2 globalPoint = LocationUtils::getGlobalPoint(
			Int2(locationData.x, locationData.y), provinceData.getGlobalRect());
		return LocationUtils::getLatitude(globalPoint);
	}();

	this->init(LocationDefinition::Type::Dungeon, locationData.name,
		locationData.x, locationData.y, latitude);
	this->dungeon.init();
}

void LocationDefinition::initMainQuestDungeon(MainQuestDungeonDefinition::Type type,
	const CityDataFile::ProvinceData::LocationData &locationData,
	const CityDataFile::ProvinceData &provinceData, const ExeData &exeData)
{
	// Start dungeon's display name is custom.
	std::string name = [type, &locationData, &exeData]()
	{
		switch (type)
		{
		case MainQuestDungeonDefinition::Type::Start:
			return exeData.locations.startDungeonName;
		case MainQuestDungeonDefinition::Type::Map:
		case MainQuestDungeonDefinition::Type::Staff:
			return locationData.name;
		default:
			DebugUnhandledReturnMsg(std::string, std::to_string(static_cast<int>(type)));
		}
	}();

	const double latitude = [&locationData, &provinceData]()
	{
		const Int2 globalPoint = LocationUtils::getGlobalPoint(
			Int2(locationData.x, locationData.y), provinceData.getGlobalRect());
		return LocationUtils::getLatitude(globalPoint);
	}();

	this->init(LocationDefinition::Type::MainQuestDungeon, std::move(name),
		locationData.x, locationData.y, latitude);
	this->mainQuest.init(type);
}

const std::string &LocationDefinition::getName() const
{
	return this->name;
}

int LocationDefinition::getScreenX() const
{
	return this->x;
}

int LocationDefinition::getScreenY() const
{
	return this->y;
}

double LocationDefinition::getLatitude() const
{
	return this->latitude;
}

bool LocationDefinition::isVisibleByDefault() const
{
	return this->visibleByDefault;
}

LocationDefinition::Type LocationDefinition::getType() const
{
	return this->type;
}

const LocationDefinition::CityDefinition &LocationDefinition::getCityDefinition() const
{
	DebugAssert(this->type == LocationDefinition::Type::City);
	return this->city;
}

const LocationDefinition::DungeonDefinition &LocationDefinition::getDungeonDefinition() const
{
	DebugAssert(this->type == LocationDefinition::Type::Dungeon);
	return this->dungeon;
}

const LocationDefinition::MainQuestDungeonDefinition &LocationDefinition::getMainQuestDungeonDefinition() const
{
	DebugAssert(this->type == LocationDefinition::Type::MainQuestDungeon);
	return this->mainQuest;
}
