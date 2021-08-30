#include <algorithm>
#include <limits>
#include <optional>

#include "SDL.h"

#include "OptionsPanel.h"
#include "OptionsUiController.h"
#include "OptionsUiView.h"
#include "PauseMenuPanel.h"
#include "../Audio/AudioManager.h"
#include "../Entities/Player.h"
#include "../Game/Game.h"
#include "../Game/GameState.h"
#include "../Game/Options.h"
#include "../Game/PlayerInterface.h"
#include "../Input/InputActionName.h"
#include "../Media/Color.h"
#include "../Media/TextureManager.h"
#include "../Rendering/ArenaRenderUtils.h"
#include "../Rendering/Renderer.h"
#include "../UI/CursorAlignment.h"
#include "../UI/CursorData.h"
#include "../UI/FontLibrary.h"
#include "../UI/Surface.h"
#include "../UI/TextAlignment.h"
#include "../UI/Texture.h"

#include "components/debug/Debug.h"
#include "components/utilities/String.h"

namespace
{
	void TryIncrementOption(OptionsUiModel::Option &option)
	{
		const OptionsUiModel::OptionType optionType = option.getType();
		if (optionType == OptionsUiModel::OptionType::Bool)
		{
			OptionsUiModel::BoolOption &boolOpt = static_cast<OptionsUiModel::BoolOption&>(option);
			boolOpt.toggle();
		}
		else if (optionType == OptionsUiModel::OptionType::Int)
		{
			OptionsUiModel::IntOption &intOpt = static_cast<OptionsUiModel::IntOption&>(option);
			intOpt.set(intOpt.getNext());
		}
		else if (optionType == OptionsUiModel::OptionType::Double)
		{
			OptionsUiModel::DoubleOption &doubleOpt = static_cast<OptionsUiModel::DoubleOption&>(option);
			doubleOpt.set(doubleOpt.getNext());
		}
		else if (optionType == OptionsUiModel::OptionType::String)
		{
			// Do nothing.
			static_cast<void>(option);
		}
		else
		{
			throw DebugException("Invalid type \"" + std::to_string(static_cast<int>(optionType)) + "\".");
		}
	}

	void TryDecrementOption(OptionsUiModel::Option &option)
	{
		const OptionsUiModel::OptionType optionType = option.getType();
		if (optionType == OptionsUiModel::OptionType::Bool)
		{
			OptionsUiModel::BoolOption &boolOpt = static_cast<OptionsUiModel::BoolOption&>(option);
			boolOpt.toggle();
		}
		else if (optionType == OptionsUiModel::OptionType::Int)
		{
			OptionsUiModel::IntOption &intOpt = static_cast<OptionsUiModel::IntOption&>(option);
			intOpt.set(intOpt.getPrev());
		}
		else if (optionType == OptionsUiModel::OptionType::Double)
		{
			OptionsUiModel::DoubleOption &doubleOpt = static_cast<OptionsUiModel::DoubleOption&>(option);
			doubleOpt.set(doubleOpt.getPrev());
		}
		else if (optionType == OptionsUiModel::OptionType::String)
		{
			// Do nothing.
			static_cast<void>(option);
		}
		else
		{
			throw DebugException("Invalid type \"" + std::to_string(static_cast<int>(optionType)) + "\".");
		}
	}
}

OptionsPanel::OptionsPanel(Game &game)
	: Panel(game) { }

