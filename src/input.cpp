#include "localization.h"
#include "input.h"
#include <math.h>
#include "lmath.h"
#include "ease.h"
#include "settings.h"
#include "render/ui.h"
#include "data/unicode.h"

namespace VI
{

namespace Input
{

const char* key_strings[s32(KeyCode::count)];
const char* btn_strings_xbox[s32(Gamepad::Btn::count)];
const char* btn_strings_playstation[s32(Gamepad::Btn::count)];
const char* control_strings[s32(Controls::count)];

const char* control_setting_names[s32(Controls::count)] =
{
	"forward",
	"backward",
	"left",
	"right",
	"primary",
	"zoom",
	"ability1",
	"ability2",
	"ability3",
	nullptr, // Start; can't be modified
	nullptr, // Cancel; can't be modified
	nullptr, // Pause; can't be modified
	nullptr, // Interact; can't be modified
	"interact",
	"scoreboard",
	"jump",
	"parkour",
	"grapple",
	"grapple_cancel",
	"ui_context_action",
	nullptr, // UIAcceptText; can't be modified
	"tab_left",
	"tab_right",
	"emote1",
	"emote2",
	"emote3",
	"emote4",
	"chat_team",
	"chat_all",
	"spot",
	"console",
};

const char* control_ui_variable_names[s32(Controls::count)] =
{
	nullptr, // Forward
	nullptr, // Backward
	nullptr, // Left
	nullptr, // Right
	"Primary",
	"Zoom",
	"Ability1",
	"Ability2",
	"Ability3",
	"Start",
	"Cancel",
	nullptr, // Pause
	"Interact",
	"InteractSecondary",
	"Scoreboard",
	"Jump",
	"Parkour",
	"Grapple",
	"GrappleCancel",
	"UIContextAction",
	"UIAcceptText",
	"TabLeft",
	"TabRight",
	"Emote1",
	"Emote2",
	"Emote3",
	"Emote4",
	"ChatTeam",
	"ChatAll",
	"Spot",
	"Console",
};

InputBinding control_defaults[s32(Controls::count)] =
{
	{ Gamepad::Btn::None, KeyCode::W, KeyCode::Up, }, // Forward
	{ Gamepad::Btn::None, KeyCode::S, KeyCode::Down, }, // Backward
	{ Gamepad::Btn::None, KeyCode::A, KeyCode::Left, }, // Left
	{ Gamepad::Btn::None, KeyCode::D, KeyCode::Right, }, // Right
	{ Gamepad::Btn::RightTrigger, KeyCode::MouseLeft, KeyCode::None, }, // Primary
	{ Gamepad::Btn::LeftTrigger, KeyCode::MouseRight, KeyCode::None, }, // Zoom
	{ Gamepad::Btn::X, KeyCode::Q, KeyCode::None, }, // Ability1
	{ Gamepad::Btn::Y, KeyCode::E, KeyCode::None, }, // Ability2
	{ Gamepad::Btn::B, KeyCode::R, KeyCode::None, }, // Ability3
	{ Gamepad::Btn::Start, KeyCode::Return, KeyCode::None, }, // Start
	{ Gamepad::Btn::B, KeyCode::Escape, KeyCode::None, }, // Cancel
	{ Gamepad::Btn::Start, KeyCode::Escape, KeyCode::None, }, // Pause
	{ Gamepad::Btn::A, KeyCode::Space, KeyCode::Return, }, // Interact
	{ Gamepad::Btn::A, KeyCode::F, KeyCode::None, }, // InteractSecondary
	{ Gamepad::Btn::Back, KeyCode::Tab, KeyCode::None, }, // Scoreboard
	{ Gamepad::Btn::RightTrigger, KeyCode::Space, KeyCode::None, }, // Jump
	{ Gamepad::Btn::LeftTrigger, KeyCode::LShift, KeyCode::None, }, // Parkour
	{ Gamepad::Btn::RightShoulder, KeyCode::MouseLeft, KeyCode::None, }, // Grapple
	{ Gamepad::Btn::B, KeyCode::MouseRight, KeyCode::None, }, // GrappleCancel
	{ Gamepad::Btn::X, KeyCode::F, KeyCode::None, }, // UIContextAction
	{ Gamepad::Btn::A, KeyCode::Return, KeyCode::KeypadEnter, }, // UIAcceptText
	{ Gamepad::Btn::LeftShoulder, KeyCode::Q, KeyCode::None, }, // TabLeft
	{ Gamepad::Btn::RightShoulder, KeyCode::E, KeyCode::None, }, // TabRight
	{ Gamepad::Btn::DLeft, KeyCode::F1, KeyCode::None, }, // Emote1
	{ Gamepad::Btn::DUp, KeyCode::F2, KeyCode::None, }, // Emote2
	{ Gamepad::Btn::DRight, KeyCode::F3, KeyCode::None, }, // Emote3
	{ Gamepad::Btn::DDown, KeyCode::F4, KeyCode::None, }, // Emote4
	{ Gamepad::Btn::None, KeyCode::T, KeyCode::None, }, // ChatTeam
	{ Gamepad::Btn::None, KeyCode::Y, KeyCode::None, }, // ChatAll
	{ Gamepad::Btn::LeftShoulder, KeyCode::F, KeyCode::None, }, // Spot
	{ Gamepad::Btn::None, KeyCode::Grave, KeyCode::None, }, // Console
};

void init()
{
	key_strings[s32(KeyCode::None)] = _(strings::key_None);
	key_strings[s32(KeyCode::Return)] = _(strings::key_Return);
	key_strings[s32(KeyCode::Escape)] = _(strings::key_Escape);
	key_strings[s32(KeyCode::Backspace)] = _(strings::key_Backspace);
	key_strings[s32(KeyCode::Tab)] = _(strings::key_Tab);
	key_strings[s32(KeyCode::Space)] = _(strings::key_Space);
	key_strings[s32(KeyCode::Comma)] = _(strings::key_Comma);
	key_strings[s32(KeyCode::Minus)] = _(strings::key_Minus);
	key_strings[s32(KeyCode::Period)] = _(strings::key_Period);
	key_strings[s32(KeyCode::Slash)] = _(strings::key_Slash);
	key_strings[s32(KeyCode::D0)] = _(strings::key_D0);
	key_strings[s32(KeyCode::D1)] = _(strings::key_D1);
	key_strings[s32(KeyCode::D2)] = _(strings::key_D2);
	key_strings[s32(KeyCode::D3)] = _(strings::key_D3);
	key_strings[s32(KeyCode::D4)] = _(strings::key_D4);
	key_strings[s32(KeyCode::D5)] = _(strings::key_D5);
	key_strings[s32(KeyCode::D6)] = _(strings::key_D6);
	key_strings[s32(KeyCode::D7)] = _(strings::key_D7);
	key_strings[s32(KeyCode::D8)] = _(strings::key_D8);
	key_strings[s32(KeyCode::D9)] = _(strings::key_D9);
	key_strings[s32(KeyCode::Semicolon)] = _(strings::key_Semicolon);
	key_strings[s32(KeyCode::Apostrophe)] = _(strings::key_Apostrophe);
	key_strings[s32(KeyCode::Equals)] = _(strings::key_Equals);
	key_strings[s32(KeyCode::LeftBracket)] = _(strings::key_LeftBracket);
	key_strings[s32(KeyCode::Backslash)] = _(strings::key_Backslash);
	key_strings[s32(KeyCode::RightBracket)] = _(strings::key_RightBracket);
	key_strings[s32(KeyCode::Grave)] = _(strings::key_Grave);
	key_strings[s32(KeyCode::A)] = _(strings::key_A);
	key_strings[s32(KeyCode::B)] = _(strings::key_B);
	key_strings[s32(KeyCode::C)] = _(strings::key_C);
	key_strings[s32(KeyCode::D)] = _(strings::key_D);
	key_strings[s32(KeyCode::E)] = _(strings::key_E);
	key_strings[s32(KeyCode::F)] = _(strings::key_F);
	key_strings[s32(KeyCode::G)] = _(strings::key_G);
	key_strings[s32(KeyCode::H)] = _(strings::key_H);
	key_strings[s32(KeyCode::I)] = _(strings::key_I);
	key_strings[s32(KeyCode::J)] = _(strings::key_J);
	key_strings[s32(KeyCode::K)] = _(strings::key_K);
	key_strings[s32(KeyCode::L)] = _(strings::key_L);
	key_strings[s32(KeyCode::M)] = _(strings::key_M);
	key_strings[s32(KeyCode::N)] = _(strings::key_N);
	key_strings[s32(KeyCode::O)] = _(strings::key_O);
	key_strings[s32(KeyCode::P)] = _(strings::key_P);
	key_strings[s32(KeyCode::Q)] = _(strings::key_Q);
	key_strings[s32(KeyCode::R)] = _(strings::key_R);
	key_strings[s32(KeyCode::S)] = _(strings::key_S);
	key_strings[s32(KeyCode::T)] = _(strings::key_T);
	key_strings[s32(KeyCode::U)] = _(strings::key_U);
	key_strings[s32(KeyCode::V)] = _(strings::key_V);
	key_strings[s32(KeyCode::W)] = _(strings::key_W);
	key_strings[s32(KeyCode::X)] = _(strings::key_X);
	key_strings[s32(KeyCode::Y)] = _(strings::key_Y);
	key_strings[s32(KeyCode::Z)] = _(strings::key_Z);
	key_strings[s32(KeyCode::Capslock)] = _(strings::key_Capslock);
	key_strings[s32(KeyCode::F1)] = _(strings::key_F1);
	key_strings[s32(KeyCode::F2)] = _(strings::key_F2);
	key_strings[s32(KeyCode::F3)] = _(strings::key_F3);
	key_strings[s32(KeyCode::F4)] = _(strings::key_F4);
	key_strings[s32(KeyCode::F5)] = _(strings::key_F5);
	key_strings[s32(KeyCode::F6)] = _(strings::key_F6);
	key_strings[s32(KeyCode::F7)] = _(strings::key_F7);
	key_strings[s32(KeyCode::F8)] = _(strings::key_F8);
	key_strings[s32(KeyCode::F9)] = _(strings::key_F9);
	key_strings[s32(KeyCode::F10)] = _(strings::key_F10);
	key_strings[s32(KeyCode::F11)] = _(strings::key_F11);
	key_strings[s32(KeyCode::F12)] = _(strings::key_F12);
	key_strings[s32(KeyCode::Printscreen)] = _(strings::key_Printscreen);
	key_strings[s32(KeyCode::Scrolllock)] = _(strings::key_Scrolllock);
	key_strings[s32(KeyCode::Pause)] = _(strings::key_Pause);
	key_strings[s32(KeyCode::Insert)] = _(strings::key_Insert);
	key_strings[s32(KeyCode::Home)] = _(strings::key_Home);
	key_strings[s32(KeyCode::PageUp)] = _(strings::key_PageUp);
	key_strings[s32(KeyCode::Delete)] = _(strings::key_Delete);
	key_strings[s32(KeyCode::End)] = _(strings::key_End);
	key_strings[s32(KeyCode::PageDown)] = _(strings::key_PageDown);
	key_strings[s32(KeyCode::Right)] = _(strings::key_Right);
	key_strings[s32(KeyCode::Left)] = _(strings::key_Left);
	key_strings[s32(KeyCode::Down)] = _(strings::key_Down);
	key_strings[s32(KeyCode::Up)] = _(strings::key_Up);
	key_strings[s32(KeyCode::NumlockClear)] = _(strings::key_NumlockClear);
	key_strings[s32(KeyCode::KeypadDivide)] = _(strings::key_KeypadDivide);
	key_strings[s32(KeyCode::KeypadMultiply)] = _(strings::key_KeypadMultiply);
	key_strings[s32(KeyCode::KeypadMinus)] = _(strings::key_KeypadMinus);
	key_strings[s32(KeyCode::KeypadPlus)] = _(strings::key_KeypadPlus);
	key_strings[s32(KeyCode::KeypadEnter)] = _(strings::key_KeypadEnter);
	key_strings[s32(KeyCode::Keypad1)] = _(strings::key_Keypad1);
	key_strings[s32(KeyCode::Keypad2)] = _(strings::key_Keypad2);
	key_strings[s32(KeyCode::Keypad3)] = _(strings::key_Keypad3);
	key_strings[s32(KeyCode::Keypad4)] = _(strings::key_Keypad4);
	key_strings[s32(KeyCode::Keypad5)] = _(strings::key_Keypad5);
	key_strings[s32(KeyCode::Keypad6)] = _(strings::key_Keypad6);
	key_strings[s32(KeyCode::Keypad7)] = _(strings::key_Keypad7);
	key_strings[s32(KeyCode::Keypad8)] = _(strings::key_Keypad8);
	key_strings[s32(KeyCode::Keypad9)] = _(strings::key_Keypad9);
	key_strings[s32(KeyCode::Keypad0)] = _(strings::key_Keypad0);
	key_strings[s32(KeyCode::KeypadPeriod)] = _(strings::key_KeypadPeriod);
	key_strings[s32(KeyCode::Application)] = _(strings::key_Application);
	key_strings[s32(KeyCode::Power)] = _(strings::key_Power);
	key_strings[s32(KeyCode::KeypadEquals)] = _(strings::key_KeypadEquals);
	key_strings[s32(KeyCode::F13)] = _(strings::key_F13);
	key_strings[s32(KeyCode::F14)] = _(strings::key_F14);
	key_strings[s32(KeyCode::F15)] = _(strings::key_F15);
	key_strings[s32(KeyCode::F16)] = _(strings::key_F16);
	key_strings[s32(KeyCode::F17)] = _(strings::key_F17);
	key_strings[s32(KeyCode::F18)] = _(strings::key_F18);
	key_strings[s32(KeyCode::F19)] = _(strings::key_F19);
	key_strings[s32(KeyCode::F20)] = _(strings::key_F20);
	key_strings[s32(KeyCode::F21)] = _(strings::key_F21);
	key_strings[s32(KeyCode::F22)] = _(strings::key_F22);
	key_strings[s32(KeyCode::F23)] = _(strings::key_F23);
	key_strings[s32(KeyCode::F24)] = _(strings::key_F24);
	key_strings[s32(KeyCode::Execute)] = _(strings::key_Execute);
	key_strings[s32(KeyCode::Help)] = _(strings::key_Help);
	key_strings[s32(KeyCode::Menu)] = _(strings::key_Menu);
	key_strings[s32(KeyCode::Select)] = _(strings::key_Select);
	key_strings[s32(KeyCode::Stop)] = _(strings::key_Stop);
	key_strings[s32(KeyCode::Again)] = _(strings::key_Again);
	key_strings[s32(KeyCode::Undo)] = _(strings::key_Undo);
	key_strings[s32(KeyCode::Cut)] = _(strings::key_Cut);
	key_strings[s32(KeyCode::Copy)] = _(strings::key_Copy);
	key_strings[s32(KeyCode::Paste)] = _(strings::key_Paste);
	key_strings[s32(KeyCode::Find)] = _(strings::key_Find);
	key_strings[s32(KeyCode::Mute)] = _(strings::key_Mute);
	key_strings[s32(KeyCode::VolumeUp)] = _(strings::key_VolumeUp);
	key_strings[s32(KeyCode::VolumeDown)] = _(strings::key_VolumeDown);
	key_strings[s32(KeyCode::KeypadComma)] = _(strings::key_KeypadComma);
	key_strings[s32(KeyCode::KeypadEqualsAS400)] = _(strings::key_KeypadEqualsAS400);
	key_strings[s32(KeyCode::AltErase)] = _(strings::key_AltErase);
	key_strings[s32(KeyCode::SysReq)] = _(strings::key_SysReq);
	key_strings[s32(KeyCode::Cancel)] = _(strings::key_Cancel);
	key_strings[s32(KeyCode::Clear)] = _(strings::key_Clear);
	key_strings[s32(KeyCode::Prior)] = _(strings::key_Prior);
	key_strings[s32(KeyCode::Return2)] = _(strings::key_Return2);
	key_strings[s32(KeyCode::Separator)] = _(strings::key_Separator);
	key_strings[s32(KeyCode::Out)] = _(strings::key_Out);
	key_strings[s32(KeyCode::Oper)] = _(strings::key_Oper);
	key_strings[s32(KeyCode::ClearAgain)] = _(strings::key_ClearAgain);
	key_strings[s32(KeyCode::CrSel)] = _(strings::key_CrSel);
	key_strings[s32(KeyCode::ExSel)] = _(strings::key_ExSel);
	key_strings[s32(KeyCode::Keypad00)] = _(strings::key_Keypad00);
	key_strings[s32(KeyCode::Keypad000)] = _(strings::key_Keypad000);
	key_strings[s32(KeyCode::ThousandsSeparator)] = _(strings::key_ThousandsSeparator);
	key_strings[s32(KeyCode::DecimalSeparator)] = _(strings::key_DecimalSeparator);
	key_strings[s32(KeyCode::CurrencyUnit)] = _(strings::key_CurrencyUnit);
	key_strings[s32(KeyCode::CurrencySubunit)] = _(strings::key_CurrencySubunit);
	key_strings[s32(KeyCode::KeypadLeftParen)] = _(strings::key_KeypadLeftParen);
	key_strings[s32(KeyCode::KeypadRightParen)] = _(strings::key_KeypadRightParen);
	key_strings[s32(KeyCode::KeypadLeftBrace)] = _(strings::key_KeypadLeftBrace);
	key_strings[s32(KeyCode::KeypadRightBrace)] = _(strings::key_KeypadRightBrace);
	key_strings[s32(KeyCode::KeypadTab)] = _(strings::key_KeypadTab);
	key_strings[s32(KeyCode::KeypadBackspace)] = _(strings::key_KeypadBackspace);
	key_strings[s32(KeyCode::KeypadA)] = _(strings::key_KeypadA);
	key_strings[s32(KeyCode::KeypadB)] = _(strings::key_KeypadB);
	key_strings[s32(KeyCode::KeypadC)] = _(strings::key_KeypadC);
	key_strings[s32(KeyCode::KeypadD)] = _(strings::key_KeypadD);
	key_strings[s32(KeyCode::KeypadE)] = _(strings::key_KeypadE);
	key_strings[s32(KeyCode::KeypadF)] = _(strings::key_KeypadF);
	key_strings[s32(KeyCode::KeypadXor)] = _(strings::key_KeypadXor);
	key_strings[s32(KeyCode::KeypadPower)] = _(strings::key_KeypadPower);
	key_strings[s32(KeyCode::KeypadPercent)] = _(strings::key_KeypadPercent);
	key_strings[s32(KeyCode::KeypadLess)] = _(strings::key_KeypadLess);
	key_strings[s32(KeyCode::KeypadGreater)] = _(strings::key_KeypadGreater);
	key_strings[s32(KeyCode::KeypadAmpersand)] = _(strings::key_KeypadAmpersand);
	key_strings[s32(KeyCode::KeypadDblAmpersand)] = _(strings::key_KeypadDblAmpersand);
	key_strings[s32(KeyCode::KeypadVerticalBar)] = _(strings::key_KeypadVerticalBar);
	key_strings[s32(KeyCode::KeypadDblVerticalBar)] = _(strings::key_KeypadDblVerticalBar);
	key_strings[s32(KeyCode::KeypadColon)] = _(strings::key_KeypadColon);
	key_strings[s32(KeyCode::KeypadHash)] = _(strings::key_KeypadHash);
	key_strings[s32(KeyCode::KeypadSpace)] = _(strings::key_KeypadSpace);
	key_strings[s32(KeyCode::KeypadAt)] = _(strings::key_KeypadAt);
	key_strings[s32(KeyCode::KeypadExclam)] = _(strings::key_KeypadExclam);
	key_strings[s32(KeyCode::KeypadMemStore)] = _(strings::key_KeypadMemStore);
	key_strings[s32(KeyCode::KeypadMemRecall)] = _(strings::key_KeypadMemRecall);
	key_strings[s32(KeyCode::KeypadMemClear)] = _(strings::key_KeypadMemClear);
	key_strings[s32(KeyCode::KeypadMemAdd)] = _(strings::key_KeypadMemAdd);
	key_strings[s32(KeyCode::KeypadMemSubtract)] = _(strings::key_KeypadMemSubtract);
	key_strings[s32(KeyCode::KeypadMemMultiply)] = _(strings::key_KeypadMemMultiply);
	key_strings[s32(KeyCode::KeypadMemDivide)] = _(strings::key_KeypadMemDivide);
	key_strings[s32(KeyCode::KeypadPlusMinus)] = _(strings::key_KeypadPlusMinus);
	key_strings[s32(KeyCode::KeypadClear)] = _(strings::key_KeypadClear);
	key_strings[s32(KeyCode::KeypadClearEntry)] = _(strings::key_KeypadClearEntry);
	key_strings[s32(KeyCode::KeypadBinary)] = _(strings::key_KeypadBinary);
	key_strings[s32(KeyCode::KeypadOctal)] = _(strings::key_KeypadOctal);
	key_strings[s32(KeyCode::KeypadDecimal)] = _(strings::key_KeypadDecimal);
	key_strings[s32(KeyCode::KeypadHexadecimal)] = _(strings::key_KeypadHexadecimal);
	key_strings[s32(KeyCode::LCtrl)] = _(strings::key_LCtrl);
	key_strings[s32(KeyCode::LShift)] = _(strings::key_LShift);
	key_strings[s32(KeyCode::LAlt)] = _(strings::key_LAlt);
	key_strings[s32(KeyCode::LGui)] = _(strings::key_LGui);
	key_strings[s32(KeyCode::RCtrl)] = _(strings::key_RCtrl);
	key_strings[s32(KeyCode::RShift)] = _(strings::key_RShift);
	key_strings[s32(KeyCode::RAlt)] = _(strings::key_RAlt);
	key_strings[s32(KeyCode::RGui)] = _(strings::key_RGui);
	key_strings[s32(KeyCode::Mode)] = _(strings::key_Mode);
	key_strings[s32(KeyCode::AudioNext)] = _(strings::key_AudioNext);
	key_strings[s32(KeyCode::AudioPrev)] = _(strings::key_AudioPrev);
	key_strings[s32(KeyCode::AudioStop)] = _(strings::key_AudioStop);
	key_strings[s32(KeyCode::AudioPlay)] = _(strings::key_AudioPlay);
	key_strings[s32(KeyCode::AudioMute)] = _(strings::key_AudioMute);
	key_strings[s32(KeyCode::MediaSelect)] = _(strings::key_MediaSelect);
	key_strings[s32(KeyCode::Www)] = _(strings::key_Www);
	key_strings[s32(KeyCode::Mail)] = _(strings::key_Mail);
	key_strings[s32(KeyCode::Calculator)] = _(strings::key_Calculator);
	key_strings[s32(KeyCode::Computer)] = _(strings::key_Computer);
	key_strings[s32(KeyCode::ACSearch)] = _(strings::key_ACSearch);
	key_strings[s32(KeyCode::ACHome)] = _(strings::key_ACHome);
	key_strings[s32(KeyCode::ACBack)] = _(strings::key_ACBack);
	key_strings[s32(KeyCode::ACForward)] = _(strings::key_ACForward);
	key_strings[s32(KeyCode::ACStop)] = _(strings::key_ACStop);
	key_strings[s32(KeyCode::ACRefresh)] = _(strings::key_ACRefresh);
	key_strings[s32(KeyCode::ACBookmarks)] = _(strings::key_ACBookmarks);
	key_strings[s32(KeyCode::BrightnessDown)] = _(strings::key_BrightnessDown);
	key_strings[s32(KeyCode::BrightnessUp)] = _(strings::key_BrightnessUp);
	key_strings[s32(KeyCode::DisplaySwitch)] = _(strings::key_DisplaySwitch);
	key_strings[s32(KeyCode::KbDillumToggle)] = _(strings::key_KbDillumToggle);
	key_strings[s32(KeyCode::KbDillumDown)] = _(strings::key_KbDillumDown);
	key_strings[s32(KeyCode::KbDillumUp)] = _(strings::key_KbDillumUp);
	key_strings[s32(KeyCode::Eject)] = _(strings::key_Eject);
	key_strings[s32(KeyCode::Sleep)] = _(strings::key_Sleep);
	key_strings[s32(KeyCode::MouseLeft)] = _(strings::key_MouseLeft);
	key_strings[s32(KeyCode::MouseRight)] = _(strings::key_MouseRight);
	key_strings[s32(KeyCode::MouseMiddle)] = _(strings::key_MouseMiddle);

	btn_strings_xbox[s32(Gamepad::Btn::LeftShoulder)] = _(strings::btn_LeftShoulder);
	btn_strings_xbox[s32(Gamepad::Btn::RightShoulder)] = _(strings::btn_RightShoulder);
	btn_strings_xbox[s32(Gamepad::Btn::LeftClick)] = _(strings::btn_LeftClick);
	btn_strings_xbox[s32(Gamepad::Btn::RightClick)] = _(strings::btn_RightClick);
	btn_strings_xbox[s32(Gamepad::Btn::A)] = _(strings::btn_A);
	btn_strings_xbox[s32(Gamepad::Btn::B)] = _(strings::btn_B);
	btn_strings_xbox[s32(Gamepad::Btn::X)] = _(strings::btn_X);
	btn_strings_xbox[s32(Gamepad::Btn::Y)] = _(strings::btn_Y);
	btn_strings_xbox[s32(Gamepad::Btn::Back)] = _(strings::btn_Back);
	btn_strings_xbox[s32(Gamepad::Btn::Start)] = _(strings::btn_Start);
	btn_strings_xbox[s32(Gamepad::Btn::LeftTrigger)] = _(strings::btn_LeftTrigger);
	btn_strings_xbox[s32(Gamepad::Btn::RightTrigger)] = _(strings::btn_RightTrigger);
	btn_strings_xbox[s32(Gamepad::Btn::DDown)] = _(strings::btn_DDown);
	btn_strings_xbox[s32(Gamepad::Btn::DUp)] = _(strings::btn_DUp);
	btn_strings_xbox[s32(Gamepad::Btn::DLeft)] = _(strings::btn_DLeft);
	btn_strings_xbox[s32(Gamepad::Btn::DRight)] = _(strings::btn_DRight);
	btn_strings_xbox[s32(Gamepad::Btn::None)] = _(strings::btn_None);

	btn_strings_playstation[s32(Gamepad::Btn::LeftShoulder)] = _(strings::btn_ps4_LeftShoulder);
	btn_strings_playstation[s32(Gamepad::Btn::RightShoulder)] = _(strings::btn_ps4_RightShoulder);
	btn_strings_playstation[s32(Gamepad::Btn::LeftClick)] = _(strings::btn_ps4_LeftClick);
	btn_strings_playstation[s32(Gamepad::Btn::RightClick)] = _(strings::btn_ps4_RightClick);
	btn_strings_playstation[s32(Gamepad::Btn::A)] = _(strings::btn_ps4_A);
	btn_strings_playstation[s32(Gamepad::Btn::B)] = _(strings::btn_ps4_B);
	btn_strings_playstation[s32(Gamepad::Btn::X)] = _(strings::btn_ps4_X);
	btn_strings_playstation[s32(Gamepad::Btn::Y)] = _(strings::btn_ps4_Y);
	btn_strings_playstation[s32(Gamepad::Btn::Back)] = _(strings::btn_ps4_Back);
	btn_strings_playstation[s32(Gamepad::Btn::Start)] = _(strings::btn_ps4_Start);
	btn_strings_playstation[s32(Gamepad::Btn::LeftTrigger)] = _(strings::btn_ps4_LeftTrigger);
	btn_strings_playstation[s32(Gamepad::Btn::RightTrigger)] = _(strings::btn_ps4_RightTrigger);
	btn_strings_playstation[s32(Gamepad::Btn::DDown)] = _(strings::btn_ps4_DDown);
	btn_strings_playstation[s32(Gamepad::Btn::DUp)] = _(strings::btn_ps4_DUp);
	btn_strings_playstation[s32(Gamepad::Btn::DLeft)] = _(strings::btn_ps4_DLeft);
	btn_strings_playstation[s32(Gamepad::Btn::DRight)] = _(strings::btn_ps4_DRight);
	btn_strings_playstation[s32(Gamepad::Btn::None)] = _(strings::btn_None);

	control_strings[s32(Controls::Forward)] = _(strings::forward);
	control_strings[s32(Controls::Backward)] = _(strings::backward);
	control_strings[s32(Controls::Left)] = _(strings::left);
	control_strings[s32(Controls::Right)] = _(strings::right);
	control_strings[s32(Controls::Primary)] = _(strings::primary);
	control_strings[s32(Controls::Zoom)] = _(strings::zoom);
	control_strings[s32(Controls::Ability1)] = _(strings::ability1);
	control_strings[s32(Controls::Ability2)] = _(strings::ability2);
	control_strings[s32(Controls::Ability3)] = _(strings::ability3);
	control_strings[s32(Controls::InteractSecondary)] = _(strings::interact);
	control_strings[s32(Controls::Scoreboard)] = _(strings::scoreboard);
	control_strings[s32(Controls::Jump)] = _(strings::jump);
	control_strings[s32(Controls::Parkour)] = _(strings::parkour);
	control_strings[s32(Controls::Grapple)] = _(strings::grapple);
	control_strings[s32(Controls::GrappleCancel)] = _(strings::grapple_cancel);
	control_strings[s32(Controls::UIContextAction)] = _(strings::ui_context_action);
	control_strings[s32(Controls::TabLeft)] = _(strings::tab_left);
	control_strings[s32(Controls::TabRight)] = _(strings::tab_right);
	control_strings[s32(Controls::Emote1)] = _(strings::emote1);
	control_strings[s32(Controls::Emote2)] = _(strings::emote2);
	control_strings[s32(Controls::Emote3)] = _(strings::emote3);
	control_strings[s32(Controls::Emote4)] = _(strings::emote4);
	control_strings[s32(Controls::ChatTeam)] = _(strings::chat_team);
	control_strings[s32(Controls::ChatAll)] = _(strings::chat_all);
	control_strings[s32(Controls::Spot)] = _(strings::spot);
	control_strings[s32(Controls::Console)] = _(strings::toggle_console);

	TextField::normal_map[s32(KeyCode::D0)] = '0';
	TextField::normal_map[s32(KeyCode::D1)] = '1';
	TextField::normal_map[s32(KeyCode::D2)] = '2';
	TextField::normal_map[s32(KeyCode::D3)] = '3';
	TextField::normal_map[s32(KeyCode::D4)] = '4';
	TextField::normal_map[s32(KeyCode::D5)] = '5';
	TextField::normal_map[s32(KeyCode::D6)] = '6';
	TextField::normal_map[s32(KeyCode::D7)] = '7';
	TextField::normal_map[s32(KeyCode::D8)] = '8';
	TextField::normal_map[s32(KeyCode::D9)] = '9';
	TextField::shift_map[s32(KeyCode::D0)] = ')';
	TextField::shift_map[s32(KeyCode::D1)] = '!';
	TextField::shift_map[s32(KeyCode::D2)] = '@';
	TextField::shift_map[s32(KeyCode::D3)] = '#';
	TextField::shift_map[s32(KeyCode::D4)] = '$';
	TextField::shift_map[s32(KeyCode::D5)] = '%';
	TextField::shift_map[s32(KeyCode::D6)] = '^';
	TextField::shift_map[s32(KeyCode::D7)] = '&';
	TextField::shift_map[s32(KeyCode::D8)] = '*';
	TextField::shift_map[s32(KeyCode::D9)] = '(';

	TextField::normal_map[s32(KeyCode::Space)] = ' ';
	TextField::shift_map[s32(KeyCode::Space)] = ' ';

	TextField::normal_map[s32(KeyCode::Apostrophe)] = '\'';
	TextField::shift_map[s32(KeyCode::Apostrophe)] = '"';

	TextField::normal_map[s32(KeyCode::Minus)] = '-';
	TextField::normal_map[s32(KeyCode::Equals)] = '=';
	TextField::normal_map[s32(KeyCode::LeftBracket)] = '[';
	TextField::normal_map[s32(KeyCode::RightBracket)] = ']';
	TextField::normal_map[s32(KeyCode::Comma)] = ',';
	TextField::normal_map[s32(KeyCode::Period)] = '.';
	TextField::normal_map[s32(KeyCode::Slash)] = '/';
	TextField::normal_map[s32(KeyCode::Grave)] = '`';
	TextField::normal_map[s32(KeyCode::Semicolon)] = ';';
	TextField::normal_map[s32(KeyCode::Backslash)] = '\\';
	TextField::shift_map[s32(KeyCode::Minus)] = '_';
	TextField::shift_map[s32(KeyCode::Equals)] = '+';
	TextField::shift_map[s32(KeyCode::LeftBracket)] = '{';
	TextField::shift_map[s32(KeyCode::RightBracket)] = '}';
	TextField::shift_map[s32(KeyCode::Comma)] = '<';
	TextField::shift_map[s32(KeyCode::Period)] = '>';
	TextField::shift_map[s32(KeyCode::Slash)] = '?';
	TextField::shift_map[s32(KeyCode::Grave)] = '~';
	TextField::shift_map[s32(KeyCode::Semicolon)] = ':';
	TextField::shift_map[s32(KeyCode::Backslash)] = '|';

	for (s32 i = 0; i <= (s32)KeyCode::Z - (s32)KeyCode::A; i++)
	{
		TextField::normal_map[s32(KeyCode::A) + i] = 'a' + i;
		TextField::shift_map[s32(KeyCode::A) + i] = 'A' + i;
	}
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

void dead_zone_cross(r32* x, r32* y, r32 threshold)
{
	*x = dead_zone(*x, threshold);
	*y = dead_zone(*y, threshold);
}

r32 dead_zone(r32 x, r32 threshold)
{
	if (fabsf(x) < threshold)
		return 0.0f;
	else
		return (x - threshold) / (1.0f - threshold);
}

const char* control_string(Controls c)
{
	return control_strings[s32(c)];
}

const char* control_ui_variable_name(Controls c)
{
	return control_ui_variable_names[s32(c)];
}

b8 control_customizable(Controls c, Gamepad::Type type)
{
	if (!control_setting_names[s32(c)])
		return false;

	if (type != Gamepad::Type::None
		&& (c == Controls::Forward
			|| c == Controls::Backward
			|| c == Controls::Left
			|| c == Controls::Right
			|| c == Controls::ChatTeam
			|| c == Controls::ChatAll))
	{
		return false;
	}

	return true;
}

}

const char* InputBinding::string(Gamepad::Type type) const
{
	switch (type)
	{
		case Gamepad::Type::None:
		{
			return Input::key_strings[s32(key1)];
		}
		case Gamepad::Type::Xbox:
		{
			return Input::btn_strings_xbox[s32(btn)];
		}
		case Gamepad::Type::Playstation:
		{
			return Input::btn_strings_playstation[s32(btn)];
		}
		default:
		{
			vi_assert(false);
			return nullptr;
			break;
		}
	}
}

b8 InputBinding::overlaps(const InputBinding& other) const
{
	return btn == other.btn || (key1 != KeyCode::None && key1 == other.key1) || (key2 != KeyCode::None && key2 == other.key2);
}

b8 InputState::get(Controls c, s32 gamepad) const
{
#if SERVER
	return false;
#else
	const InputBinding& binding = Settings::gamepads[gamepad].bindings[s32(c)];
	return (gamepad == 0 && (keys.get(s32(binding.key1)) || (binding.key2 != KeyCode::None && keys.get(s32(binding.key2))))) // keys
		|| (gamepads[gamepad].btns & (1 << s32(binding.btn))); // gamepad buttons
#endif
}

char TextField::shift_map[127] = {};
char TextField::normal_map[127] = {};

#define TEXT_FIELD_REPEAT_DELAY 0.2f
#define TEXT_FIELD_REPEAT_INTERVAL 0.03f

TextField::TextField()
	: value(1, 1),
	ignored_keys(),
	repeat_start_time(),
	repeat_last_time()
{

}

void TextField::set(const char* v)
{
	value.resize(s32(strlen(v)) + 1);
	memcpy(value.data, v, value.length - 1);
	value[value.length - 1] = '\0';
}

void TextField::get(UIText* text, s32 truncate) const
{
	if (truncate > 0 && value.length > truncate + 1) // truncate
	{
		const char* start = value.data;
		while (start < &value.data[value.length - truncate])
			start = Unicode::codepoint_next(start);
		text->text_raw(0, start, UITextFlagSingleLine);
	}
	else
		text->text_raw(0, value.data, UITextFlagSingleLine);
}

b8 TextField::update(const Update& u, s32 first_editable_index, s32 max_length)
{
	b8 changed = false;

	b8 shift = u.input->keys.get(s32(KeyCode::LShift))
		|| u.input->keys.get(s32(KeyCode::RShift));
	b8 any_key_pressed = u.input->keys.any();
	KeyCode console_key = Settings::gamepads[0].bindings[s32(Controls::Console)].key1;
	if (max_length == 0 || value.length < max_length - sizeof(u32)) // make sure we have room even for a big ol' UTF-8 character
	{
		for (s32 i = u.input->keys.start; i < u.input->keys.end && i < 127; i = u.input->keys.next(i))
		{
			b8 ignore_key = i == s32(console_key);
			if (!ignore_key)
			{
				for (s32 j = 0; j < ignored_keys.length; j++)
				{
					if (i == s32(ignored_keys[j]))
					{
						ignore_key = true;
						break;
					}
				}
			}
			if (ignore_key)
				continue;

			char c = shift ? shift_map[i] : normal_map[i];
			if (!c)
				continue;

			b8 add = false;
			if (!u.last_input->keys.get(i))
			{
				repeat_start_time = u.real_time.total;
				add = true;
			}
			else if (u.real_time.total - repeat_start_time > TEXT_FIELD_REPEAT_DELAY
				&& u.real_time.total - repeat_last_time > TEXT_FIELD_REPEAT_INTERVAL)
			{
				repeat_last_time = u.real_time.total;
				add = true;
			}

			if (add)
			{
				value[value.length - 1] = c;
				value.add(0);
				changed = true;
				break;
			}
		}
	}

	if (value.length > first_editable_index + 1 && u.input->keys.get(s32(KeyCode::Backspace)))
	{
		any_key_pressed = true;

		b8 remove = false;
		if (!u.last_input->keys.get(s32(KeyCode::Backspace)))
		{
			repeat_start_time = u.real_time.total;
			remove = true;
		}
		else if (u.real_time.total - repeat_start_time > TEXT_FIELD_REPEAT_DELAY
			&& u.real_time.total - repeat_last_time > TEXT_FIELD_REPEAT_INTERVAL)
		{
			repeat_last_time = u.real_time.total;
			remove = true;
		}

		if (remove)
		{
			value.remove(value.length - 1);
			value[value.length - 1] = '\0';
			changed = true;
		}
	}

	if (!any_key_pressed)
		repeat_start_time = 0.0f;

	return changed;
}


}
