#include "SDL.h"

#include "CharacterCreationUiController.h"
#include "CharacterCreationUiModel.h"
#include "CharacterCreationUiView.h"
#include "ChooseRacePanel.h"
#include "TextSubPanel.h"
#include "WorldMapUiModel.h"
#include "../Game/Game.h"
#include "../Input/InputActionName.h"
#include "../UI/CursorData.h"
#include "../UI/Surface.h"

ChooseRacePanel::ChooseRacePanel(Game &game)
	: Panel(game) { }

bool ChooseRacePanel::init()
{
	auto &game = this->getGame();

	this->addInputActionListener(InputActionName::Back, ChooseRaceUiController::onBackToChooseGenderInputAction);
	this->addMouseButtonChangedListener(ChooseRaceUiController::onMouseButtonChanged);

	// Push the initial text sub-panel.
	// @todo: allocate std::function for unravelling the map with "push initial parchment sub-panel" on finished,
	// setting the std::function to empty at that time?
	std::unique_ptr<Panel> textSubPanel = ChooseRacePanel::getInitialSubPanel(game);
	game.pushSubPanel(std::move(textSubPanel));

	return true;
}

std::unique_ptr<Panel> ChooseRacePanel::getInitialSubPanel(Game &game)
{
	const auto &fontLibrary = game.getFontLibrary();
	const std::string text = ChooseRaceUiModel::getTitleText(game);
	const TextBox::InitInfo textBoxInitInfo = TextBox::InitInfo::makeWithCenter(
		text,
		ChooseRaceUiView::InitialPopUpTextCenterPoint,
		ChooseRaceUiView::InitialPopUpFontName,
		ChooseRaceUiView::InitialPopUpColor,
		ChooseRaceUiView::InitialPopUpAlignment,
		std::nullopt,
		ChooseRaceUiView::InitialPopUpLineSpacing,
		fontLibrary);

	auto &renderer = game.getRenderer();
	Surface surface = TextureUtils::generate(
		ChooseRaceUiView::InitialPopUpPatternType,
		ChooseRaceUiView::InitialPopUpTextureWidth,
		ChooseRaceUiView::InitialPopUpTextureHeight,
		game.getTextureManager(),
		renderer);
	Texture texture = renderer.createTextureFromSurface(surface);

	std::unique_ptr<TextSubPanel> subPanel = std::make_unique<TextSubPanel>(game);
	if (!subPanel->init(textBoxInitInfo, text, ChooseRaceUiController::onInitialPopUpButtonSelected,
		std::move(texture), ChooseRaceUiView::InitialPopUpTextureCenterPoint))
	{
		DebugCrash("Couldn't init choose race initial sub-panel.");
	}

	return subPanel;
}

std::optional<CursorData> ChooseRacePanel::getCurrentCursor() const
{
	return this->getDefaultCursor();
}

void ChooseRacePanel::drawProvinceTooltip(int provinceID, Renderer &renderer)
{
	auto &game = this->getGame();
	const Texture tooltip = TextureUtils::createTooltip(
		ChooseRaceUiModel::getProvinceTooltipText(game, provinceID),
		this->getGame().getFontLibrary(),
		renderer);

	const auto &inputManager = game.getInputManager();
	const Int2 mousePosition = inputManager.getMousePosition();
	const Int2 originalPosition = renderer.nativeToOriginal(mousePosition);
	const int mouseX = originalPosition.x;
	const int mouseY = originalPosition.y;
	const int x = ((mouseX + 8 + tooltip.getWidth()) < ArenaRenderUtils::SCREEN_WIDTH) ?
		(mouseX + 8) : (mouseX - tooltip.getWidth());
	const int y = ((mouseY + tooltip.getHeight()) < ArenaRenderUtils::SCREEN_HEIGHT) ?
		mouseY : (mouseY - tooltip.getHeight());

	renderer.drawOriginal(tooltip, x, y);
}

void ChooseRacePanel::render(Renderer &renderer)
{
	// Clear full screen.
	renderer.clear();

	// Draw background map.
	auto &textureManager = this->getGame().getTextureManager();
	const TextureAssetReference backgroundTextureAssetRef = ChooseRaceUiView::getBackgroundTextureAssetRef();
	const std::optional<PaletteID> backgroundPaletteID = textureManager.tryGetPaletteID(backgroundTextureAssetRef);
	if (!backgroundPaletteID.has_value())
	{
		DebugLogError("Couldn't get race select palette ID for \"" + backgroundTextureAssetRef.filename + "\".");
		return;
	}

	const std::optional<TextureBuilderID> backgroundTextureBuilderID = textureManager.tryGetTextureBuilderID(backgroundTextureAssetRef);
	if (!backgroundTextureBuilderID.has_value())
	{
		DebugLogError("Couldn't get race select texture builder ID for \"" + backgroundTextureAssetRef.filename + "\".");
		return;
	}

	renderer.drawOriginal(*backgroundTextureBuilderID, *backgroundPaletteID, textureManager);

	// Cover up the "exit" text at the bottom right.
	const TextureAssetReference noExitTextureAssetRef = ChooseRaceUiView::getNoExitTextureAssetRef();
	const std::optional<TextureBuilderID> noExitTextureBuilderID = textureManager.tryGetTextureBuilderID(noExitTextureAssetRef);
	if (!noExitTextureBuilderID.has_value())
	{
		DebugLogError("Couldn't get exit cover texture builder ID for \"" + noExitTextureAssetRef.filename + "\".");
		return;
	}

	const TextureBuilder &noExitTextureBuilder = textureManager.getTextureBuilderHandle(*noExitTextureBuilderID);
	const int exitCoverX = ChooseRaceUiView::getNoExitTextureX(noExitTextureBuilder.getWidth());
	const int exitCoverY = ChooseRaceUiView::getNoExitTextureY(noExitTextureBuilder.getHeight());
	renderer.drawOriginal(*noExitTextureBuilderID, *backgroundPaletteID, exitCoverX, exitCoverY, textureManager);
}

void ChooseRacePanel::renderSecondary(Renderer &renderer)
{
	auto &game = this->getGame();
	const auto &inputManager = game.getInputManager();
	const Int2 mousePosition = inputManager.getMousePosition();

	// Draw tooltip if the mouse is in a province.
	const std::optional<int> provinceID = WorldMapUiModel::getMaskID(game, mousePosition, true, true);
	if (provinceID.has_value())
	{
		this->drawProvinceTooltip(*provinceID, renderer);
	}
}
