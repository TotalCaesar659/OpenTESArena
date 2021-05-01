#include <algorithm>
#include <limits>

#include "ArenaWeatherUtils.h"
#include "WeatherDefinition.h"
#include "WeatherInstance.h"
#include "../Math/Constants.h"
#include "../Math/Random.h"
#include "../Rendering/ArenaRenderUtils.h"

#include "components/debug/Debug.h"

namespace
{
	double MakeSecondsUntilNextLightning(Random &random)
	{
		return ArenaWeatherUtils::THUNDERSTORM_SKY_FLASH_SECONDS + (random.nextReal() * 5.0);
	}

	Radians MakeLightningBoltAngle(Random &random)
	{
		return random.nextReal() * Constants::TwoPi;
	}

	bool MakeSnowflakeDirection(Random &random)
	{
		return (random.next() % 2) != 0;
	}
}

void WeatherInstance::Particle::init(double xPercent, double yPercent)
{
	this->xPercent = xPercent;
	this->yPercent = yPercent;
}

void WeatherInstance::RainInstance::Thunderstorm::init(Buffer<uint8_t> &&flashColors, Random &random)
{
	this->flashColors = std::move(flashColors);
	this->secondsSincePrevLightning = std::numeric_limits<double>::infinity();
	this->secondsUntilNextLightning = MakeSecondsUntilNextLightning(random);
	this->lightningBoltAngle = 0.0;
	this->active = false;
}

int WeatherInstance::RainInstance::Thunderstorm::getFlashColorCount() const
{
	return this->flashColors.getCount();
}

uint8_t WeatherInstance::RainInstance::Thunderstorm::getFlashColor(int index) const
{
	return this->flashColors.get(index);
}

double WeatherInstance::RainInstance::Thunderstorm::getFlashPercent() const
{
	const double percent = this->secondsSincePrevLightning / ArenaWeatherUtils::THUNDERSTORM_SKY_FLASH_SECONDS;
	return std::clamp(1.0 - percent, 0.0, 1.0);
}

bool WeatherInstance::RainInstance::Thunderstorm::isLightningBoltVisible() const
{
	return this->secondsSincePrevLightning <= ArenaWeatherUtils::THUNDERSTORM_BOLT_SECONDS;
}

void WeatherInstance::RainInstance::Thunderstorm::update(double dt, Random &random)
{
	if (this->active)
	{
		this->secondsSincePrevLightning += dt;
		this->secondsUntilNextLightning -= dt;
		if (this->secondsUntilNextLightning <= 0.0)
		{
			this->secondsSincePrevLightning = 0.0;
			this->secondsUntilNextLightning = MakeSecondsUntilNextLightning(random);
			this->lightningBoltAngle = MakeLightningBoltAngle(random);

			// @todo: signal lightning bolt to generate + appear, and audio to play.
		}
	}
}

void WeatherInstance::RainInstance::init(bool isThunderstorm, Buffer<uint8_t> &&flashColors, Random &random)
{
	this->particles.init(ArenaWeatherUtils::RAINDROP_TOTAL_COUNT);
	for (int i = 0; i < this->particles.getCount(); i++)
	{
		Particle &particle = this->particles.get(i);
		particle.init(random.nextReal(), random.nextReal());
	}

	if (isThunderstorm)
	{
		this->thunderstorm = std::make_optional<Thunderstorm>();
		this->thunderstorm->init(std::move(flashColors), random);
	}
	else
	{
		this->thunderstorm = std::nullopt;
	}
}

