#include "input.h"
#include "strings.h"
#include <math.h>
#include "lmath.h"
#include "ease.h"
#include "settings.h"
#include "render/ui.h"

namespace VI
{

namespace Input
{

const char* key_strings[(s32)KeyCode::Count];
const char* btn_strings[(s32)Gamepad::Btn::Count];

void load_strings()
{
	key_strings[(s32)KeyCode::None] = _(strings::key_None);
	key_strings[(s32)KeyCode::Return] = _(strings::key_Return);
	key_strings[(s32)KeyCode::Escape] = _(strings::key_Escape);
	key_strings[(s32)KeyCode::Backspace] = _(strings::key_Backspace);
	key_strings[(s32)KeyCode::Tab] = _(strings::key_Tab);
	key_strings[(s32)KeyCode::Space] = _(strings::key_Space);
	key_strings[(s32)KeyCode::Comma] = _(strings::key_Comma);
	key_strings[(s32)KeyCode::Minus] = _(strings::key_Minus);
	key_strings[(s32)KeyCode::Period] = _(strings::key_Period);
	key_strings[(s32)KeyCode::Slash] = _(strings::key_Slash);
	key_strings[(s32)KeyCode::D0] = _(strings::key_D0);
	key_strings[(s32)KeyCode::D1] = _(strings::key_D1);
	key_strings[(s32)KeyCode::D2] = _(strings::key_D2);
	key_strings[(s32)KeyCode::D3] = _(strings::key_D3);
	key_strings[(s32)KeyCode::D4] = _(strings::key_D4);
	key_strings[(s32)KeyCode::D5] = _(strings::key_D5);
	key_strings[(s32)KeyCode::D6] = _(strings::key_D6);
	key_strings[(s32)KeyCode::D7] = _(strings::key_D7);
	key_strings[(s32)KeyCode::D8] = _(strings::key_D8);
	key_strings[(s32)KeyCode::D9] = _(strings::key_D9);
	key_strings[(s32)KeyCode::Semicolon] = _(strings::key_Semicolon);
	key_strings[(s32)KeyCode::Apostrophe] = _(strings::key_Apostrophe);
	key_strings[(s32)KeyCode::Equals] = _(strings::key_Equals);
	key_strings[(s32)KeyCode::LeftBracket] = _(strings::key_LeftBracket);
	key_strings[(s32)KeyCode::Backslash] = _(strings::key_Backslash);
	key_strings[(s32)KeyCode::RightBracket] = _(strings::key_RightBracket);
	key_strings[(s32)KeyCode::Grave] = _(strings::key_Grave);
	key_strings[(s32)KeyCode::A] = _(strings::key_A);
	key_strings[(s32)KeyCode::B] = _(strings::key_B);
	key_strings[(s32)KeyCode::C] = _(strings::key_C);
	key_strings[(s32)KeyCode::D] = _(strings::key_D);
	key_strings[(s32)KeyCode::E] = _(strings::key_E);
	key_strings[(s32)KeyCode::F] = _(strings::key_F);
	key_strings[(s32)KeyCode::G] = _(strings::key_G);
	key_strings[(s32)KeyCode::H] = _(strings::key_H);
	key_strings[(s32)KeyCode::I] = _(strings::key_I);
	key_strings[(s32)KeyCode::J] = _(strings::key_J);
	key_strings[(s32)KeyCode::K] = _(strings::key_K);
	key_strings[(s32)KeyCode::L] = _(strings::key_L);
	key_strings[(s32)KeyCode::M] = _(strings::key_M);
	key_strings[(s32)KeyCode::N] = _(strings::key_N);
	key_strings[(s32)KeyCode::O] = _(strings::key_O);
	key_strings[(s32)KeyCode::P] = _(strings::key_P);
	key_strings[(s32)KeyCode::Q] = _(strings::key_Q);
	key_strings[(s32)KeyCode::R] = _(strings::key_R);
	key_strings[(s32)KeyCode::S] = _(strings::key_S);
	key_strings[(s32)KeyCode::T] = _(strings::key_T);
	key_strings[(s32)KeyCode::U] = _(strings::key_U);
	key_strings[(s32)KeyCode::V] = _(strings::key_V);
	key_strings[(s32)KeyCode::W] = _(strings::key_W);
	key_strings[(s32)KeyCode::X] = _(strings::key_X);
	key_strings[(s32)KeyCode::Y] = _(strings::key_Y);
	key_strings[(s32)KeyCode::Z] = _(strings::key_Z);
	key_strings[(s32)KeyCode::Capslock] = _(strings::key_Capslock);
	key_strings[(s32)KeyCode::F1] = _(strings::key_F1);
	key_strings[(s32)KeyCode::F2] = _(strings::key_F2);
	key_strings[(s32)KeyCode::F3] = _(strings::key_F3);
	key_strings[(s32)KeyCode::F4] = _(strings::key_F4);
	key_strings[(s32)KeyCode::F5] = _(strings::key_F5);
	key_strings[(s32)KeyCode::F6] = _(strings::key_F6);
	key_strings[(s32)KeyCode::F7] = _(strings::key_F7);
	key_strings[(s32)KeyCode::F8] = _(strings::key_F8);
	key_strings[(s32)KeyCode::F9] = _(strings::key_F9);
	key_strings[(s32)KeyCode::F10] = _(strings::key_F10);
	key_strings[(s32)KeyCode::F11] = _(strings::key_F11);
	key_strings[(s32)KeyCode::F12] = _(strings::key_F12);
	key_strings[(s32)KeyCode::Printscreen] = _(strings::key_Printscreen);
	key_strings[(s32)KeyCode::Scrolllock] = _(strings::key_Scrolllock);
	key_strings[(s32)KeyCode::Pause] = _(strings::key_Pause);
	key_strings[(s32)KeyCode::Insert] = _(strings::key_Insert);
	key_strings[(s32)KeyCode::Home] = _(strings::key_Home);
	key_strings[(s32)KeyCode::PageUp] = _(strings::key_PageUp);
	key_strings[(s32)KeyCode::Delete] = _(strings::key_Delete);
	key_strings[(s32)KeyCode::End] = _(strings::key_End);
	key_strings[(s32)KeyCode::PageDown] = _(strings::key_PageDown);
	key_strings[(s32)KeyCode::Right] = _(strings::key_Right);
	key_strings[(s32)KeyCode::Left] = _(strings::key_Left);
	key_strings[(s32)KeyCode::Down] = _(strings::key_Down);
	key_strings[(s32)KeyCode::Up] = _(strings::key_Up);
	key_strings[(s32)KeyCode::NumlockClear] = _(strings::key_NumlockClear);
	key_strings[(s32)KeyCode::KeypadDivide] = _(strings::key_KeypadDivide);
	key_strings[(s32)KeyCode::KeypadMultiply] = _(strings::key_KeypadMultiply);
	key_strings[(s32)KeyCode::KeypadMinus] = _(strings::key_KeypadMinus);
	key_strings[(s32)KeyCode::KeypadPlus] = _(strings::key_KeypadPlus);
	key_strings[(s32)KeyCode::KeypadEnter] = _(strings::key_KeypadEnter);
	key_strings[(s32)KeyCode::Keypad1] = _(strings::key_Keypad1);
	key_strings[(s32)KeyCode::Keypad2] = _(strings::key_Keypad2);
	key_strings[(s32)KeyCode::Keypad3] = _(strings::key_Keypad3);
	key_strings[(s32)KeyCode::Keypad4] = _(strings::key_Keypad4);
	key_strings[(s32)KeyCode::Keypad5] = _(strings::key_Keypad5);
	key_strings[(s32)KeyCode::Keypad6] = _(strings::key_Keypad6);
	key_strings[(s32)KeyCode::Keypad7] = _(strings::key_Keypad7);
	key_strings[(s32)KeyCode::Keypad8] = _(strings::key_Keypad8);
	key_strings[(s32)KeyCode::Keypad9] = _(strings::key_Keypad9);
	key_strings[(s32)KeyCode::Keypad0] = _(strings::key_Keypad0);
	key_strings[(s32)KeyCode::KeypadPeriod] = _(strings::key_KeypadPeriod);
	key_strings[(s32)KeyCode::Application] = _(strings::key_Application);
	key_strings[(s32)KeyCode::Power] = _(strings::key_Power);
	key_strings[(s32)KeyCode::KeypadEquals] = _(strings::key_KeypadEquals);
	key_strings[(s32)KeyCode::F13] = _(strings::key_F13);
	key_strings[(s32)KeyCode::F14] = _(strings::key_F14);
	key_strings[(s32)KeyCode::F15] = _(strings::key_F15);
	key_strings[(s32)KeyCode::F16] = _(strings::key_F16);
	key_strings[(s32)KeyCode::F17] = _(strings::key_F17);
	key_strings[(s32)KeyCode::F18] = _(strings::key_F18);
	key_strings[(s32)KeyCode::F19] = _(strings::key_F19);
	key_strings[(s32)KeyCode::F20] = _(strings::key_F20);
	key_strings[(s32)KeyCode::F21] = _(strings::key_F21);
	key_strings[(s32)KeyCode::F22] = _(strings::key_F22);
	key_strings[(s32)KeyCode::F23] = _(strings::key_F23);
	key_strings[(s32)KeyCode::F24] = _(strings::key_F24);
	key_strings[(s32)KeyCode::Execute] = _(strings::key_Execute);
	key_strings[(s32)KeyCode::Help] = _(strings::key_Help);
	key_strings[(s32)KeyCode::Menu] = _(strings::key_Menu);
	key_strings[(s32)KeyCode::Select] = _(strings::key_Select);
	key_strings[(s32)KeyCode::Stop] = _(strings::key_Stop);
	key_strings[(s32)KeyCode::Again] = _(strings::key_Again);
	key_strings[(s32)KeyCode::Undo] = _(strings::key_Undo);
	key_strings[(s32)KeyCode::Cut] = _(strings::key_Cut);
	key_strings[(s32)KeyCode::Copy] = _(strings::key_Copy);
	key_strings[(s32)KeyCode::Paste] = _(strings::key_Paste);
	key_strings[(s32)KeyCode::Find] = _(strings::key_Find);
	key_strings[(s32)KeyCode::Mute] = _(strings::key_Mute);
	key_strings[(s32)KeyCode::VolumeUp] = _(strings::key_VolumeUp);
	key_strings[(s32)KeyCode::VolumeDown] = _(strings::key_VolumeDown);
	key_strings[(s32)KeyCode::KeypadComma] = _(strings::key_KeypadComma);
	key_strings[(s32)KeyCode::KeypadEqualsAS400] = _(strings::key_KeypadEqualsAS400);
	key_strings[(s32)KeyCode::AltErase] = _(strings::key_AltErase);
	key_strings[(s32)KeyCode::SysReq] = _(strings::key_SysReq);
	key_strings[(s32)KeyCode::Cancel] = _(strings::key_Cancel);
	key_strings[(s32)KeyCode::Clear] = _(strings::key_Clear);
	key_strings[(s32)KeyCode::Prior] = _(strings::key_Prior);
	key_strings[(s32)KeyCode::Return2] = _(strings::key_Return2);
	key_strings[(s32)KeyCode::Separator] = _(strings::key_Separator);
	key_strings[(s32)KeyCode::Out] = _(strings::key_Out);
	key_strings[(s32)KeyCode::Oper] = _(strings::key_Oper);
	key_strings[(s32)KeyCode::ClearAgain] = _(strings::key_ClearAgain);
	key_strings[(s32)KeyCode::CrSel] = _(strings::key_CrSel);
	key_strings[(s32)KeyCode::ExSel] = _(strings::key_ExSel);
	key_strings[(s32)KeyCode::Keypad00] = _(strings::key_Keypad00);
	key_strings[(s32)KeyCode::Keypad000] = _(strings::key_Keypad000);
	key_strings[(s32)KeyCode::ThousandsSeparator] = _(strings::key_ThousandsSeparator);
	key_strings[(s32)KeyCode::DecimalSeparator] = _(strings::key_DecimalSeparator);
	key_strings[(s32)KeyCode::CurrencyUnit] = _(strings::key_CurrencyUnit);
	key_strings[(s32)KeyCode::CurrencySubunit] = _(strings::key_CurrencySubunit);
	key_strings[(s32)KeyCode::KeypadLeftParen] = _(strings::key_KeypadLeftParen);
	key_strings[(s32)KeyCode::KeypadRightParen] = _(strings::key_KeypadRightParen);
	key_strings[(s32)KeyCode::KeypadLeftBrace] = _(strings::key_KeypadLeftBrace);
	key_strings[(s32)KeyCode::KeypadRightBrace] = _(strings::key_KeypadRightBrace);
	key_strings[(s32)KeyCode::KeypadTab] = _(strings::key_KeypadTab);
	key_strings[(s32)KeyCode::KeypadBackspace] = _(strings::key_KeypadBackspace);
	key_strings[(s32)KeyCode::KeypadA] = _(strings::key_KeypadA);
	key_strings[(s32)KeyCode::KeypadB] = _(strings::key_KeypadB);
	key_strings[(s32)KeyCode::KeypadC] = _(strings::key_KeypadC);
	key_strings[(s32)KeyCode::KeypadD] = _(strings::key_KeypadD);
	key_strings[(s32)KeyCode::KeypadE] = _(strings::key_KeypadE);
	key_strings[(s32)KeyCode::KeypadF] = _(strings::key_KeypadF);
	key_strings[(s32)KeyCode::KeypadXor] = _(strings::key_KeypadXor);
	key_strings[(s32)KeyCode::KeypadPower] = _(strings::key_KeypadPower);
	key_strings[(s32)KeyCode::KeypadPercent] = _(strings::key_KeypadPercent);
	key_strings[(s32)KeyCode::KeypadLess] = _(strings::key_KeypadLess);
	key_strings[(s32)KeyCode::KeypadGreater] = _(strings::key_KeypadGreater);
	key_strings[(s32)KeyCode::KeypadAmpersand] = _(strings::key_KeypadAmpersand);
	key_strings[(s32)KeyCode::KeypadDblAmpersand] = _(strings::key_KeypadDblAmpersand);
	key_strings[(s32)KeyCode::KeypadVerticalBar] = _(strings::key_KeypadVerticalBar);
	key_strings[(s32)KeyCode::KeypadDblVerticalBar] = _(strings::key_KeypadDblVerticalBar);
	key_strings[(s32)KeyCode::KeypadColon] = _(strings::key_KeypadColon);
	key_strings[(s32)KeyCode::KeypadHash] = _(strings::key_KeypadHash);
	key_strings[(s32)KeyCode::KeypadSpace] = _(strings::key_KeypadSpace);
	key_strings[(s32)KeyCode::KeypadAt] = _(strings::key_KeypadAt);
	key_strings[(s32)KeyCode::KeypadExclam] = _(strings::key_KeypadExclam);
	key_strings[(s32)KeyCode::KeypadMemStore] = _(strings::key_KeypadMemStore);
	key_strings[(s32)KeyCode::KeypadMemRecall] = _(strings::key_KeypadMemRecall);
	key_strings[(s32)KeyCode::KeypadMemClear] = _(strings::key_KeypadMemClear);
	key_strings[(s32)KeyCode::KeypadMemAdd] = _(strings::key_KeypadMemAdd);
	key_strings[(s32)KeyCode::KeypadMemSubtract] = _(strings::key_KeypadMemSubtract);
	key_strings[(s32)KeyCode::KeypadMemMultiply] = _(strings::key_KeypadMemMultiply);
	key_strings[(s32)KeyCode::KeypadMemDivide] = _(strings::key_KeypadMemDivide);
	key_strings[(s32)KeyCode::KeypadPlusMinus] = _(strings::key_KeypadPlusMinus);
	key_strings[(s32)KeyCode::KeypadClear] = _(strings::key_KeypadClear);
	key_strings[(s32)KeyCode::KeypadClearEntry] = _(strings::key_KeypadClearEntry);
	key_strings[(s32)KeyCode::KeypadBinary] = _(strings::key_KeypadBinary);
	key_strings[(s32)KeyCode::KeypadOctal] = _(strings::key_KeypadOctal);
	key_strings[(s32)KeyCode::KeypadDecimal] = _(strings::key_KeypadDecimal);
	key_strings[(s32)KeyCode::KeypadHexadecimal] = _(strings::key_KeypadHexadecimal);
	key_strings[(s32)KeyCode::LCtrl] = _(strings::key_LCtrl);
	key_strings[(s32)KeyCode::LShift] = _(strings::key_LShift);
	key_strings[(s32)KeyCode::LAlt] = _(strings::key_LAlt);
	key_strings[(s32)KeyCode::LGui] = _(strings::key_LGui);
	key_strings[(s32)KeyCode::RCtrl] = _(strings::key_RCtrl);
	key_strings[(s32)KeyCode::RShift] = _(strings::key_RShift);
	key_strings[(s32)KeyCode::RAlt] = _(strings::key_RAlt);
	key_strings[(s32)KeyCode::RGui] = _(strings::key_RGui);
	key_strings[(s32)KeyCode::Mode] = _(strings::key_Mode);
	key_strings[(s32)KeyCode::AudioNext] = _(strings::key_AudioNext);
	key_strings[(s32)KeyCode::AudioPrev] = _(strings::key_AudioPrev);
	key_strings[(s32)KeyCode::AudioStop] = _(strings::key_AudioStop);
	key_strings[(s32)KeyCode::AudioPlay] = _(strings::key_AudioPlay);
	key_strings[(s32)KeyCode::AudioMute] = _(strings::key_AudioMute);
	key_strings[(s32)KeyCode::MediaSelect] = _(strings::key_MediaSelect);
	key_strings[(s32)KeyCode::Www] = _(strings::key_Www);
	key_strings[(s32)KeyCode::Mail] = _(strings::key_Mail);
	key_strings[(s32)KeyCode::Calculator] = _(strings::key_Calculator);
	key_strings[(s32)KeyCode::Computer] = _(strings::key_Computer);
	key_strings[(s32)KeyCode::ACSearch] = _(strings::key_ACSearch);
	key_strings[(s32)KeyCode::ACHome] = _(strings::key_ACHome);
	key_strings[(s32)KeyCode::ACBack] = _(strings::key_ACBack);
	key_strings[(s32)KeyCode::ACForward] = _(strings::key_ACForward);
	key_strings[(s32)KeyCode::ACStop] = _(strings::key_ACStop);
	key_strings[(s32)KeyCode::ACRefresh] = _(strings::key_ACRefresh);
	key_strings[(s32)KeyCode::ACBookmarks] = _(strings::key_ACBookmarks);
	key_strings[(s32)KeyCode::BrightnessDown] = _(strings::key_BrightnessDown);
	key_strings[(s32)KeyCode::BrightnessUp] = _(strings::key_BrightnessUp);
	key_strings[(s32)KeyCode::DisplaySwitch] = _(strings::key_DisplaySwitch);
	key_strings[(s32)KeyCode::KbDillumToggle] = _(strings::key_KbDillumToggle);
	key_strings[(s32)KeyCode::KbDillumDown] = _(strings::key_KbDillumDown);
	key_strings[(s32)KeyCode::KbDillumUp] = _(strings::key_KbDillumUp);
	key_strings[(s32)KeyCode::Eject] = _(strings::key_Eject);
	key_strings[(s32)KeyCode::Sleep] = _(strings::key_Sleep);
	key_strings[(s32)KeyCode::MouseLeft] = _(strings::key_MouseLeft);
	key_strings[(s32)KeyCode::MouseRight] = _(strings::key_MouseRight);
	key_strings[(s32)KeyCode::MouseMiddle] = _(strings::key_MouseMiddle);

	btn_strings[(s32)Gamepad::Btn::LeftShoulder] = _(strings::btn_LeftShoulder);
	btn_strings[(s32)Gamepad::Btn::RightShoulder] = _(strings::btn_RightShoulder);
	btn_strings[(s32)Gamepad::Btn::LeftClick] = _(strings::btn_LeftClick);
	btn_strings[(s32)Gamepad::Btn::RightClick] = _(strings::btn_RightClick);
	btn_strings[(s32)Gamepad::Btn::A] = _(strings::btn_A);
	btn_strings[(s32)Gamepad::Btn::B] = _(strings::btn_B);
	btn_strings[(s32)Gamepad::Btn::X] = _(strings::btn_X);
	btn_strings[(s32)Gamepad::Btn::Y] = _(strings::btn_Y);
	btn_strings[(s32)Gamepad::Btn::Back] = _(strings::btn_Back);
	btn_strings[(s32)Gamepad::Btn::Start] = _(strings::btn_Start);
	btn_strings[(s32)Gamepad::Btn::LeftTrigger] = _(strings::btn_LeftTrigger);
	btn_strings[(s32)Gamepad::Btn::RightTrigger] = _(strings::btn_RightTrigger);
	btn_strings[(s32)Gamepad::Btn::None] = _(strings::btn_None);
}

void dead_zone(r32* x, r32* y, r32 threshold)
{
	Vec2 p(*x, *y);
	r32 length = p.length();
	if (length < threshold)
	{
		*x = 0.0f;
		*y = 0.0f;
	}
	else
	{
		p *= Ease::quad_in((vi_min(length, 1.0f) - threshold) / (1.0f - threshold), 0.0f, 1.0f) / length;
		*x = p.x;
		*y = p.y;
	}
}

r32 dead_zone(r32 x)
{
	if (fabs(x) < UI_JOYSTICK_DEAD_ZONE)
		return 0.0f;
	else
		return (x - UI_JOYSTICK_DEAD_ZONE) / (1.0f - UI_JOYSTICK_DEAD_ZONE);
}

}

const char* InputBinding::string(b8 gamepad) const
{
	if (gamepad)
		return Input::btn_strings[(s32)btn];
	else
		return Input::key_strings[(s32)key1];
}

b8 InputBinding::overlaps(const InputBinding& other) const
{
	return btn == other.btn || (key1 != KeyCode::None && key1 == other.key1) || (key2 != KeyCode::None && key2 == other.key2);
}

b8 InputState::get(Controls c, s32 gamepad) const
{
	const InputBinding& binding = Settings::gamepads[gamepad].bindings[(s32)c];
	return (gamepad == 0 && (keys[(s32)binding.key1] || (binding.key2 != KeyCode::None && keys[(s32)binding.key2])))
		|| (gamepads[gamepad].btns & (s32)binding.btn);
}


}