bool OptionsPanel::init()
{
	auto &game = this->getGame();
	auto &renderer = game.getRenderer();
	const auto &fontLibrary = game.getFontLibrary();

	const std::string titleText = OptionsUiModel::OptionsTitleText;
	const TextBox::InitInfo titleTextBoxInitInfo =
		OptionsUiView::getTitleTextBoxInitInfo(titleText, fontLibrary);
	if (!this->titleTextBox.init(titleTextBoxInitInfo, titleText, renderer))
	{
		DebugLogError("Couldn't init title text box.");
		return false;
	}

	const std::string backToPauseMenuText = OptionsUiModel::BackToPauseMenuText;
	const TextBox::InitInfo backToPauseMenuTextBoxInitInfo =
		OptionsUiView::getBackToPauseMenuTextBoxInitInfo(backToPauseMenuText, fontLibrary);
	if (!this->backToPauseMenuTextBox.init(backToPauseMenuTextBoxInitInfo, backToPauseMenuText, renderer))
	{
		DebugLogError("Couldn't init back to pause menu text box.");
		return false;
	}

	auto initTabTextBox = [&renderer, &fontLibrary](TextBox &textBox, int tabIndex, const std::string &text)
	{
		const Rect &graphicsTabRect = OptionsUiView::GraphicsTabRect;
		const Int2 &tabsDimensions = OptionsUiView::TabsDimensions;
		const Int2 initialTabTextCenter(
			graphicsTabRect.getLeft() + (graphicsTabRect.getWidth() / 2),
			graphicsTabRect.getTop() + (graphicsTabRect.getHeight() / 2));
		const Int2 tabOffset(0, tabsDimensions.y * tabIndex);
		const Int2 center = initialTabTextCenter + tabOffset;

		const TextBox::InitInfo textBoxInitInfo = TextBox::InitInfo::makeWithCenter(
			text,
			center,
			OptionsUiView::TabFontName,
			OptionsUiView::getTabTextColor(),
			OptionsUiView::TabTextAlignment,
			fontLibrary);

		if (!textBox.init(textBoxInitInfo, text, renderer))
		{
			DebugCrash("Couldn't init text box " + std::to_string(tabIndex) + ".");
		}
	};

	// @todo: should make this iterable
	initTabTextBox(this->graphicsTextBox, 0, OptionsUiModel::GRAPHICS_TAB_NAME);
	initTabTextBox(this->audioTextBox, 1, OptionsUiModel::AUDIO_TAB_NAME);
	initTabTextBox(this->inputTextBox, 2, OptionsUiModel::INPUT_TAB_NAME);
	initTabTextBox(this->miscTextBox, 3, OptionsUiModel::MISC_TAB_NAME);
	initTabTextBox(this->devTextBox, 4, OptionsUiModel::DEV_TAB_NAME);

	// Button proxies are added later.
	this->backToPauseMenuButton = Button<Game&>(
		OptionsUiView::BackToPauseMenuButtonCenterPoint,
		OptionsUiView::BackToPauseMenuButtonWidth,
		OptionsUiView::BackToPauseMenuButtonHeight,
		OptionsUiController::onBackToPauseMenuButtonSelected);
	this->tabButton = Button<OptionsPanel&, OptionsUiModel::Tab*, OptionsUiModel::Tab>(
		OptionsUiController::onTabButtonSelected);

	this->addInputActionListener(InputActionName::Back,
		[this](const InputActionCallbackValues &values)
	{
		if (values.performed)
		{
			this->backToPauseMenuButton.click(values.game);
		}
	});

	// Create graphics options.
	this->graphicsOptions.emplace_back(OptionsUiModel::makeWindowModeOption(game));
	this->graphicsOptions.emplace_back(OptionsUiModel::makeFpsLimitOption(game));
	this->graphicsOptions.emplace_back(OptionsUiModel::makeResolutionScaleOption(game));
	this->graphicsOptions.emplace_back(OptionsUiModel::makeVerticalFovOption(game));
	this->graphicsOptions.emplace_back(OptionsUiModel::makeLetterboxModeOption(game));
	this->graphicsOptions.emplace_back(OptionsUiModel::makeCursorScaleOption(game));
	this->graphicsOptions.emplace_back(OptionsUiModel::makeModernInterfaceOption(game));
	this->graphicsOptions.emplace_back(OptionsUiModel::makeRenderThreadsModeOption(game));

	// Create audio options.
	this->audioOptions.emplace_back(OptionsUiModel::makeSoundChannelsOption(game));
	this->audioOptions.emplace_back(OptionsUiModel::makeSoundResamplingOption(game));
	this->audioOptions.emplace_back(OptionsUiModel::makeIs3dAudioOption(game));

	// Create input options.
	this->inputOptions.emplace_back(OptionsUiModel::makeHorizontalSensitivityOption(game));
	this->inputOptions.emplace_back(OptionsUiModel::makeVerticalSensitivityOption(game));
	this->inputOptions.emplace_back(OptionsUiModel::makeCameraPitchLimitOption(game));
	this->inputOptions.emplace_back(OptionsUiModel::makePixelPerfectSelectionOption(game));

	// Create miscellaneous options.
	this->miscOptions.emplace_back(OptionsUiModel::makeShowCompassOption(game));
	this->miscOptions.emplace_back(OptionsUiModel::makeShowIntroOption(game));
	this->miscOptions.emplace_back(OptionsUiModel::makeTimeScaleOption(game));
	this->miscOptions.emplace_back(OptionsUiModel::makeChunkDistanceOption(game));
	this->miscOptions.emplace_back(OptionsUiModel::makeStarDensityOption(game));
	this->miscOptions.emplace_back(OptionsUiModel::makePlayerHasLightOption(game));

	// Create developer options.
	this->devOptions.emplace_back(OptionsUiModel::makeCollisionOption(game));
	this->devOptions.emplace_back(OptionsUiModel::makeProfilerLevelOption(game));

	// Set initial tab.
	this->tab = OptionsUiModel::Tab::Graphics;

	// Initialize all option text boxes and buttons for the initial tab.
	this->updateVisibleOptions();

	return true;
}