void WeatherInstance::RainInstance::update(double dt, double aspectRatio, Random &random)
{
	auto animateRaindropRange = [this, dt, aspectRatio, &random](int startIndex, int endIndex,
		double velocityPercentX, double velocityPercentY)
	{
		for (int i = startIndex; i < endIndex; i++)
		{
			Particle &particle = this->particles.get(i);
			const bool canBeRestarted = (particle.xPercent < 0.0) || (particle.yPercent >= 1.0);
			if (canBeRestarted)
			{
				// Pick a screen edge to spawn at. This involves the aspect ratio so drops are properly distributed.
				const double topEdgeLength = aspectRatio;
				constexpr double rightEdgeLength = 1.0;
				const double topEdgePercent = topEdgeLength / (topEdgeLength + rightEdgeLength);
				if (random.nextReal() <= topEdgePercent)
				{
					// Top edge.
					particle.xPercent = random.nextReal();
					particle.yPercent = 0.0;
				}
				else
				{
					// Right edge.
					particle.xPercent = 1.0;
					particle.yPercent = random.nextReal();
				}
			}
			else
			{
				// The particle's horizontal movement is aspect-ratio-dependent.
				const double aspectRatioMultiplierX = ArenaRenderUtils::ASPECT_RATIO / aspectRatio;
				const double deltaPercentX = (velocityPercentX * aspectRatioMultiplierX) * dt;
				const double deltaPercentY = velocityPercentY * dt;
				particle.xPercent += deltaPercentX;
				particle.yPercent += deltaPercentY;
			}
		}
	};

	constexpr int fastStartIndex = 0;
	constexpr int fastEndIndex = ArenaWeatherUtils::RAINDROP_FAST_COUNT;
	constexpr int mediumStartIndex = fastEndIndex;
	constexpr int mediumEndIndex = mediumStartIndex + ArenaWeatherUtils::RAINDROP_MEDIUM_COUNT;
	constexpr int slowStartIndex = mediumEndIndex;
	constexpr int slowEndIndex = slowStartIndex + ArenaWeatherUtils::RAINDROP_SLOW_COUNT;

	constexpr double arenaScreenWidthReal = static_cast<double>(ArenaRenderUtils::SCREEN_WIDTH);
	constexpr double arenaScreenHeightReal = static_cast<double>(ArenaRenderUtils::SCREEN_HEIGHT);
	constexpr int arenaFramesPerSecond = ArenaRenderUtils::FRAMES_PER_SECOND;

	constexpr double fastVelocityPercentX = static_cast<double>(
		ArenaWeatherUtils::RAINDROP_FAST_PIXELS_PER_FRAME_X * arenaFramesPerSecond) / arenaScreenWidthReal;
	constexpr double fastVelocityPercentY = static_cast<double>(
		ArenaWeatherUtils::RAINDROP_FAST_PIXELS_PER_FRAME_Y * arenaFramesPerSecond) / arenaScreenHeightReal;
	constexpr double mediumVelocityPercentX = static_cast<double>(
		ArenaWeatherUtils::RAINDROP_MEDIUM_PIXELS_PER_FRAME_X * arenaFramesPerSecond) / arenaScreenWidthReal;
	constexpr double mediumVelocityPercentY = static_cast<double>(
		ArenaWeatherUtils::RAINDROP_MEDIUM_PIXELS_PER_FRAME_Y * arenaFramesPerSecond) / arenaScreenHeightReal;
	constexpr double slowVelocityPercentX = static_cast<double>(
		ArenaWeatherUtils::RAINDROP_SLOW_PIXELS_PER_FRAME_X * arenaFramesPerSecond) / arenaScreenWidthReal;
	constexpr double slowVelocityPercentY = static_cast<double>(
		ArenaWeatherUtils::RAINDROP_SLOW_PIXELS_PER_FRAME_Y * arenaFramesPerSecond) / arenaScreenHeightReal;

	animateRaindropRange(fastStartIndex, fastEndIndex, fastVelocityPercentX, fastVelocityPercentY);
	animateRaindropRange(mediumStartIndex, mediumEndIndex, mediumVelocityPercentX, mediumVelocityPercentY);
	animateRaindropRange(slowStartIndex, slowEndIndex, slowVelocityPercentX, slowVelocityPercentY);

	if (this->thunderstorm.has_value())
	{
		this->thunderstorm->update(dt, random);
	}
}

