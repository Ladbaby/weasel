﻿#include "stdafx.h"
#include <resource.h>
#include "WeaselTSF.h"
#include "LanguageBar.h"
#include "CandidateList.h"

static const DWORD LANGBARITEMSINK_COOKIE = 0x42424242;

static void HMENU2ITfMenu(HMENU hMenu, ITfMenu *pTfMenu)
{
	/* NOTE: Only limited functions are supported */
	int N = GetMenuItemCount(hMenu);
	for (int i = 0; i < N; i++)
	{
		MENUITEMINFO mii;
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
		mii.dwTypeData = NULL;
		if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
		{
			UINT id = mii.wID;
			if (mii.fType == MFT_SEPARATOR)
				pTfMenu->AddMenuItem(id, TF_LBMENUF_SEPARATOR, NULL, NULL, NULL, 0, NULL);
			else if (mii.fType == MFT_STRING)
			{
				mii.dwTypeData = (LPWSTR) malloc(sizeof(WCHAR) * (mii.cch + 1));
				mii.cch++;
				if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
					pTfMenu->AddMenuItem(id, 0, NULL, NULL, mii.dwTypeData, mii.cch, NULL);
				free(mii.dwTypeData);
			}
		}
	}
}

CLangBarItemButton::CLangBarItemButton(com_ptr<WeaselTSF> pTextService, REFGUID guid, weasel::UIStyle& style)
	: _status(0), _style(style), _current_schema_icon()
{
	DllAddRef();

	_pLangBarItemSink = NULL;
	_cRef = 1;
	_pTextService = pTextService;
	_guid = guid;
	ascii_mode = false;
}

CLangBarItemButton::~CLangBarItemButton()
{
	DllRelease();
}

STDAPI CLangBarItemButton::QueryInterface(REFIID riid, void **ppvObject)
{
	if (ppvObject == NULL)
		return E_INVALIDARG;

	*ppvObject = NULL;
	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) || IsEqualIID(riid, IID_ITfLangBarItemButton))
		*ppvObject = (ITfLangBarItemButton *) this;
	else if (IsEqualIID(riid, IID_ITfSource))
		*ppvObject = (ITfSource *) this;

	if (*ppvObject)
	{
		AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

STDAPI_(ULONG) CLangBarItemButton::AddRef()
{
	return ++_cRef;
}

STDAPI_(ULONG) CLangBarItemButton::Release()
{
	LONG cr = --_cRef;
	assert(_cRef >= 0);
	if (_cRef == 0)
		delete this;
	return cr;
}

STDAPI CLangBarItemButton::GetInfo(TF_LANGBARITEMINFO *pInfo)
{
	pInfo->clsidService = c_clsidTextService;
	pInfo->guidItem = _guid;
	pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY;
	pInfo->ulSort = 1;
	lstrcpyW(pInfo->szDescription, L"WeaselTSF Button");
	return S_OK;
}

STDAPI CLangBarItemButton::GetStatus(DWORD *pdwStatus)
{
	*pdwStatus = _status;
	return S_OK;
}

STDAPI CLangBarItemButton::Show(BOOL fShow)
{
	SetLangbarStatus(TF_LBI_STATUS_HIDDEN, fShow ? FALSE : TRUE);
	return S_OK;
}

STDAPI CLangBarItemButton::GetTooltipString(BSTR *pbstrToolTip)
{
	*pbstrToolTip = SysAllocString(L"左鍵切換模式，右鍵打開菜單");
	return (*pbstrToolTip == NULL)? E_OUTOFMEMORY: S_OK;
}

STDAPI CLangBarItemButton::OnClick(TfLBIClick click, POINT pt, const RECT *prcArea)
{
	if (click == TF_LBI_CLK_LEFT)
	{
		_pTextService->_HandleLangBarMenuSelect(ascii_mode ? ID_WEASELTRAY_DISABLE_ASCII : ID_WEASELTRAY_ENABLE_ASCII);
		ascii_mode = !ascii_mode;
		if (_pLangBarItemSink) {
			_pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
		}
	}
	else if (click == TF_LBI_CLK_RIGHT)
	{
		/* Open menu */
		HWND hwnd = _pTextService->_GetFocusedContextWindow();
		if (hwnd != NULL)
		{
			HMENU menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP));
			HMENU popupMenu = GetSubMenu(menu, 0);
			UINT wID = TrackPopupMenuEx(popupMenu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_HORPOSANIMATION, pt.x, pt.y, hwnd, NULL);
			DestroyMenu(menu);
			_pTextService->_HandleLangBarMenuSelect(wID);
		}
	}
	return S_OK;
}

STDAPI CLangBarItemButton::InitMenu(ITfMenu *pMenu)
{
	HMENU menu = LoadMenuW(g_hInst, MAKEINTRESOURCE(IDR_MENU_POPUP));
	HMENU popupMenu = GetSubMenu(menu, 0);
	HMENU2ITfMenu(popupMenu, pMenu);
	DestroyMenu(menu);
	return S_OK;
}

STDAPI CLangBarItemButton::OnMenuSelect(UINT wID)
{
	_pTextService->_HandleLangBarMenuSelect(wID);
	return S_OK;
}