std::vector<std::unique_ptr<OptionsUiModel::Option>> &OptionsPanel::getVisibleOptions()
{
	if (this->tab == OptionsUiModel::Tab::Graphics)
	{
		return this->graphicsOptions;
	}
	else if (this->tab == OptionsUiModel::Tab::Audio)
	{
		return this->audioOptions;
	}
	else if (this->tab == OptionsUiModel::Tab::Input)
	{
		return this->inputOptions;
	}
	else if (this->tab == OptionsUiModel::Tab::Misc)
	{
		return this->miscOptions;
	}
	else if (this->tab == OptionsUiModel::Tab::Dev)
	{
		return this->devOptions;
	}
	else
	{
		throw DebugException("Invalid tab \"" +
			std::to_string(static_cast<int>(this->tab)) + "\".");
	}
}

void OptionsPanel::initOptionTextBox(int index)
{
	auto &game = this->getGame();
	const auto &fontLibrary = game.getFontLibrary();

	const std::string &fontName = OptionsUiView::OptionTextBoxFontName;
	int fontDefIndex;
	if (!fontLibrary.tryGetDefinitionIndex(fontName.c_str(), &fontDefIndex))
	{
		DebugCrash("Couldn't get font definition for \"" + fontName + "\".");
	}

	const FontDefinition &fontDef = fontLibrary.getDefinition(fontDefIndex);
	const std::string dummyText(28, TextRenderUtils::LARGEST_CHAR);
	const TextRenderUtils::TextureGenInfo textureGenInfo = TextRenderUtils::makeTextureGenInfo(dummyText, fontDef);
	const Int2 &point = OptionsUiView::ListOrigin;
	const int yOffset = textureGenInfo.height * index;
	const TextBox::InitInfo textBoxInitInfo = TextBox::InitInfo::makeWithXY(
		dummyText,
		point.x,
		point.y + yOffset,
		fontName,
		OptionsUiView::getOptionTextBoxColor(),
		OptionsUiView::OptionTextBoxTextAlignment,
		fontLibrary);

	DebugAssertIndex(this->currentTabTextBoxes, index);
	TextBox &textBox = this->currentTabTextBoxes[index];
	if (!textBox.init(textBoxInitInfo, game.getRenderer()))
	{
		DebugCrash("Couldn't init tab text box " + std::to_string(index) + ".");
	}
}

void OptionsPanel::updateOptionTextBoxText(int index)
{
	auto &game = this->getGame();
	const auto &visibleOptions = this->getVisibleOptions();
	DebugAssertIndex(visibleOptions, index);
	const auto &visibleOption = visibleOptions[index];
	const std::string text = visibleOption->getName() + ": " + visibleOption->getDisplayedValue();

	DebugAssertIndex(this->currentTabTextBoxes, index);
	TextBox &textBox = this->currentTabTextBoxes[index];
	textBox.setText(text);
}