void WeatherInstance::SnowInstance::init(Random &random)
{
	this->particles.init(ArenaWeatherUtils::SNOWFLAKE_TOTAL_COUNT);
	for (int i = 0; i < this->particles.getCount(); i++)
	{
		Particle &particle = this->particles.get(i);
		particle.init(random.nextReal(), random.nextReal());
	}

	this->directions.init(this->particles.getCount());
	for (int i = 0; i < this->directions.getCount(); i++)
	{
		this->directions.set(i, MakeSnowflakeDirection(random));
	}

	this->lastDirectionChangeSeconds.init(this->particles.getCount());
	this->lastDirectionChangeSeconds.fill(0.0);
}

void WeatherInstance::SnowInstance::update(double dt, double aspectRatio, Random &random)
{
	auto animateSnowflakeRange = [this, dt, aspectRatio, &random](int startIndex, int endIndex,
		double velocityPercentX, double velocityPercentY)
	{
		for (int i = startIndex; i < endIndex; i++)
		{
			Particle &particle = this->particles.get(i);
			const bool canBeRestarted = particle.yPercent >= 1.0;
			if (canBeRestarted)
			{
				// Pick somewhere on the top edge to spawn.
				particle.xPercent = random.nextReal();
				particle.yPercent = 0.0;

				this->directions.set(i, MakeSnowflakeDirection(random));
			}
			else
			{
				double &secondsSinceDirectionChange = this->lastDirectionChangeSeconds.get(i);
				secondsSinceDirectionChange += dt;

				// The snowflake gets a chance to change direction a few times a second.
				if (secondsSinceDirectionChange >= ArenaWeatherUtils::SNOWFLAKE_MIN_SECONDS_BEFORE_DIRECTION_CHANGE)
				{
					secondsSinceDirectionChange = std::fmod(
						secondsSinceDirectionChange, ArenaWeatherUtils::SNOWFLAKE_MIN_SECONDS_BEFORE_DIRECTION_CHANGE);

					if (ArenaWeatherUtils::shouldSnowflakeChangeDirection(random))
					{
						this->directions.set(i, !this->directions.get(i));
					}
				}

				const double directionX = this->directions.get(i) ? 1.0 : -1.0;

				// The particle's horizontal movement is aspect-ratio-dependent.
				const double aspectRatioMultiplierX = ArenaRenderUtils::ASPECT_RATIO / aspectRatio;

				// This seems to make snowflakes move at a closer speed to the original game.
				constexpr double velocityCorrectionX = 0.50;

				const double deltaPercentX = (velocityPercentX * directionX * aspectRatioMultiplierX * velocityCorrectionX) * dt;
				const double deltaPercentY = velocityPercentY * dt;
				particle.xPercent += deltaPercentX;
				particle.yPercent += deltaPercentY;
			}
		}
	};

	constexpr int fastStartIndex = 0;
	constexpr int fastEndIndex = ArenaWeatherUtils::SNOWFLAKE_FAST_COUNT;
	constexpr int mediumStartIndex = fastEndIndex;
	constexpr int mediumEndIndex = mediumStartIndex + ArenaWeatherUtils::SNOWFLAKE_MEDIUM_COUNT;
	constexpr int slowStartIndex = mediumEndIndex;
	constexpr int slowEndIndex = slowStartIndex + ArenaWeatherUtils::SNOWFLAKE_SLOW_COUNT;

	constexpr double arenaScreenWidthReal = static_cast<double>(ArenaRenderUtils::SCREEN_WIDTH);
	constexpr double arenaScreenHeightReal = static_cast<double>(ArenaRenderUtils::SCREEN_HEIGHT);
	constexpr int arenaFramesPerSecond = ArenaRenderUtils::FRAMES_PER_SECOND;

	constexpr double fastVelocityPercentX = static_cast<double>(
		ArenaWeatherUtils::SNOWFLAKE_PIXELS_PER_FRAME_X * arenaFramesPerSecond) / arenaScreenWidthReal;
	constexpr double fastVelocityPercentY = static_cast<double>(
		ArenaWeatherUtils::SNOWFLAKE_FAST_PIXELS_PER_FRAME_Y * arenaFramesPerSecond) / arenaScreenHeightReal;
	constexpr double mediumVelocityPercentX = static_cast<double>(
		ArenaWeatherUtils::SNOWFLAKE_PIXELS_PER_FRAME_X * arenaFramesPerSecond) / arenaScreenWidthReal;
	constexpr double mediumVelocityPercentY = static_cast<double>(
		ArenaWeatherUtils::SNOWFLAKE_MEDIUM_PIXELS_PER_FRAME_Y * arenaFramesPerSecond) / arenaScreenHeightReal;
	constexpr double slowVelocityPercentX = static_cast<double>(
		ArenaWeatherUtils::SNOWFLAKE_PIXELS_PER_FRAME_X * arenaFramesPerSecond) / arenaScreenWidthReal;
	constexpr double slowVelocityPercentY = static_cast<double>(
		ArenaWeatherUtils::SNOWFLAKE_SLOW_PIXELS_PER_FRAME_Y * arenaFramesPerSecond) / arenaScreenHeightReal;

	animateSnowflakeRange(fastStartIndex, fastEndIndex, fastVelocityPercentX, fastVelocityPercentY);
	animateSnowflakeRange(mediumStartIndex, mediumEndIndex, mediumVelocityPercentX, mediumVelocityPercentY);
	animateSnowflakeRange(slowStartIndex, slowEndIndex, slowVelocityPercentX, slowVelocityPercentY);
}