STDAPI CLangBarItemButton::GetIcon(HICON *phIcon)
{
	if (ascii_mode || _style.current_zhung_icon.empty())
		*phIcon = (HICON)LoadImageW(
			g_hInst,
			MAKEINTRESOURCEW(ascii_mode ? IDI_EN : IDI_ZH),
			IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON),
			GetSystemMetrics(SM_CYSMICON),
			LR_SHARED);
	else
	{ 
		*phIcon = (HICON)LoadImageW(
			NULL,
			_style.current_zhung_icon.c_str(),
			IMAGE_ICON,
			GetSystemMetrics(SM_CXSMICON),
			GetSystemMetrics(SM_CYSMICON),
			LR_LOADFROMFILE);
	}
	return (*phIcon == NULL)? E_FAIL: S_OK;
}

STDAPI CLangBarItemButton::GetText(BSTR *pbstrText)
{
	*pbstrText = SysAllocString(L"WeaselTSF Button");
	return (*pbstrText == NULL)? E_OUTOFMEMORY: S_OK;
}

STDAPI CLangBarItemButton::AdviseSink(REFIID riid, IUnknown *punk, DWORD *pdwCookie)
{
	if (!IsEqualIID(riid, IID_ITfLangBarItemSink))
		return CONNECT_E_CANNOTCONNECT;
	if (_pLangBarItemSink != NULL)
		return CONNECT_E_ADVISELIMIT;

	if (punk->QueryInterface(IID_ITfLangBarItemSink, (LPVOID *) &_pLangBarItemSink) != S_OK)
	{
		_pLangBarItemSink = NULL;
		return E_NOINTERFACE;
	}
	*pdwCookie = LANGBARITEMSINK_COOKIE;
	return S_OK;
}

STDAPI CLangBarItemButton::UnadviseSink(DWORD dwCookie)
{
	if (dwCookie != LANGBARITEMSINK_COOKIE || _pLangBarItemSink == NULL)
		return CONNECT_E_NOCONNECTION;
	_pLangBarItemSink = NULL;
	return S_OK;
}

void CLangBarItemButton::UpdateWeaselStatus(weasel::Status stat)
{
	if (stat.ascii_mode != ascii_mode) {
		ascii_mode = stat.ascii_mode;
		if (_pLangBarItemSink) {
			_pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
		}
	}
	if (_current_schema_icon != _style.current_zhung_icon) {
		_current_schema_icon = _style.current_zhung_icon;
		if (_pLangBarItemSink) {
			_pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
		}
	}

}

void CLangBarItemButton::SetLangbarStatus(DWORD dwStatus, BOOL fSet)
{
	BOOL isChange = FALSE;

	if (fSet)
	{
		if (!(_status & dwStatus))
		{
			_status |= dwStatus;
			isChange = TRUE;
		}
	}
	else
	{
		if (_status & dwStatus)
		{
			_status &= ~dwStatus;
			isChange = TRUE;
		}
	}

	if (isChange && _pLangBarItemSink)
	{
		_pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
	}

	return;
}


void WeaselTSF::_HandleLangBarMenuSelect(UINT wID)
{
	m_client.TrayCommand(wID);
}

HWND WeaselTSF::_GetFocusedContextWindow()
{
	HWND hwnd = NULL;
	ITfDocumentMgr *pDocMgr;
	if (_pThreadMgr->GetFocus(&pDocMgr) == S_OK && pDocMgr != NULL)
	{
		ITfContext *pContext;
		if (pDocMgr->GetTop(&pContext) == S_OK && pContext != NULL)
		{
			ITfContextView *pContextView;
			if (pContext->GetActiveView(&pContextView) == S_OK && pContextView != NULL)
			{
				pContextView->GetWnd(&hwnd);
				pContextView->Release();
			}
			pContext->Release();
		}
		pDocMgr->Release();
	}

	if (hwnd == NULL)
	{
		HWND hwndForeground = GetForegroundWindow();
		if (GetWindowThreadProcessId(hwndForeground, NULL) == GetCurrentThreadId())
			hwnd = hwndForeground;
	}

	return hwnd;
}

BOOL WeaselTSF::_InitLanguageBar()
{
	com_ptr<ITfLangBarItemMgr> pLangBarItemMgr;
	BOOL fRet = FALSE;

	if (_pThreadMgr->QueryInterface(&pLangBarItemMgr) != S_OK)
		return FALSE;

	if ((_pLangBarButton = new CLangBarItemButton(this, GUID_LBI_INPUTMODE, _cand->style())) == NULL)
		return FALSE;

	if (pLangBarItemMgr->AddItem(_pLangBarButton) != S_OK)
	{
		_pLangBarButton = NULL;
		return FALSE;
	}

	_pLangBarButton->Show(TRUE);
	fRet = TRUE;

	return fRet;
}

void WeaselTSF::_UninitLanguageBar()
{
	com_ptr<ITfLangBarItemMgr> pLangBarItemMgr;

	if (_pLangBarButton == NULL)
		return;

	if (_pThreadMgr->QueryInterface(&pLangBarItemMgr) == S_OK)
	{
		pLangBarItemMgr->RemoveItem(_pLangBarButton);
	}

	_pLangBarButton = NULL;
}

void WeaselTSF::_UpdateLanguageBar(weasel::Status stat)
{
	if (!_pLangBarButton) return;
	_pLangBarButton->UpdateWeaselStatus(stat);
}

void WeaselTSF::_ShowLanguageBar(BOOL show)
{
	if (!_pLangBarButton) return;
	_pLangBarButton->Show(show);

}

void WeaselTSF::_EnableLanguageBar(BOOL enable)
{
	if (!_pLangBarButton) return;
	_pLangBarButton->SetLangbarStatus(TF_LBI_STATUS_DISABLED, !enable);
}