void OptionsPanel::updateVisibleOptions()
{
	auto &game = this->getGame();
	const auto &visibleOptions = this->getVisibleOptions();

	this->currentTabTextBoxes.clear();
	this->currentTabTextBoxes.resize(visibleOptions.size());

	// Remove all button proxies, including the static ones.
	this->clearButtonProxies();

	auto addTabButtonProxy = [this](OptionsUiModel::Tab tab, const Rect &rect)
	{
		this->addButtonProxy(MouseButtonType::Left, rect,
			[this, tab]() { this->tabButton.click(*this, &this->tab, tab); });
	};

	// Add the static button proxies.
	addTabButtonProxy(OptionsUiModel::Tab::Graphics, OptionsUiView::GraphicsTabRect);
	addTabButtonProxy(OptionsUiModel::Tab::Audio, OptionsUiView::AudioTabRect);
	addTabButtonProxy(OptionsUiModel::Tab::Input, OptionsUiView::InputTabRect);
	addTabButtonProxy(OptionsUiModel::Tab::Misc, OptionsUiView::MiscTabRect);
	addTabButtonProxy(OptionsUiModel::Tab::Dev, OptionsUiView::DevTabRect);
	this->addButtonProxy(MouseButtonType::Left, this->backToPauseMenuButton.getRect(),
		[this, &game]() { this->backToPauseMenuButton.click(game); });

	auto addOptionButtonProxies = [this](int index)
	{
		DebugAssertIndex(this->currentTabTextBoxes, index);
		const TextBox &optionTextBox = this->currentTabTextBoxes[index];
		const int optionTextBoxHeight = optionTextBox.getRect().getHeight();

		const Rect optionRect(
			OptionsUiView::ListOrigin.x,
			OptionsUiView::ListOrigin.y + (optionTextBoxHeight * index),
			OptionsUiView::ListDimensions.x,
			optionTextBoxHeight);

		auto buttonFunc = [this, index](bool isLeftClick)
		{
			auto &visibleOptions = this->getVisibleOptions();
			DebugAssertIndex(visibleOptions, index);
			std::unique_ptr<OptionsUiModel::Option> &optionPtr = visibleOptions[index];
			OptionsUiModel::Option &option = *optionPtr;

			// Modify the option based on which button was pressed.
			if (isLeftClick)
			{
				TryIncrementOption(option);
			}
			else
			{
				TryDecrementOption(option);
			}

			this->updateOptionTextBoxText(index);
		};

		this->addButtonProxy(MouseButtonType::Left, optionRect, [buttonFunc]() { buttonFunc(true); });
		this->addButtonProxy(MouseButtonType::Right, optionRect, [buttonFunc]() { buttonFunc(false); });
	};

	for (int i = 0; i < static_cast<int>(visibleOptions.size()); i++)
	{
		this->initOptionTextBox(i);
		this->updateOptionTextBoxText(i);
		addOptionButtonProxies(i);
	}
}

std::optional<CursorData> OptionsPanel::getCurrentCursor() const
{
	return this->getDefaultCursor();
}

void OptionsPanel::drawReturnButtonsAndTabs(Renderer &renderer)
{
	auto &textureManager = this->getGame().getTextureManager();
	const Rect &graphicsTabRect = OptionsUiView::GraphicsTabRect;
	Surface tabBackgroundSurface = TextureUtils::generate(
		OptionsUiView::TabBackgroundPatternType,
		graphicsTabRect.getWidth(),
		graphicsTabRect.getHeight(),
		textureManager,
		renderer);
	Texture tabBackground = renderer.createTextureFromSurface(tabBackgroundSurface);

	// @todo: this loop condition should be driven by actual tab count
	for (int i = 0; i < 5; i++)
	{
		const int tabX = graphicsTabRect.getLeft();
		const int tabY = graphicsTabRect.getTop() + (tabBackground.getHeight() * i);
		renderer.drawOriginal(tabBackground, tabX, tabY);
	}

	Surface returnBackgroundSurface = TextureUtils::generate(
		OptionsUiView::TabBackgroundPatternType,
		this->backToPauseMenuButton.getWidth(),
		this->backToPauseMenuButton.getHeight(),
		textureManager,
		renderer);
	Texture returnBackground = renderer.createTextureFromSurface(returnBackgroundSurface);

	renderer.drawOriginal(returnBackground, this->backToPauseMenuButton.getX(), this->backToPauseMenuButton.getY());
}