WeatherInstance::WeatherInstance()
{
	this->type = static_cast<WeatherInstance::Type>(-1);
}

void WeatherInstance::init(const WeatherDefinition &weatherDef, const ExeData &exeData, Random &random)
{
	const WeatherDefinition::Type weatherDefType = weatherDef.getType();

	if ((weatherDefType == WeatherDefinition::Type::Clear) ||
		(weatherDefType == WeatherDefinition::Type::Overcast))
	{
		this->type = WeatherInstance::Type::None;
	}
	else if (weatherDefType == WeatherDefinition::Type::Rain)
	{
		this->type = WeatherInstance::Type::Rain;

		const WeatherDefinition::RainDefinition &rainDef = weatherDef.getRain();
		Buffer<uint8_t> thunderstormColors = ArenaWeatherUtils::makeThunderstormColors(exeData);
		this->rain.init(rainDef.thunderstorm, std::move(thunderstormColors), random);
	}
	else if (weatherDefType == WeatherDefinition::Type::Snow)
	{
		this->type = WeatherInstance::Type::Snow;
		this->snow.init(random);
	}
	else
	{
		DebugNotImplementedMsg(std::to_string(static_cast<int>(weatherDefType)));
	}
}

WeatherInstance::Type WeatherInstance::getType() const
{
	return this->type;
}

WeatherInstance::RainInstance& WeatherInstance::getRain()
{
	DebugAssert(this->type == WeatherInstance::Type::Rain);
	return this->rain;
}

const WeatherInstance::RainInstance &WeatherInstance::getRain() const
{
	DebugAssert(this->type == WeatherInstance::Type::Rain);
	return this->rain;
}

const WeatherInstance::SnowInstance &WeatherInstance::getSnow() const
{
	DebugAssert(this->type == WeatherInstance::Type::Snow);
	return this->snow;
}

void WeatherInstance::update(double dt, double aspectRatio, Random &random)
{
	if (this->type == WeatherInstance::Type::None)
	{
		// Do nothing.
	}
	else if (this->type == WeatherInstance::Type::Rain)
	{
		this->rain.update(dt, aspectRatio, random);
	}
	else if (this->type == WeatherInstance::Type::Snow)
	{
		this->snow.update(dt, aspectRatio, random);
	}
	else
	{
		DebugNotImplementedMsg(std::to_string(static_cast<int>(this->type)));
	}
}
