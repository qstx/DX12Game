#pragma once
#include "Game.h"

namespace Library
{
	class RenderingGame : public Game
	{
	public:
		RenderingGame(HINSTANCE instance, const std::wstring& windowClass, const std::wstring& windowTitle, int showCommand);
		~RenderingGame();

		virtual void Initialize() override;
		virtual void Update(const GameTimer& gt) override;
		virtual void Draw() override;
		/*virtual Camera* GetCamera() override;
		virtual Scene* GetScene() override;*/

	protected:
		virtual void Shutdown() override;

	private:
		static const XMFLOAT4 BackgroundColor;
		//FirstPersonCamera* mCamera;
		LPDIRECTINPUT8 mDirectInput;
		//Keyboard* mKeyboard;
		//Mouse* mMouse;

		//FpsComponent* mFpsComponent;
		//RenderStateHelper* mRenderStateHelper;
		//Scene* mScene;
	};
}