void OptionsPanel::drawText(Renderer &renderer)
{
	auto drawTextBox = [&renderer](TextBox &textBox)
	{
		const Rect &textBoxRect = textBox.getRect();
		renderer.drawOriginal(textBox.getTextureID(), textBoxRect.getLeft(), textBoxRect.getTop());
	};

	drawTextBox(this->titleTextBox);
	drawTextBox(this->backToPauseMenuTextBox);
	drawTextBox(this->graphicsTextBox);
	drawTextBox(this->audioTextBox);
	drawTextBox(this->inputTextBox);
	drawTextBox(this->miscTextBox);
	drawTextBox(this->devTextBox);
}

void OptionsPanel::drawTextOfOptions(Renderer &renderer)
{
	const auto &visibleOptions = this->getVisibleOptions();
	std::optional<int> highlightedOptionIndex;
	for (int i = 0; i < static_cast<int>(visibleOptions.size()); i++)
	{
		auto &optionTextBox = this->currentTabTextBoxes.at(i);
		const int optionTextBoxHeight = optionTextBox.getRect().getHeight();
		const Rect optionRect(
			OptionsUiView::ListOrigin.x,
			OptionsUiView::ListOrigin.y + (optionTextBoxHeight * i),
			OptionsUiView::ListDimensions.x,
			optionTextBoxHeight);

		const auto &inputManager = this->getGame().getInputManager();
		const Int2 mousePosition = inputManager.getMousePosition();
		const Int2 originalPosition = renderer.nativeToOriginal(mousePosition);
		const bool optionRectContainsMouse = optionRect.contains(originalPosition);

		// If the options rect contains the mouse cursor, highlight it before drawing text.
		if (optionRectContainsMouse)
		{
			renderer.fillOriginalRect(OptionsUiView::HighlightColor, optionRect.getLeft(),
				optionRect.getTop(), optionRect.getWidth(), optionRect.getHeight());

			// Store the highlighted option index for tooltip drawing.
			highlightedOptionIndex = i;
		}

		// Draw option text.
		const Rect &optionTextBoxRect = optionTextBox.getRect();
		renderer.drawOriginal(optionTextBox.getTextureID(), optionTextBoxRect.getLeft(), optionTextBoxRect.getTop());

		// Draw description if hovering over an option with a non-empty tooltip.
		if (highlightedOptionIndex.has_value())
		{
			DebugAssertIndex(visibleOptions, *highlightedOptionIndex);
			const auto &visibleOption = visibleOptions[*highlightedOptionIndex];
			const std::string &tooltip = visibleOption->getTooltip();

			// Only draw if the tooltip has text.
			if (!tooltip.empty())
			{
				this->drawDescription(tooltip, renderer);
			}
		}
	}
}

void OptionsPanel::drawDescription(const std::string &text, Renderer &renderer)
{
	auto &game = this->getGame();
	const auto &fontLibrary = game.getFontLibrary();

	const Int2 &point = OptionsUiView::DescriptionOrigin;
	const TextBox::InitInfo textBoxInitInfo = TextBox::InitInfo::makeWithXY(
		text,
		point.x,
		point.y,
		OptionsUiView::DescriptionTextFontName,
		OptionsUiView::getDescriptionTextColor(),
		OptionsUiView::DescriptionTextAlignment,
		fontLibrary);

	TextBox descriptionTextBox;
	if (!descriptionTextBox.init(textBoxInitInfo, text, renderer))
	{
		DebugCrash("Couldn't init description text box.");
	}

	const Rect &descriptionTextBoxRect = descriptionTextBox.getRect();
	renderer.drawOriginal(descriptionTextBox.getTextureID(),
		descriptionTextBoxRect.getLeft(), descriptionTextBoxRect.getTop());
}

void OptionsPanel::render(Renderer &renderer)
{
	// Clear full screen.
	renderer.clear();

	// Draw solid background.
	renderer.clearOriginal(OptionsUiView::BackgroundColor);

	// Draw elements.
	this->drawReturnButtonsAndTabs(renderer);
	this->drawText(renderer);
	this->drawTextOfOptions(renderer);
}
