/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/

#include <memory>		// // //
#include <cmath>
#include "stdafx.h"
#include "FamiTracker.h"
#include "FamiTrackerDoc.h"
#include "FamiTrackerView.h"
#include "MainFrm.h"
#include "MIDI.h"
#include "InstrumentEditDlg.h"
#include "SpeedDlg.h"
#include "SoundGen.h"
#include "PatternAction.h"
#include "PatternEditor.h"
#include "FrameEditor.h"
#include "Settings.h"
#include "Accelerator.h"
#include "TrackerChannel.h"
#include "Clipboard.h"
#include "APU/APU.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Clipboard ID
const TCHAR CFamiTrackerView::CLIPBOARD_ID[] = _T("FamiTracker Pattern"); // keep

// Effect texts (TODO update this list)
const TCHAR *EFFECT_TEXTS[] = {
	{_T("FXX - Set speed/tempo, XX < 20 = speed, XX >= 20 = tempo")},
	{_T("BXX - Jump to frame")},
	{_T("DXX - Skip to next frame")},
	{_T("CXX - Halt")},
	{_T("EXX - Set volume")},
	{_T("3XX - Automatic portamento, XX = speed")},
	{_T("(not used)")},
	{_T("(not used)")},
	{_T("(not used)")},
	{_T("0XY - Arpeggio, X = second note, Y = third note")},
	{_T("4XY - Vibrato, x = speed, y = depth")},
	{_T("7XY - Tremolo, x = speed, y = depth")},
	{_T("PXX - Fine pitch")},
	{_T("GXX - Row delay, XX = number of frames")},
	{_T("(not used)")},		// // //
	{_T("1XX - Slide up, XX = speed")},
	{_T("2XX - Slide down, XX = speed")},
	{_T("VXX - Square duty / Noise mode")},
	{_T("(not used)")},		// // //
	{_T("QXY - Portamento up, X = speed, Y = notes")},
	{_T("RXY - Portamento down, X = speed, Y = notes")},
	{_T("AXY - Volume slide, X = up, Y = down")},
};

// OLE copy and mix
#define	DROPEFFECT_COPY_MIX	( 8 )

const unsigned char KEY_DOT = 0xBD;		// '.'
const unsigned char KEY_DASH = 0xBE;	// '-'

const int NOTE_HALT = -1;
const int NOTE_RELEASE = -2;

const int SINGLE_STEP = 1;				// Size of single step moves (default: 1)

// Timer IDs
enum { 
	TMR_UPDATE,
	TMR_SCROLL
};

// CFamiTrackerView

IMPLEMENT_DYNCREATE(CFamiTrackerView, CView)

BEGIN_MESSAGE_MAP(CFamiTrackerView, CView)
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_TIMER()
	ON_WM_VSCROLL()
	ON_WM_HSCROLL()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONUP()
	ON_WM_MENUCHAR()
	ON_WM_SYSKEYDOWN()
	ON_COMMAND(ID_EDIT_PASTEMIX, OnEditPastemix)
	ON_COMMAND(ID_EDIT_INSTRUMENTMASK, OnEditInstrumentMask)
	ON_COMMAND(ID_EDIT_VOLUMEMASK, OnEditVolumeMask)
	ON_COMMAND(ID_EDIT_INTERPOLATE, OnEditInterpolate)
	ON_COMMAND(ID_EDIT_REVERSE, OnEditReverse)
	ON_COMMAND(ID_EDIT_REPLACEINSTRUMENT, OnEditReplaceInstrument)
	ON_COMMAND(ID_TRANSPOSE_DECREASENOTE, OnTransposeDecreasenote)
	ON_COMMAND(ID_TRANSPOSE_DECREASEOCTAVE, OnTransposeDecreaseoctave)
	ON_COMMAND(ID_TRANSPOSE_INCREASENOTE, OnTransposeIncreasenote)
	ON_COMMAND(ID_TRANSPOSE_INCREASEOCTAVE, OnTransposeIncreaseoctave)
	ON_COMMAND(ID_DECREASEVALUES, OnDecreaseValues)
	ON_COMMAND(ID_INCREASEVALUES, OnIncreaseValues)
	ON_COMMAND(ID_TRACKER_PLAYROW, OnTrackerPlayrow)
	ON_COMMAND(ID_TRACKER_EDIT, OnTrackerEdit)
	ON_COMMAND(ID_TRACKER_PAL, OnTrackerPal)
	ON_COMMAND(ID_TRACKER_NTSC, OnTrackerNtsc)
	ON_COMMAND(ID_SPEED_CUSTOM, OnSpeedCustom)
	ON_COMMAND(ID_SPEED_DEFAULT, OnSpeedDefault)
	ON_COMMAND(ID_TRACKER_TOGGLECHANNEL, OnTrackerToggleChannel)
	ON_COMMAND(ID_TRACKER_SOLOCHANNEL, OnTrackerSoloChannel)
	ON_COMMAND(ID_CMD_OCTAVE_NEXT, OnNextOctave)
	ON_COMMAND(ID_CMD_OCTAVE_PREVIOUS, OnPreviousOctave)
	ON_COMMAND(ID_CMD_INCREASESTEPSIZE, OnIncreaseStepSize)
	ON_COMMAND(ID_CMD_DECREASESTEPSIZE, OnDecreaseStepSize)
	ON_COMMAND(ID_CMD_STEP_UP, OnOneStepUp)
	ON_COMMAND(ID_CMD_STEP_DOWN, OnOneStepDown)	
	ON_COMMAND(ID_POPUP_TOGGLECHANNEL, OnTrackerToggleChannel)
	ON_COMMAND(ID_POPUP_SOLOCHANNEL, OnTrackerSoloChannel)
	ON_COMMAND(ID_POPUP_UNMUTEALLCHANNELS, OnTrackerUnmuteAllChannels)
//	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
//	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSTRUMENTMASK, OnUpdateEditInstrumentMask)
	ON_UPDATE_COMMAND_UI(ID_EDIT_VOLUMEMASK, OnUpdateEditVolumeMask)
	ON_UPDATE_COMMAND_UI(ID_TRACKER_EDIT, OnUpdateTrackerEdit)
	ON_UPDATE_COMMAND_UI(ID_TRACKER_PAL, OnUpdateTrackerPal)
	ON_UPDATE_COMMAND_UI(ID_TRACKER_NTSC, OnUpdateTrackerNtsc)
	ON_UPDATE_COMMAND_UI(ID_SPEED_DEFAULT, OnUpdateSpeedDefault)
	ON_UPDATE_COMMAND_UI(ID_SPEED_CUSTOM, OnUpdateSpeedCustom)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTEMIX, OnUpdateEditPaste)
	ON_WM_NCMOUSEMOVE()
	ON_COMMAND(ID_BLOCK_START, OnBlockStart)	
	ON_COMMAND(ID_BLOCK_END, OnBlockEnd)
	ON_COMMAND(ID_POPUP_PICKUPROW, OnPickupRow)
	ON_MESSAGE(WM_USER_MIDI_EVENT, OnUserMidiEvent)
	ON_MESSAGE(WM_USER_PLAYER, OnUserPlayerEvent)
	ON_MESSAGE(WM_USER_NOTE_EVENT, OnUserNoteEvent)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// Convert keys 0-F to numbers, -1 = invalid key
static int ConvertKeyToHex(int Key) {

	switch (Key) {
		case 48: case VK_NUMPAD0: return 0x00;
		case 49: case VK_NUMPAD1: return 0x01;
		case 50: case VK_NUMPAD2: return 0x02;
		case 51: case VK_NUMPAD3: return 0x03;
		case 52: case VK_NUMPAD4: return 0x04;
		case 53: case VK_NUMPAD5: return 0x05;
		case 54: case VK_NUMPAD6: return 0x06;
		case 55: case VK_NUMPAD7: return 0x07;
		case 56: case VK_NUMPAD8: return 0x08;
		case 57: case VK_NUMPAD9: return 0x09;
		case 65: return 0x0A;
		case 66: return 0x0B;
		case 67: return 0x0C;
		case 68: return 0x0D;
		case 69: return 0x0E;
		case 70: return 0x0F;

		case KEY_DOT:
		case KEY_DASH:
			return 0x80;
	}

	return -1;
}

// CFamiTrackerView construction/destruction

CFamiTrackerView::CFamiTrackerView() : 
	m_iClipboard(0),
	m_iMoveKeyStepping(1),
	m_iInsertKeyStepping(1),
	m_bEditEnable(false),
	m_bMaskInstrument(false),
	m_bMaskVolume(true),
	m_bSwitchToInstrument(false),
	m_iOctave(3),
	m_iLastNote(NONE),		// // // 0CC-FT
	m_iLastVolume(MAX_VOLUME),
	m_iLastInstrument(MAX_INSTRUMENTS),
	m_iLastEffect(EF_NONE),		// // // 0CC-FT
	m_iLastEffectParam(0),		// // // 0CC-FT
	m_iSwitchToInstrument(-1),
	m_bFollowMode(true),
	m_iAutoArpPtr(0),
	m_iLastAutoArpPtr(0),
	m_iAutoArpKeyCount(0),
	m_iMenuChannel(-1),
	m_iKeyboardNote(-1),
	m_nDropEffect(DROPEFFECT_NONE),
	m_bDragSource(false),
	m_pPatternEditor(new CPatternEditor())
{
	memset(m_bMuteChannels, 0, sizeof(bool) * MAX_CHANNELS);
	memset(m_iActiveNotes, 0, sizeof(int) * MAX_CHANNELS);
	memset(m_cKeyList, 0, sizeof(char) * 256);
	memset(m_iArpeggiate, 0, sizeof(int) * MAX_CHANNELS);

	// Register this object in the sound generator
	CSoundGen *pSoundGen = theApp.GetSoundGenerator();
	ASSERT_VALID(pSoundGen);

	pSoundGen->AssignView(this);
}

CFamiTrackerView::~CFamiTrackerView()
{
	// Release allocated objects
	SAFE_RELEASE(m_pPatternEditor);
}



// CFamiTrackerView diagnostics

#ifdef _DEBUG
void CFamiTrackerView::AssertValid() const
{
	CView::AssertValid();
}

void CFamiTrackerView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CFamiTrackerDoc* CFamiTrackerView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CFamiTrackerDoc)));
	return (CFamiTrackerDoc*)m_pDocument;
}
#endif //_DEBUG


//
// Static functions
//

CFamiTrackerView *CFamiTrackerView::GetView()
{
	CFrameWnd *pFrame = static_cast<CFrameWnd*>(AfxGetApp()->m_pMainWnd);
	ASSERT_VALID(pFrame);

	CView *pView = pFrame->GetActiveView();

	if (!pView)
		return NULL;

	// Fail if view is of wrong kind
	// (this could occur with splitter windows, or additional
	// views on a single document
	if (!pView->IsKindOf(RUNTIME_CLASS(CFamiTrackerView)))
		return NULL;

	return static_cast<CFamiTrackerView*>(pView);
}

// Creation / destroy

int CFamiTrackerView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// Install a timer for screen updates, 20ms
	SetTimer(TMR_UPDATE, 20, NULL);

	m_DropTarget.Register(this);

	// Setup pattern editor
	m_pPatternEditor->ApplyColorScheme();

	// Create clipboard format
	m_iClipboard = ::RegisterClipboardFormat(CLIPBOARD_ID);

	if (m_iClipboard == 0)
		AfxMessageBox(IDS_CLIPBOARD_ERROR, MB_ICONERROR);

	return 0;
}

void CFamiTrackerView::OnDestroy()
{
	// Kill timers
	KillTimer(TMR_UPDATE);
	KillTimer(TMR_SCROLL);

	CView::OnDestroy();
}


// CFamiTrackerView message handlers

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tracker drawing routines
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CFamiTrackerView::OnDraw(CDC* pDC)
{
	// How should we protect the DC in this method?

	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	// Check document
	if (!pDoc->IsFileLoaded()) {
		LPCTSTR str = _T("No module loaded.");
		pDC->FillSolidRect(0, 0, m_iWindowWidth, m_iWindowHeight, 0x000000);
		pDC->SetTextColor(0xFFFFFF);
		CRect textRect(0, 0, m_iWindowWidth, m_iWindowHeight);
		pDC->DrawText(str, _tcslen(str), &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		return;
	}

	// Don't draw when rendering to wave file
	CSoundGen *pSoundGen = theApp.GetSoundGenerator();
	if (pSoundGen == NULL || pSoundGen->IsBackgroundTask())
		return;

	m_pPatternEditor->DrawScreen(pDC, this);
}

BOOL CFamiTrackerView::OnEraseBkgnd(CDC* pDC)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	// Check document
	if (!pDoc->IsFileLoaded())
		return FALSE;

	// Called when the background should be erased
	m_pPatternEditor->CreateBackground(pDC);

	return FALSE;
}

void CFamiTrackerView::SetupColors()
{
	// Color scheme has changed
	CMainFrame *pMainFrame = static_cast<CMainFrame*>(GetParentFrame());
	m_pPatternEditor->ApplyColorScheme();

	m_pPatternEditor->InvalidatePatternData();
	m_pPatternEditor->InvalidateBackground();
	RedrawPatternEditor();

	// Frame editor
	CFrameEditor *pFrameEditor = GetFrameEditor();
	pFrameEditor->SetupColors();
	pFrameEditor->RedrawFrameEditor();

	pMainFrame->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
}

void CFamiTrackerView::UpdateMeters()
{
	// TODO: Change this to use the ordinary drawing routines
	m_csDrawLock.Lock();

	CDC *pDC = GetDC();
	if (pDC && pDC->m_hDC) {
		m_pPatternEditor->DrawMeters(pDC);
		ReleaseDC(pDC);
	}

	m_csDrawLock.Unlock();
}

void CFamiTrackerView::InvalidateCursor()
{
	// Cursor has moved, redraw screen	
	m_pPatternEditor->InvalidateCursor();
	RedrawPatternEditor();
	RedrawFrameEditor();
}

void CFamiTrackerView::InvalidateHeader()
{
	// Header area has changed (channel muted etc...)
	m_pPatternEditor->InvalidateHeader();
	RedrawWindow(m_pPatternEditor->GetHeaderRect(), NULL, RDW_INVALIDATE);
}

void CFamiTrackerView::InvalidatePatternEditor()
{
	// Pattern data has changed, redraw screen
	RedrawPatternEditor();
	// ??? TODO do we need this??
	RedrawFrameEditor();
}

void CFamiTrackerView::InvalidateFrameEditor()
{
	// Frame data has changed, redraw frame editor

	CFrameEditor *pFrameEditor = GetFrameEditor();
	pFrameEditor->InvalidateFrameData();

	RedrawFrameEditor();
	// Update pattern editor according to selected frame
	RedrawPatternEditor();
}

void CFamiTrackerView::RedrawPatternEditor()
{
	// Redraw the pattern editor, partial or full if needed
	bool NeedErase = m_pPatternEditor->CursorUpdated();

	if (NeedErase)
		RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
	else
		RedrawWindow(m_pPatternEditor->GetInvalidatedRect(), NULL, RDW_INVALIDATE);
}

void CFamiTrackerView::RedrawFrameEditor()
{
	// Redraw the frame editor	
	CFrameEditor *pFrameEditor = GetFrameEditor();
	pFrameEditor->RedrawFrameEditor();
}

CFrameEditor *CFamiTrackerView::GetFrameEditor() const
{
	CMainFrame *pMainFrame = static_cast<CMainFrame*>(GetParentFrame());
	ASSERT_VALID(pMainFrame);
	CFrameEditor *pFrameEditor = pMainFrame->GetFrameEditor();
	ASSERT_VALID(pFrameEditor);
	return pFrameEditor;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// General
////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CFamiTrackerView::OnUserMidiEvent(WPARAM wParam, LPARAM lParam)
{
	TranslateMidiMessage();
	return 0;
}

LRESULT CFamiTrackerView::OnUserPlayerEvent(WPARAM wParam, LPARAM lParam)
{
	// Player is playing
	// TODO clean up
	int Frame = (int)wParam;
	int Row = (int)lParam;

	m_pPatternEditor->InvalidateCursor();
	RedrawPatternEditor();

	//m_pPatternEditor->AdjustCursor();	// Fix frame editor cursor
	RedrawFrameEditor();
	return 0;
}

LRESULT CFamiTrackerView::OnUserNoteEvent(WPARAM wParam, LPARAM lParam)
{
	int Channel = wParam;
	int Note = lParam;

	RegisterKeyState(Channel, Note);

	return 0;
}

void CFamiTrackerView::CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType)
{
	// Window size has changed
	m_iWindowWidth	= lpClientRect->right - lpClientRect->left;
	m_iWindowHeight	= lpClientRect->bottom - lpClientRect->top;

	m_iWindowWidth  -= ::GetSystemMetrics(SM_CXEDGE) * 2;
	m_iWindowHeight -= ::GetSystemMetrics(SM_CYEDGE) * 2;

	m_pPatternEditor->SetWindowSize(m_iWindowWidth, m_iWindowHeight);
	m_pPatternEditor->InvalidateBackground();
	// Update cursor since first visible channel might change
	m_pPatternEditor->CursorUpdated();
	
	CView::CalcWindowRect(lpClientRect, nAdjustType);
}

// Scroll

void CFamiTrackerView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	m_pPatternEditor->OnVScroll(nSBCode, nPos);
	InvalidateCursor();

	CView::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CFamiTrackerView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	m_pPatternEditor->OnHScroll(nSBCode, nPos);
	InvalidateCursor();

	CView::OnHScroll(nSBCode, nPos, pScrollBar);
}

// Mouse

void CFamiTrackerView::OnRButtonUp(UINT nFlags, CPoint point)
{
	// Popup menu
	CRect WinRect;
	CMenu *pPopupMenu, PopupMenuBar;
	
	if (m_pPatternEditor->CancelDragging()) {
		InvalidateCursor();
		CView::OnRButtonUp(nFlags, point);
		return;
	}

	m_pPatternEditor->OnMouseRDown(point);

	GetWindowRect(WinRect);

	if (m_pPatternEditor->IsOverHeader(point)) {
		// Pattern header
		m_iMenuChannel = m_pPatternEditor->GetChannelAtPoint(point.x);
		PopupMenuBar.LoadMenu(IDR_PATTERN_HEADER_POPUP);
		pPopupMenu = PopupMenuBar.GetSubMenu(0);
		pPopupMenu->TrackPopupMenu(TPM_RIGHTBUTTON, point.x + WinRect.left, point.y + WinRect.top, this);
	}
	else {
		// Pattern area
		m_iMenuChannel = -1;
		PopupMenuBar.LoadMenu(IDR_PATTERN_POPUP);
		pPopupMenu = PopupMenuBar.GetSubMenu(0);
		// Send messages to parent in order to get the menu options working
		pPopupMenu->TrackPopupMenu(TPM_RIGHTBUTTON, point.x + WinRect.left, point.y + WinRect.top, GetParentFrame());
	}
	
	CView::OnRButtonUp(nFlags, point);
}

void CFamiTrackerView::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetTimer(TMR_SCROLL, 10, NULL);

	m_pPatternEditor->OnMouseDown(point);
	SetCapture();	// Capture mouse 
	InvalidateCursor();

	if (m_pPatternEditor->IsOverHeader(point))
		InvalidateHeader();

	CView::OnLButtonDown(nFlags, point);
}

void CFamiTrackerView::OnLButtonUp(UINT nFlags, CPoint point)
{
	KillTimer(TMR_SCROLL);

	m_pPatternEditor->OnMouseUp(point);
	ReleaseCapture();	// Release mouse

	InvalidateCursor();
	InvalidateHeader();

	/*
	if (m_bControlPressed && !m_pPatternEditor->IsSelecting()) {
		m_pPatternEditor->JumpToRow(m_pPatternEditor->GetRow());
		theApp.GetSoundGenerator()->StartPlayer(MODE_PLAY_CURSOR);
	}
	*/

	CView::OnLButtonUp(nFlags, point);
}

void CFamiTrackerView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (theApp.GetSettings()->General.bDblClickSelect && !m_pPatternEditor->IsOverHeader(point))
		return;

	m_pPatternEditor->OnMouseDblClk(point);
	InvalidateCursor();
	CView::OnLButtonDblClk(nFlags, point);
}

void CFamiTrackerView::OnMouseMove(UINT nFlags, CPoint point)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	if (nFlags & MK_LBUTTON) {
		// Left button down
		m_pPatternEditor->OnMouseMove(nFlags, point);
		InvalidateCursor();
	}
	else {
		// Left button up
		if (m_pPatternEditor->OnMouseHover(nFlags, point))
			InvalidateHeader();
	}
	
	CView::OnMouseMove(nFlags, point);
}

BOOL CFamiTrackerView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CPatternAction *pAction = NULL;

	bool bShiftPressed = IsShiftPressed();
	bool bControlPressed = IsControlPressed();

	if (bControlPressed && bShiftPressed) {
		if (zDelta < 0)
			m_pPatternEditor->NextFrame();
		else
			m_pPatternEditor->PreviousFrame();
	}
	if (bControlPressed) {
		if (zDelta > 0) {
			pAction = new CPatternAction(CPatternAction::ACT_TRANSPOSE);
			pAction->SetTranspose(TRANSPOSE_INC_NOTES);
		}
		else {
			pAction = new CPatternAction(CPatternAction::ACT_TRANSPOSE);
			pAction->SetTranspose(TRANSPOSE_DEC_NOTES);
		}
	}
	else if (bShiftPressed) {
		if (zDelta > 0) {
			pAction = new CPatternAction(CPatternAction::ACT_SCROLL_VALUES);
			pAction->SetScroll(1);
		}
		else {
			pAction = new CPatternAction(CPatternAction::ACT_SCROLL_VALUES);
			pAction->SetScroll(-1);
		}
	}
	else
		m_pPatternEditor->OnMouseScroll(zDelta);

	if (pAction != NULL) {
		AddAction(pAction);
	}
	else
		InvalidateCursor();
	
	return CView::OnMouseWheel(nFlags, zDelta, pt);
}

// End of mouse

void CFamiTrackerView::OnKillFocus(CWnd* pNewWnd)
{
	CView::OnKillFocus(pNewWnd);
	m_bHasFocus = false;
	m_pPatternEditor->SetFocus(false);
	InvalidateCursor();
}

void CFamiTrackerView::OnSetFocus(CWnd* pOldWnd)
{
	CView::OnSetFocus(pOldWnd);
	m_bHasFocus = true;
	m_pPatternEditor->SetFocus(true);
	InvalidateCursor();
}

void CFamiTrackerView::OnTimer(UINT_PTR nIDEvent)
{
	// Timer callback function
	switch (nIDEvent) {
		// Drawing updates when playing
		case TMR_UPDATE: 
			PeriodicUpdate();
			break;

		// Auto-scroll timer
		case TMR_SCROLL:
			if (m_pPatternEditor->ScrollTimerCallback()) {
				// Redraw entire since pattern layout might change
				RedrawPatternEditor();
			}
			break;
	}

	CView::OnTimer(nIDEvent);
}

void CFamiTrackerView::PeriodicUpdate()
{
	// Called periodically by an background timer

	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	CMainFrame *pMainFrm = static_cast<CMainFrame*>(GetParentFrame());
	ASSERT_VALID(pMainFrm);

	CSoundGen *pSoundGen = theApp.GetSoundGenerator();

	if (pSoundGen != NULL) {

#ifdef EXPORT_TEST
		if (pSoundGen->IsTestingExport()) {
			DrawExportTestProgress();			
		}
#endif /* EXPORT_TEST */

		// Skip updates when doing background tasks (WAV render for example)
		if (!pSoundGen->IsBackgroundTask()) {

			int PlayTicks = pSoundGen->GetPlayerTicks();
			int PlayTime = (PlayTicks * 10) / pDoc->GetFrameRate();

			// Play time
			int Min = PlayTime / 600;
			int Sec = (PlayTime / 10) % 60;
			int mSec = PlayTime % 10;

			pMainFrm->SetIndicatorTime(Min, Sec, mSec);

			int Frame = pSoundGen->GetPlayerFrame();
			int Row = pSoundGen->GetPlayerRow();

			pMainFrm->SetIndicatorPos(Frame, Row);

			// // //

			if (pDoc->IsFileLoaded())
				UpdateMeters();
		}
	}

	// TODO get rid of static variables
	static int LastNoteState;

	if (LastNoteState != m_iKeyboardNote)
		pMainFrm->ChangeNoteState(m_iKeyboardNote);

	LastNoteState = m_iKeyboardNote;

	// Switch instrument
	if (m_iSwitchToInstrument != -1) {
		SetInstrument(m_iSwitchToInstrument);
		m_iSwitchToInstrument = -1;
	}
}

#ifdef EXPORT_TEST
void CFamiTrackerView::DrawExportTestProgress()
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	CSoundGen *pSoundGen = theApp.GetSoundGenerator();

	m_csDrawLock.Lock();

	CDC *pDC = GetDC();
	if (pDC && pDC->m_hDC) {
		int x = m_iWindowWidth / 2 - 100;
		int y = m_iWindowHeight / 2 - 20;
		int frame = pSoundGen->GetPlayerFrame();
		int frameCount = pDoc->GetFrameCount(pSoundGen->GetPlayerTrack());
		pDC->Rectangle(x, y, x + 200, y + 40);
		CString str;
		str.Format(_T("Frame %i of %i"), frame, frameCount);
		CRect rect(x, y + 5, x + 200, y + 30);
		pDC->DrawText(str, rect, DT_CENTER | DT_VCENTER);
		pDC->FillSolidRect(x + 10, y + 25, ((frame * 180) / frameCount), 10, 0x00AA00);
		ReleaseDC(pDC);
	}

	m_csDrawLock.Unlock();
}
#endif /* EXPORT_TEST */

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Menu commands
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CFamiTrackerView::OnEditCopy()
{
	bool bShiftPressed = IsShiftPressed();

	CClipboard Clipboard(this, m_iClipboard);

	if (!Clipboard.IsOpened()) {
		AfxMessageBox(IDS_CLIPBOARD_OPEN_ERROR);
		return;
	}
	
	std::unique_ptr<CPatternClipData> pClipData(m_pPatternEditor->Copy());		// // //

	SIZE_T Size = pClipData->GetAllocSize();
	HGLOBAL hMem = Clipboard.AllocMem(Size);

	if (hMem != NULL) {
		pClipData->ToMem(hMem);
		// Set clipboard for internal data, hMem may not be used after this point
		Clipboard.SetData(hMem);
	}

	// Copy volume values
	if (bShiftPressed) {
		CopyVolumeColumn();
	}
}

void CFamiTrackerView::OnEditCut()
{
	OnEditCopy();
	OnEditDelete();
}

void CFamiTrackerView::OnEditPaste()
{
	CClipboard Clipboard(this, m_iClipboard);

	if (!Clipboard.IsOpened()) {
		AfxMessageBox(IDS_CLIPBOARD_OPEN_ERROR);
		return;
	}

	if (!Clipboard.IsDataAvailable()) {
		AfxMessageBox(IDS_CLIPBOARD_NOT_AVALIABLE);
		::CloseClipboard();
		return;
	}

	HGLOBAL hMem = Clipboard.GetData();
	if (hMem != NULL) {
		CPatternClipData *pClipData = new CPatternClipData();
		pClipData->FromMem(hMem);
		// Create an undo point
		CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_EDIT_PASTE);
		pAction->SetPaste(pClipData);
		AddAction(pAction);
	}
}

void CFamiTrackerView::OnEditPastemix()
{
	CClipboard Clipboard(this, m_iClipboard);

	if (!Clipboard.IsOpened()) {
		AfxMessageBox(IDS_CLIPBOARD_OPEN_ERROR);
		return;
	}

	if (!Clipboard.IsDataAvailable()) {
		AfxMessageBox(IDS_CLIPBOARD_NOT_AVALIABLE);
		::CloseClipboard();
		return;
	}

	HGLOBAL hMem = Clipboard.GetData();

	if (hMem != NULL) {
		CPatternClipData *pClipData = new CPatternClipData();
		pClipData->FromMem(hMem);

		// Add an undo point
		CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_EDIT_PASTE_MIX);
		pAction->SetPaste(pClipData);
		AddAction(pAction);
	}
}

void CFamiTrackerView::OnEditDelete()
{
	AddAction(new CPatternAction(CPatternAction::ACT_EDIT_DELETE));
}

void CFamiTrackerView::OnTrackerEdit()
{
	m_bEditEnable = !m_bEditEnable;

	if (m_bEditEnable)
		GetParentFrame()->SetMessageText(IDS_EDIT_MODE);
	else
		GetParentFrame()->SetMessageText(IDS_NORMAL_MODE);
	
	m_pPatternEditor->InvalidateBackground();
	m_pPatternEditor->InvalidateHeader();
	m_pPatternEditor->InvalidateCursor();
	RedrawPatternEditor();
}

void CFamiTrackerView::OnTrackerPal()
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int Machine = PAL;
	pDoc->SetMachine(Machine);
	theApp.GetSoundGenerator()->LoadMachineSettings(Machine, pDoc->GetEngineSpeed());		// // //
}

void CFamiTrackerView::OnTrackerNtsc()
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int Machine = NTSC;
	pDoc->SetMachine(Machine);
	theApp.GetSoundGenerator()->LoadMachineSettings(Machine, pDoc->GetEngineSpeed());		// // //
}

void CFamiTrackerView::OnSpeedCustom()
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	CSpeedDlg SpeedDlg;

	int Machine = pDoc->GetMachine();
	int Speed = pDoc->GetEngineSpeed();
	if (Speed == 0)
		Speed = (Machine == NTSC) ? CAPU::FRAME_RATE_NTSC : CAPU::FRAME_RATE_PAL;
	Speed = SpeedDlg.GetSpeedFromDlg(Speed);

	if (Speed == 0)
		return;

	pDoc->SetEngineSpeed(Speed);
	theApp.GetSoundGenerator()->LoadMachineSettings(Machine, Speed);		// // //
}

void CFamiTrackerView::OnSpeedDefault()
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int Speed = 0;
	pDoc->SetEngineSpeed(Speed);
	theApp.GetSoundGenerator()->LoadMachineSettings(pDoc->GetMachine(), Speed);		// // //
}

void CFamiTrackerView::OnTransposeDecreasenote()
{
	CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_TRANSPOSE);
	pAction->SetTranspose(TRANSPOSE_DEC_NOTES);
	AddAction(pAction);
}

void CFamiTrackerView::OnTransposeDecreaseoctave()
{
	CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_TRANSPOSE);
	pAction->SetTranspose(TRANSPOSE_DEC_OCTAVES);
	AddAction(pAction);
}

void CFamiTrackerView::OnTransposeIncreasenote()
{
	CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_TRANSPOSE);
	pAction->SetTranspose(TRANSPOSE_INC_NOTES);
	AddAction(pAction);
}

void CFamiTrackerView::OnTransposeIncreaseoctave()
{
	CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_TRANSPOSE);
	pAction->SetTranspose(TRANSPOSE_INC_OCTAVES);
	AddAction(pAction);
}

void CFamiTrackerView::OnDecreaseValues()
{
	CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_SCROLL_VALUES);
	pAction->SetScroll(-1);
	AddAction(pAction);
}

void CFamiTrackerView::OnIncreaseValues()
{
	CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_SCROLL_VALUES);
	pAction->SetScroll(1);
	AddAction(pAction);
}

void CFamiTrackerView::OnEditInstrumentMask()
{
	m_bMaskInstrument = !m_bMaskInstrument;
}

void CFamiTrackerView::OnEditVolumeMask()
{
	m_bMaskVolume = !m_bMaskVolume;
}

void CFamiTrackerView::OnEditSelectall()
{
	m_pPatternEditor->SelectAll();
	InvalidateCursor();
}

void CFamiTrackerView::OnTrackerPlayrow()
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	const int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();
	const int Frame = m_pPatternEditor->GetFrame();
	const int Row = m_pPatternEditor->GetRow();
	const int Channels = pDoc->GetAvailableChannels();

	for (int i = 0; i < Channels; ++i) {
		stChanNote Note;
		pDoc->GetNoteData(Track, Frame, i, Row, &Note);
		if (!m_bMuteChannels[i])
			theApp.GetSoundGenerator()->QueueNote(i, Note, NOTE_PRIO_1);
	}

	m_pPatternEditor->MoveDown(1);
	InvalidateCursor();
}

void CFamiTrackerView::CopyVolumeColumn()
{
	CString str;
	m_pPatternEditor->GetVolumeColumn(str);

	CClipboard Clipboard(this, CF_TEXT);

	if (!Clipboard.IsOpened()) {
		AfxMessageBox(IDS_CLIPBOARD_OPEN_ERROR);
		return;
	}

	if (!Clipboard.SetDataPointer(str.GetBuffer(), str.GetLength() + 1)) {
		AfxMessageBox(IDS_CLIPBOARD_COPY_ERROR);
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI updates
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CFamiTrackerView::OnInitialUpdate()
{
	//
	// Called when the view is first attached to a document,
	// when a file is loaded or new document is created.
	//

	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	CMainFrame *pMainFrame = static_cast<CMainFrame*>(GetParentFrame());
	ASSERT_VALID(pMainFrame);

	CFrameEditor *pFrameEditor = GetFrameEditor();

	TRACE1("View: OnInitialUpdate (%s)\n", pDoc->GetTitle());

	// Setup order window
	pFrameEditor->AssignDocument(pDoc, this);

	// Notify the pattern view about new document & view
	m_pPatternEditor->SetDocument(pDoc, this);
	m_pPatternEditor->ResetCursor();

	// Always start with first track
	pMainFrame->SelectTrack(0);

	// Update mainframe with new document settings
	pMainFrame->UpdateInstrumentList();
	pMainFrame->SetSongInfo(pDoc->GetSongName(), pDoc->GetSongArtist(), pDoc->GetSongCopyright());
	pMainFrame->UpdateTrackBox();
	pMainFrame->DisplayOctave();
	pMainFrame->UpdateControls();
	pMainFrame->ResetUndo();
	pMainFrame->ResizeFrameWindow();

	// Fetch highlight
	int FirstHighlight = pDoc->GetFirstHighlight();
	int SecondHighlight = pDoc->GetSecondHighlight();

	m_pPatternEditor->SetHighlight(FirstHighlight, SecondHighlight);

	pMainFrame->SetFirstHighlightRow(FirstHighlight);
	pMainFrame->SetSecondHighlightRow(SecondHighlight);

	// Follow mode
	SetFollowMode(theApp.GetSettings()->FollowMode);

	// Setup speed/tempo (TODO remove?)
	theApp.GetSoundGenerator()->ResetState();
	theApp.GetSoundGenerator()->ResetTempo();

	// Default
	SetInstrument(0);

	// Unmute all channels
	for (int i = 0; i < MAX_CHANNELS; ++i) {
		SetChannelMute(i, false);
	}

	// Draw screen
	m_pPatternEditor->InvalidateBackground();
	m_pPatternEditor->InvalidatePatternData();
	RedrawPatternEditor();

	pFrameEditor->CancelSelection();
	pFrameEditor->InvalidateFrameData();
	RedrawFrameEditor();

	if (theApp.m_pMainWnd->IsWindowVisible())
		SetFocus();

	// Display comment box
	if (pDoc->ShowCommentOnOpen())
		pMainFrame->PostMessage(WM_COMMAND, ID_MODULE_COMMENTS);

	// Call OnUpdate
	//CView::OnInitialUpdate();	
}

void CFamiTrackerView::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/)
{
	// Called when the document has changed
	CMainFrame *pMainFrm = static_cast<CMainFrame*>(GetParentFrame());
	ASSERT_VALID(pMainFrm);

	// Handle new flags
	switch (lHint) {
	// Track has been added, removed or changed
	case UPDATE_TRACK:
		if (theApp.IsPlaying())
			theApp.StopPlayer();
		pMainFrm->UpdateTrackBox();
		m_pPatternEditor->InvalidateBackground();
		m_pPatternEditor->InvalidatePatternData();
		m_pPatternEditor->InvalidateHeader();
		RedrawPatternEditor();
		break;
	// Pattern data has been edited
	case UPDATE_PATTERN:
		m_pPatternEditor->InvalidatePatternData();
		RedrawPatternEditor();
		break;
	// Frame data has been edited
	case UPDATE_FRAME:
		InvalidateFrameEditor();

		m_pPatternEditor->InvalidatePatternData();
		RedrawPatternEditor();
		break;
	// Instrument has been added / removed
	case UPDATE_INSTRUMENT:
		pMainFrm->UpdateInstrumentList();
		m_pPatternEditor->InvalidatePatternData();
		RedrawPatternEditor();
		break;
	// Module properties has changed (including channel count)
	case UPDATE_PROPERTIES:
		// Old
		m_pPatternEditor->ResetCursor();
		//m_pPatternEditor->Modified();
		pMainFrm->ResetUndo();
		pMainFrm->ResizeFrameWindow();

		m_pPatternEditor->InvalidateBackground();	// Channel count might have changed, recalculated pattern layout
		m_pPatternEditor->InvalidatePatternData();
		m_pPatternEditor->InvalidateHeader();
		RedrawPatternEditor();
		break;
	// Row highlight option has changed
	case UPDATE_HIGHLIGHT:
		m_pPatternEditor->SetHighlight(GetDocument()->GetFirstHighlight(), GetDocument()->GetSecondHighlight());
		m_pPatternEditor->InvalidatePatternData();
		RedrawPatternEditor();
		break;
	// Effect columns has changed
	case UPDATE_COLUMNS:
		m_pPatternEditor->InvalidateBackground();	// Recalculate pattern layout
		m_pPatternEditor->InvalidateHeader();
		m_pPatternEditor->InvalidatePatternData();
		RedrawPatternEditor();
		break;
	// Document is closing
	case UPDATE_CLOSE:
		// Old
		pMainFrm->CloseInstrumentEditor();
		break;
	}
}

void CFamiTrackerView::TrackChanged(unsigned int Track)
{
	// Called when the selected track has changed
	CMainFrame *pMainFrm = static_cast<CMainFrame*>(GetParentFrame());
	ASSERT_VALID(pMainFrm);

	if (theApp.IsPlaying())
		theApp.ResetPlayer();

	pMainFrm->ResetUndo();

	m_pPatternEditor->ResetCursor();
	m_pPatternEditor->InvalidatePatternData();
	m_pPatternEditor->InvalidateBackground();
	m_pPatternEditor->InvalidateHeader();
	RedrawPatternEditor();

	pMainFrm->UpdateTrackBox();

	InvalidateFrameEditor();
	RedrawFrameEditor();

	// Update setting boxes
	pMainFrm->UpdateControls();
}

// GUI elements updates

void CFamiTrackerView::OnUpdateEditInstrumentMask(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bMaskInstrument ? 1 : 0);
}

void CFamiTrackerView::OnUpdateEditVolumeMask(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bMaskVolume ? 1 : 0);
}

void CFamiTrackerView::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!m_pPatternEditor->IsSelecting() ? 0 : 1);
}

void CFamiTrackerView::OnUpdateEditCut(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!m_pPatternEditor->IsSelecting() ? 0 : 1);
}

void CFamiTrackerView::OnUpdateEditPaste(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(IsClipboardAvailable() ? 1 : 0);
}

void CFamiTrackerView::OnUpdateEditDelete(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(!m_pPatternEditor->IsSelecting() ? 0 : 1);
}

void CFamiTrackerView::OnUpdateTrackerEdit(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bEditEnable ? 1 : 0);
}

void CFamiTrackerView::OnUpdateTrackerPal(CCmdUI *pCmdUI)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	pCmdUI->Enable(pDoc->GetExpansionChip() == SNDCHIP_NONE);
	UINT item = pDoc->GetMachine() == PAL ? ID_TRACKER_PAL : ID_TRACKER_NTSC;
	if (pCmdUI->m_pMenu != NULL)
		pCmdUI->m_pMenu->CheckMenuRadioItem(ID_TRACKER_NTSC, ID_TRACKER_PAL, item, MF_BYCOMMAND);
}

void CFamiTrackerView::OnUpdateTrackerNtsc(CCmdUI *pCmdUI)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	UINT item = pDoc->GetMachine() == NTSC ? ID_TRACKER_NTSC : ID_TRACKER_PAL;
	if (pCmdUI->m_pMenu != NULL)
		pCmdUI->m_pMenu->CheckMenuRadioItem(ID_TRACKER_NTSC, ID_TRACKER_PAL, item, MF_BYCOMMAND);
}

void CFamiTrackerView::OnUpdateSpeedDefault(CCmdUI *pCmdUI)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	pCmdUI->SetCheck(pDoc->GetEngineSpeed() == 0);
}

void CFamiTrackerView::OnUpdateSpeedCustom(CCmdUI *pCmdUI)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);	

	pCmdUI->SetCheck(pDoc->GetEngineSpeed() != 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tracker playing routines
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CFamiTrackerView::PlayerTick()
{
	// Callback from sound thread (TODO move this to sound thread?)

	if (m_iAutoArpKeyCount == 1 || !theApp.GetSettings()->Midi.bMidiArpeggio)
		return;

	// auto arpeggio
	int OldPtr = m_iAutoArpPtr;
	do {
		m_iAutoArpPtr = (m_iAutoArpPtr + 1) & 127;				
		if (m_iAutoArpNotes[m_iAutoArpPtr] == 1) {
			m_iLastAutoArpPtr = m_iAutoArpPtr;
			m_iArpeggiate[m_pPatternEditor->GetChannel()] = m_iAutoArpPtr;
			break;
		}
		else if (m_iAutoArpNotes[m_iAutoArpPtr] == 2) {
			m_iAutoArpNotes[m_iAutoArpPtr] = 0;
		}
	}
	while (m_iAutoArpPtr != OldPtr);
}

bool CFamiTrackerView::PlayerGetNote(int Track, int Frame, int Channel, int Row, stChanNote &NoteData)
{
	CFamiTrackerDoc *pDoc = GetDocument();
	bool ValidCommand = false;

	pDoc->GetNoteData(Track, Frame, Channel, Row, &NoteData);
	
	if (!IsChannelMuted(Channel)) {
		// Let view know what is about to play
		PlayerPlayNote(Channel, &NoteData);
		ValidCommand = true;
	}
	else {
		// These effects will pass even if the channel is muted
		const int PASS_EFFECTS[] = {EF_HALT, EF_JUMP, EF_SPEED, EF_SKIP, EF_SN_CONTROL};		// // //
		int Columns = pDoc->GetEffColumns(Track, Channel) + 1;
		
		NoteData.Note		= HALT;
		NoteData.Octave		= 0;
		NoteData.Instrument = 0;

		for (int j = 0; j < Columns; ++j) {
			bool Clear = true;
			for (int k = 0; k < sizeof(PASS_EFFECTS) / sizeof(*PASS_EFFECTS); ++k) {		// // //
				if (NoteData.EffNumber[j] == PASS_EFFECTS[k]) {
					ValidCommand = true;
					Clear = false;
				}
			}
			if (Clear)
				NoteData.EffNumber[j] = EF_NONE;
		}
	}

	return ValidCommand;
}

void CFamiTrackerView::PlayerPlayNote(int Channel, stChanNote *pNote)
{
	// Callback from sound thread

	if (pNote->Instrument < MAX_INSTRUMENTS && pNote->Note > 0 && Channel == m_pPatternEditor->GetChannel() && m_bSwitchToInstrument) {
		m_iSwitchToInstrument = pNote->Instrument;
	}
}

unsigned int CFamiTrackerView::GetSelectedFrame() const 
{ 
	return m_pPatternEditor->GetFrame(); 
}

unsigned int CFamiTrackerView::GetSelectedChannel() const 
{ 
	return m_pPatternEditor->GetChannel();
}

unsigned int CFamiTrackerView::GetSelectedRow() const
{
	return m_pPatternEditor->GetRow();
}

void CFamiTrackerView::SetFollowMode(bool Mode)
{
	m_bFollowMode = Mode;
	m_pPatternEditor->SetFollowMove(Mode);
}

bool CFamiTrackerView::GetFollowMode() const
{ 
	return m_bFollowMode; 
}

int	CFamiTrackerView::GetAutoArpeggio(unsigned int Channel)
{
	// Return and reset next arpeggio note
	int ret = m_iArpeggiate[Channel];
	m_iArpeggiate[Channel] = 0;
	return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// General
////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CFamiTrackerView::RegisterKeyState(int Channel, int Note)
{
	CFamiTrackerDoc *pDoc = GetDocument();
	CTrackerChannel *pChannel = pDoc->GetChannel(m_pPatternEditor->GetChannel());

	if (pChannel->GetID() == Channel)
		m_iKeyboardNote = Note;

	/*
	if (Channel == m_pPatternEditor->GetChannel())
		m_iKeyboardNote = Note;
		*/
}

void CFamiTrackerView::SelectNextFrame()
{
	m_pPatternEditor->NextFrame();
	InvalidateFrameEditor();
}

void CFamiTrackerView::SelectPrevFrame()
{
	m_pPatternEditor->PreviousFrame();
	InvalidateFrameEditor();
}

void CFamiTrackerView::SelectFirstFrame()
{
	m_pPatternEditor->MoveToFrame(0);
	InvalidateFrameEditor();
}

void CFamiTrackerView::SelectLastFrame()
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();
	m_pPatternEditor->MoveToFrame(pDoc->GetFrameCount(Track) - 1);
	InvalidateFrameEditor();
}

void CFamiTrackerView::MoveCursorNextChannel()
{
	m_pPatternEditor->NextChannel();
	InvalidateCursor();
}

void CFamiTrackerView::MoveCursorPrevChannel()
{
	m_pPatternEditor->PreviousChannel();
	InvalidateCursor();
}

void CFamiTrackerView::SelectFrame(unsigned int Frame)
{
	ASSERT(Frame < MAX_FRAMES);
	m_pPatternEditor->MoveToFrame(Frame);
	InvalidateCursor();
}

void CFamiTrackerView::SelectChannel(unsigned int Channel)
{
	ASSERT(Channel < MAX_CHANNELS);
	m_pPatternEditor->MoveToChannel(Channel);
	InvalidateCursor();
}

void CFamiTrackerView::SelectFrameChannel(unsigned int Frame, unsigned int Channel)
{
	ASSERT(Frame < MAX_FRAMES);
	ASSERT(Channel < MAX_CHANNELS);
	m_pPatternEditor->MoveToFrame(Frame);
	m_pPatternEditor->MoveToChannel(Channel);
	// This method does no redrawing
}

void CFamiTrackerView::ToggleChannel(unsigned int Channel)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	if (Channel >= pDoc->GetAvailableChannels())
		return;

	SetChannelMute(Channel, !m_bMuteChannels[Channel]);
	HaltNoteSingle(Channel);
	InvalidateHeader();
}

void CFamiTrackerView::SoloChannel(unsigned int Channel)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int channels = pDoc->GetAvailableChannels();

	if (Channel >= unsigned(channels))
		return;

	if (IsChannelSolo(Channel)) {
		// Revert channels
		for (int i = 0; i < channels; ++i) {
			SetChannelMute(i, false);
		}
	}
	else {
		// Solo selected channel
		for (int i = 0; i < channels; ++i) {
			if (i != Channel) {
				SetChannelMute(i, true);
				HaltNote(i);
			}
		}
		SetChannelMute(Channel, false);
	}

	InvalidateHeader();
}

void CFamiTrackerView::UnmuteAllChannels()
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int channels = pDoc->GetAvailableChannels();

	for (int i = 0; i < channels; ++i) {
		SetChannelMute(i, false);
	}

	InvalidateHeader();
}

bool CFamiTrackerView::IsChannelSolo(unsigned int Channel) const
{
	// Returns true if Channel is the only active channel 
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int channels = pDoc->GetAvailableChannels();

	for (int i = 0; i < channels; ++i) {
		if (m_bMuteChannels[i] == false && i != Channel)
			return false;
	}
	return true;
}

void CFamiTrackerView::SetChannelMute(int Channel, bool bMute)
{
	m_bMuteChannels[Channel] = bMute;
}

bool CFamiTrackerView::IsChannelMuted(unsigned int Channel) const
{
	ASSERT(Channel < MAX_CHANNELS);
	return m_bMuteChannels[Channel];
}

void CFamiTrackerView::SetInstrument(int Instrument)
{
	// Todo: remove this
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	CMainFrame *pMainFrm = static_cast<CMainFrame*>(GetParentFrame());
	ASSERT_VALID(pMainFrm);
	ASSERT(Instrument >= 0 && Instrument < MAX_INSTRUMENTS);

	if (Instrument == MAX_INSTRUMENTS)
		return;

	pMainFrm->SelectInstrument(Instrument);
	m_iLastInstrument = GetInstrument(); // Gets actual selected instrument //  Instrument;
}

unsigned int CFamiTrackerView::GetInstrument() const 
{
	CMainFrame *pMainFrm = static_cast<CMainFrame*>(GetParentFrame());
	ASSERT_VALID(pMainFrm);
	return pMainFrm->GetSelectedInstrument();
}

void CFamiTrackerView::StepDown()
{
	// Update pattern length in case it has changed
	m_pPatternEditor->UpdatePatternLength();

	if (m_iInsertKeyStepping)
		m_pPatternEditor->MoveDown(m_iInsertKeyStepping);

	InvalidateCursor();
}

void CFamiTrackerView::InsertNote(int Note, int Octave, int Channel, int Velocity)
{
	// Inserts a note
	const int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();
	const int Frame = m_pPatternEditor->GetFrame();
	const int Row = m_pPatternEditor->GetRow();

	stChanNote Cell;
	GetDocument()->GetNoteData(Track, Frame, Channel, Row, &Cell);

	Cell.Note = Note;

	if (Note != HALT && Note != RELEASE) {
		Cell.Octave	= Octave;

		if (!m_bMaskInstrument)
			Cell.Instrument = GetInstrument();

		if (!m_bMaskVolume) 
			Cell.Vol = m_iLastVolume;

		if (Velocity < 128)
			Cell.Vol = (Velocity / 8);
	}	

	// Quantization
	if (theApp.GetSettings()->Midi.bMidiMasterSync) {
		int Delay = theApp.GetMIDI()->GetQuantization();
		if (Delay > 0) {
			Cell.EffNumber[0] = EF_DELAY;
			Cell.EffParam[0] = Delay;
		}
	}
	
	if (m_bEditEnable) {
		if (Note == HALT)
			m_iLastNote = NOTE_HALT;
		else if (Note == RELEASE)
			m_iLastNote = NOTE_RELEASE;
		else
			m_iLastNote = (Note - 1) + Octave * 12;
		
		CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_EDIT_NOTE);
		pAction->SetNote(Cell);
		if (AddAction(pAction)) {
			const CSettings *pSettings = theApp.GetSettings();
			if ((m_pPatternEditor->GetColumn() == 0) && !theApp.IsPlaying() && (m_iInsertKeyStepping > 0) && !pSettings->Midi.bMidiMasterSync)
				StepDown();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Note playing routines
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void CFamiTrackerView::PlayNote(unsigned int Channel, unsigned int Note, unsigned int Octave, unsigned int Velocity)
{
	// Play a note in a channel
	stChanNote NoteData;

	memset(&NoteData, 0, sizeof(stChanNote));

	NoteData.Note		= Note;
	NoteData.Octave		= Octave;
	NoteData.Vol		= Velocity / 8;
	NoteData.Instrument	= GetInstrument();
/*	
	if (theApp.GetSettings()->General.iEditStyle == EDIT_STYLE3)
		NoteData.Instrument	= m_iLastInstrument;
	else
		NoteData.Instrument	= GetInstrument();
*/

	theApp.GetSoundGenerator()->QueueNote(Channel, NoteData, NOTE_PRIO_2);

	if (Channel >= 2) //sh8bit
	{
		CFamiTrackerDoc *pDoc = GetDocument();
		stChanNote ChanNote;
		int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();
		int Frame = m_pPatternEditor->GetFrame();
		int Row = m_pPatternEditor->GetRow();

		if (Channel == 2)
		{
			pDoc->GetNoteData(Track, Frame, 3, Row, &ChanNote);
			theApp.GetSoundGenerator()->QueueNote(3, ChanNote, NOTE_PRIO_1);
		}
		if (Channel == 3)
		{
			pDoc->GetNoteData(Track, Frame, 2, Row, &ChanNote);
			theApp.GetSoundGenerator()->QueueNote(2, ChanNote, NOTE_PRIO_1);
		}
	}

	if (theApp.GetSettings()->General.bPreviewFullRow) {
		CFamiTrackerDoc *pDoc = GetDocument();
		stChanNote ChanNote;
		int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();
		int Frame = m_pPatternEditor->GetFrame();
		int Row = m_pPatternEditor->GetRow();
		int Channels = pDoc->GetAvailableChannels();

		for (int i = 0; i < Channels; ++i) {
			pDoc->GetNoteData(Track, Frame, i, Row, &ChanNote);
			if (!m_bMuteChannels[i] && i != Channel)
				theApp.GetSoundGenerator()->QueueNote(i, ChanNote, (i == Channel) ? NOTE_PRIO_2 : NOTE_PRIO_1);
		}	
	}
}

void CFamiTrackerView::ReleaseNote(unsigned int Channel)
{
	// Releases a channel
	stChanNote NoteData;
	memset(&NoteData, 0, sizeof(stChanNote));

	NoteData.Note = RELEASE;
	NoteData.Vol = MAX_VOLUME;
	NoteData.Instrument = GetInstrument();

	theApp.GetSoundGenerator()->QueueNote(Channel, NoteData, NOTE_PRIO_2);

	if (theApp.GetSettings()->General.bPreviewFullRow) {
		CFamiTrackerDoc *pDoc = GetDocument();
		int Channels = pDoc->GetChannelCount();
		NoteData.Note = HALT;
		for (int i = 0; i < Channels; ++i) {
			if (i != Channel)
				theApp.GetSoundGenerator()->QueueNote(i, NoteData, NOTE_PRIO_1);
		}
	}
}

void CFamiTrackerView::HaltNote(unsigned int Channel)
{
	// Halts a channel
	stChanNote NoteData;
	memset(&NoteData, 0, sizeof(stChanNote));

	NoteData.Note = HALT;
	NoteData.Vol = MAX_VOLUME;
	NoteData.Instrument = GetInstrument();

	theApp.GetSoundGenerator()->QueueNote(Channel, NoteData, NOTE_PRIO_2);

	if (Channel >= 2) //sh8bit
	{
		CFamiTrackerDoc *pDoc = GetDocument();
		stChanNote ChanNote;
		int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();
		int Frame = m_pPatternEditor->GetFrame();
		int Row = m_pPatternEditor->GetRow();

		if (Channel == 2)
		{
			NoteData.Note = HALT;
			theApp.GetSoundGenerator()->QueueNote(3, NoteData, NOTE_PRIO_1);
		}
		if (Channel == 3)
		{
			NoteData.Note = HALT;
			theApp.GetSoundGenerator()->QueueNote(2, NoteData, NOTE_PRIO_1);
		}
	}

	if (theApp.GetSettings()->General.bPreviewFullRow) {
		CFamiTrackerDoc *pDoc = GetDocument();
		int Channels = pDoc->GetChannelCount();
		for (int i = 0; i < Channels; ++i) {
			if (i != Channel)
				theApp.GetSoundGenerator()->QueueNote(i, NoteData, NOTE_PRIO_1);
		}
	}
}

void CFamiTrackerView::HaltNoteSingle(unsigned int Channel)
{
	// Halts one single channel only
	stChanNote NoteData;
	memset(&NoteData, 0, sizeof(stChanNote));

	NoteData.Note = HALT;
	NoteData.Vol = MAX_VOLUME;
	NoteData.Instrument = GetInstrument();

	theApp.GetSoundGenerator()->QueueNote(Channel, NoteData, NOTE_PRIO_2);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MIDI note handling functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////

static void FixNoise(unsigned int &MidiNote, unsigned int &Octave, unsigned int &Note)
{
	// NES noise channel
	MidiNote = (MidiNote % 16) + 16;
	Octave = GET_OCTAVE(MidiNote);
	Note = GET_NOTE(MidiNote);
}

// Play a note
void CFamiTrackerView::TriggerMIDINote(unsigned int Channel, unsigned int MidiNote, unsigned int Velocity, bool Insert)
{
	// Play a MIDI note
	unsigned int Octave = GET_OCTAVE(MidiNote);
	unsigned int Note = GET_NOTE(MidiNote);

	if (Octave > 7)	// ?
		return;

	CFamiTrackerDoc *pDoc = GetDocument();

	if (pDoc->GetChannel(Channel)->GetID() == CHANID_NOISE)
		FixNoise(MidiNote, Octave, Note);

	m_iActiveNotes[Channel] = MidiNote;

	if (!theApp.GetSettings()->Midi.bMidiVelocity) {
		if (theApp.GetSettings()->General.iEditStyle != EDIT_STYLE3)
			Velocity = 127;
		else {
			Velocity = m_iLastVolume * 8;
		}
	}

	PlayNote(Channel, Note, Octave, Velocity);

	if (Insert)
		InsertNote(Note, Octave, Channel, Velocity + 1);

	m_iAutoArpNotes[MidiNote] = 1;
	m_iAutoArpPtr = MidiNote;
	m_iLastAutoArpPtr = m_iAutoArpPtr;

	UpdateArpDisplay();

	m_iLastMIDINote = MidiNote;

	m_iAutoArpKeyCount = 0;

	for (int i = 0; i < 128; ++i) {
		if (m_iAutoArpNotes[i] == 1)
			++m_iAutoArpKeyCount;
	}

	TRACE("%i: Trigger note %i on channel %i\n", GetTickCount(), MidiNote, Channel);
}

// Cut the currently playing note
void CFamiTrackerView::CutMIDINote(unsigned int Channel, unsigned int MidiNote, bool InsertCut)
{
	// Cut a MIDI note
	unsigned int Octave = GET_OCTAVE(MidiNote);
	unsigned int Note = GET_NOTE(MidiNote);

	ASSERT(Channel < 28);
	ASSERT(MidiNote < 128);

	CFamiTrackerDoc* pDoc = GetDocument();

	if (pDoc->GetChannel(Channel)->GetID() == CHANID_NOISE)
		FixNoise(MidiNote, Octave, Note);

	m_iActiveNotes[Channel] = 0;
	m_iAutoArpNotes[MidiNote] = 2;

	UpdateArpDisplay();

	// Cut note
	if (m_iLastMIDINote == MidiNote)
		HaltNote(Channel);

	if (InsertCut)
		InsertNote(HALT, 0, Channel, 0);

	// IT-mode, cut note on cuts
	if (theApp.GetSettings()->General.iEditStyle == EDIT_STYLE3)
		HaltNote(Channel);

	TRACE("%i: Cut note %i on channel %i\n", GetTickCount(), MidiNote, Channel);
}

// Release the currently playing note
void CFamiTrackerView::ReleaseMIDINote(unsigned int Channel, unsigned int MidiNote, bool InsertCut)
{
	// Release a MIDI note
	unsigned int Octave = GET_OCTAVE(MidiNote);
	unsigned int Note = GET_NOTE(MidiNote);

	CFamiTrackerDoc* pDoc = GetDocument();

	if (pDoc->GetChannel(Channel)->GetID() == CHANID_NOISE)
		FixNoise(MidiNote, Octave, Note);

	m_iActiveNotes[Channel] = 0;
	m_iAutoArpNotes[MidiNote] = 2;

	UpdateArpDisplay();

	// Cut note
	if (m_iLastMIDINote == MidiNote)
		ReleaseNote(Channel);

	if (InsertCut)
		InsertNote(RELEASE, 0, Channel, 0);

	// IT-mode, release note
	if (theApp.GetSettings()->General.iEditStyle == EDIT_STYLE3)
		ReleaseNote(Channel);

	TRACE("%i: Release note %i on channel %i\n", GetTickCount(), MidiNote, Channel);
}

void CFamiTrackerView::UpdateArpDisplay()
{
	if (theApp.GetSettings()->Midi.bMidiArpeggio == true) {
		int Base = -1;
		CString str = _T("Auto-arpeggio: ");

		for (int i = 0; i < 128; ++i) {
			if (m_iAutoArpNotes[i] == 1) {
				if (Base == -1)
					Base = i;
				str.AppendFormat(_T("%i "), i - Base);
			}
		}

		if (Base != -1)
			GetParentFrame()->SetMessageText(str);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Tracker input routines
//////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// API keyboard handling routines
//

bool CFamiTrackerView::IsShiftPressed() const
{
	return (::GetKeyState(VK_SHIFT) & 0x80) == 0x80;
}

bool CFamiTrackerView::IsControlPressed() const
{
	return (::GetKeyState(VK_CONTROL) & 0x80) == 0x80;
}

void CFamiTrackerView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{	
	// Called when a key is pressed
	if (GetFocus() != this)
		return;

	if (nChar >= VK_NUMPAD0 && nChar <= VK_NUMPAD9) {
		// Switch instrument
		if (m_pPatternEditor->GetColumn() == C_NOTE) {
			SetInstrument(nChar - VK_NUMPAD0);
			return;
		}
	}
	
	switch (nChar) {
		case VK_UP:
			OnKeyDirUp();
			break;
		case VK_DOWN:
			OnKeyDirDown();
			break;
		case VK_LEFT:
			OnKeyDirLeft();
			break;
		case VK_RIGHT:
			OnKeyDirRight();
			break;
		case VK_HOME:
			OnKeyHome();
			break;
		case VK_END:
			OnKeyEnd();
			break;
		case VK_PRIOR:
			OnKeyPageUp();
			break;
		case VK_NEXT:
			OnKeyPageDown();
			break;
		case VK_TAB:
			OnKeyTab();
			break;
		case VK_ADD:
			KeyIncreaseAction();
			break;
		case VK_SUBTRACT:
			KeyDecreaseAction();
			break;
		case VK_DELETE:
			OnKeyDelete();
			break;
		case VK_INSERT:
			OnKeyInsert();
			break;
		case VK_BACK:
			OnKeyBackspace();
			break;
		case VK_ESCAPE:
			OnKeyEscape();
			break;

		// Octaves, unless overridden
		case VK_F2: SetOctave(0); break;
		case VK_F3: SetOctave(1); break;
		case VK_F4: SetOctave(2); break;
		case VK_F5: SetOctave(3); break;
		case VK_F6: SetOctave(4); break;
		case VK_F7: SetOctave(5); break;
		case VK_F8: SetOctave(6); break;
		case VK_F9: SetOctave(7); break;

		default:
			HandleKeyboardInput(nChar);
	}

	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CFamiTrackerView::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// This is called when a key + ALT is pressed
	if (nChar >= VK_NUMPAD0 && nChar <= VK_NUMPAD9) {
		SetStepping(nChar - VK_NUMPAD0);
		return;
	}

	switch (nChar) {
		case VK_LEFT:
			m_pPatternEditor->MoveChannelLeft();
			InvalidateCursor();
			break;
		case VK_RIGHT:
			m_pPatternEditor->MoveChannelRight();
			InvalidateCursor();
			break;
	}

	CView::OnSysKeyDown(nChar, nRepCnt, nFlags);
}

void CFamiTrackerView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Called when a key is released
	HandleKeyboardNote(nChar, false);
	m_cKeyList[nChar] = 0;

	CView::OnKeyUp(nChar, nRepCnt, nFlags);
}

//
// Custom key handling routines
//

void CFamiTrackerView::OnKeyDirUp()
{
	// Move up
	m_pPatternEditor->MoveUp(m_iMoveKeyStepping);
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyDirDown()
{
	// Move down
	m_pPatternEditor->MoveDown(m_iMoveKeyStepping);
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyDirLeft()
{
	// Move left
	m_pPatternEditor->MoveLeft();
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyDirRight()
{
	// Move right
	m_pPatternEditor->MoveRight();
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyEscape()
{
	// ESC -> remove selection
	m_pPatternEditor->CancelSelection();
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyTab()
{
	// Move between channels
	bool bShiftPressed = IsShiftPressed();

	if (bShiftPressed)
		m_pPatternEditor->PreviousChannel();
	else
		m_pPatternEditor->NextChannel();

	InvalidateCursor();
}

void CFamiTrackerView::OnKeyPageUp()
{
	// Move page up
	int PageSize = theApp.GetSettings()->General.iPageStepSize;
	m_pPatternEditor->MoveUp(PageSize);
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyPageDown()
{
	// Move page down
	int PageSize = theApp.GetSettings()->General.iPageStepSize;
	m_pPatternEditor->MoveDown(PageSize);
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyHome()
{
	// Move home
	m_pPatternEditor->OnHomeKey();
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyEnd()
{
	// Move to end
	m_pPatternEditor->OnEndKey();
	InvalidateCursor();
}

void CFamiTrackerView::OnKeyInsert()
{
	// Insert row
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	if (PreventRepeat(VK_INSERT, true))
		return;

	if (m_pPatternEditor->IsSelecting()) {
		AddAction(new CPatternAction(CPatternAction::ACT_INSERT_SEL_ROWS));
	}
	else {
		AddAction(new CPatternAction(CPatternAction::ACT_INSERT_ROW));
	}
}

void CFamiTrackerView::OnKeyBackspace()
{
	// Pull up row
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	if (m_pPatternEditor->IsSelecting()) {
		AddAction(new CPatternAction(CPatternAction::ACT_EDIT_DELETE_ROWS));
	}
	else {
		if (PreventRepeat(VK_BACK, true))
			return;
		CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_DELETE_ROW);
		pAction->SetDelete(true, true);
		if (AddAction(pAction)) {
			m_pPatternEditor->MoveUp(1);
			InvalidateCursor();
		}
	}
}

void CFamiTrackerView::OnKeyDelete()
{
	// Delete row data
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	bool bShiftPressed = IsShiftPressed();

	if (PreventRepeat(VK_DELETE, true))
		return;

	if (m_pPatternEditor->IsSelecting()) {
		OnEditDelete();
	}
	else {
		CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_DELETE_ROW);
		bool bPullUp = theApp.GetSettings()->General.bPullUpDelete || bShiftPressed;
		pAction->SetDelete(bPullUp, false);
		AddAction(pAction);
		if (!bPullUp)
			StepDown();
	}
}

void CFamiTrackerView::KeyIncreaseAction()
{
	AddAction(new CPatternAction(CPatternAction::ACT_INCREASE));
}

void CFamiTrackerView::KeyDecreaseAction()
{
	AddAction(new CPatternAction(CPatternAction::ACT_DECREASE));
}

bool CFamiTrackerView::EditInstrumentColumn(stChanNote &Note, int Key, bool &StepDown, bool &MoveRight, bool &MoveLeft)
{
	int EditStyle = theApp.GetSettings()->General.iEditStyle;
	int Column = m_pPatternEditor->GetColumn();

	if (!m_bEditEnable)
		return false;

	if (CheckClearKey(Key)) {
		Note.Instrument = MAX_INSTRUMENTS;	// Indicate no instrument selected
		if (EditStyle != EDIT_STYLE2)
			StepDown = true;
		m_iLastInstrument = MAX_INSTRUMENTS;
		return true;
	}
	else if (CheckRepeatKey(Key)) {
		Note.Instrument = m_iLastInstrument;
		SetInstrument(m_iLastInstrument);
		if (EditStyle != EDIT_STYLE2)
			StepDown = true;
		return true;
	}

	int Value = ConvertKeyToHex(Key);
	unsigned char Mask, Shift;

	if (Value == -1)
		return false;

	if (Column == C_INSTRUMENT1) {
		Mask = 0x0F;
		Shift = 4;
	}
	else {
		Mask = 0xF0;
		Shift = 0;
	}

	if (Note.Instrument == MAX_INSTRUMENTS)
		Note.Instrument = 0;

	switch (EditStyle) {
		case EDIT_STYLE1: // FT2
			Note.Instrument = (Note.Instrument & Mask) | (Value << Shift);
			StepDown = true;
			break;
		case EDIT_STYLE2: // MPT
			if (Note.Instrument == (MAX_INSTRUMENTS - 1))
				Note.Instrument = 0;
			Note.Instrument = ((Note.Instrument & 0x0F) << 4) | Value & 0x0F;
			break;
		case EDIT_STYLE3: // IT
			Note.Instrument = (Note.Instrument & Mask) | (Value << Shift);
			if (Column == C_INSTRUMENT1)
				MoveRight = true;
			else if (Column == C_INSTRUMENT2) {
				MoveLeft = true;
				StepDown = true;
			}
			break;
	}

	if (Note.Instrument > (MAX_INSTRUMENTS - 1))
		Note.Instrument = (MAX_INSTRUMENTS - 1);

	if (Value == 0x80)
		Note.Instrument = MAX_INSTRUMENTS;

	SetInstrument(Note.Instrument);

	return true;
}

bool CFamiTrackerView::EditVolumeColumn(stChanNote &Note, int Key, bool &bStepDown)
{
	int EditStyle = theApp.GetSettings()->General.iEditStyle;

	if (!m_bEditEnable)
		return false;

	if (CheckClearKey(Key)) {
		Note.Vol = MAX_VOLUME;
		if (EditStyle != EDIT_STYLE2)
			bStepDown = true;
		m_iLastVolume = MAX_VOLUME;
		return true;
	}
	else if (CheckRepeatKey(Key)) {
		Note.Vol = m_iLastVolume;
		if (EditStyle != EDIT_STYLE2)
			bStepDown = true;
		return true;
	}

	int Value = ConvertKeyToHex(Key);

	if (Value == -1)
		return false;

	if (Value == 0x80)
		Note.Vol = MAX_VOLUME;
	else
		Note.Vol = Value;

	m_iLastVolume = Note.Vol;

	if (EditStyle != EDIT_STYLE2)
		bStepDown = true;

	return true;
}

bool CFamiTrackerView::EditEffNumberColumn(stChanNote &Note, unsigned char nChar, int EffectIndex, bool &bStepDown)
{
	int EditStyle = theApp.GetSettings()->General.iEditStyle;

	if (!m_bEditEnable)
		return false;

	if (CheckRepeatKey(nChar)) {
		Note.EffNumber[EffectIndex] = m_iLastEffect;
		Note.EffParam[EffectIndex] = m_iLastEffectParam;
		if (EditStyle != EDIT_STYLE2)		// // // 0CC-FT
			bStepDown = true;
		return true;
	}

	if (CheckClearKey(nChar)) {
		Note.EffNumber[EffectIndex] = EF_NONE;
		if (EditStyle != EDIT_STYLE2)
			bStepDown = true;
		return true;
	}

	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int Chip = pDoc->GetChannel(m_pPatternEditor->GetChannel())->GetChip();

	bool bValidEffect = false;
	int Effect;

	if (nChar >= VK_NUMPAD0 && nChar <= VK_NUMPAD9)
		nChar = '0' + nChar - VK_NUMPAD0;

	// // //

	// Common effects
	for (int i = 0; i < EF_COUNT && !bValidEffect; ++i) {
		if (nChar == EFF_CHAR[i]) {
			bValidEffect = true;
			Effect = i + 1;
		}
	}

	if (bValidEffect) {
		Note.EffNumber[EffectIndex] = Effect;
		switch (EditStyle) {
			case EDIT_STYLE2:	// Modplug
				if (Effect == m_iLastEffect)
					Note.EffParam[EffectIndex] = m_iLastEffectParam;
				break;
			default:
				bStepDown = true;
		}
		m_iLastEffect = Effect;
		return true;
	}

	return false;
}

bool CFamiTrackerView::EditEffParamColumn(stChanNote &Note, int Key, int EffectIndex, bool &bStepDown, bool &bMoveRight, bool &bMoveLeft)
{
	int EditStyle = theApp.GetSettings()->General.iEditStyle;
	int Column = m_pPatternEditor->GetColumn();
	int Value = ConvertKeyToHex(Key);

	if (!m_bEditEnable)
		return false;

	if (CheckRepeatKey(Key)) {
		Note.EffNumber[EffectIndex] = m_iLastEffect;
		Note.EffParam[EffectIndex] = m_iLastEffectParam;
		if (EditStyle != EDIT_STYLE2)		// // // 0CC-FT
			bStepDown = true;
		return true;
	}

	if (CheckClearKey(Key)) {
		Note.EffParam[EffectIndex] = 0;
		if (EditStyle != EDIT_STYLE2)
			bStepDown = true;
		return true;
	}

	if (Value == -1 || Value == 0x80)
		return false;

	unsigned char Mask, Shift;
	if (Column == C_EFF_PARAM1 || Column == C_EFF2_PARAM1 || Column == C_EFF3_PARAM1 || Column == C_EFF4_PARAM1) {
		Mask = 0x0F;
		Shift = 4;
	}
	else {
		Mask = 0xF0;
		Shift = 0;
	}
	
	switch (EditStyle) {
		case EDIT_STYLE1:	// FT2
			Note.EffParam[EffectIndex] = (Note.EffParam[EffectIndex] & Mask) | Value << Shift;
			bStepDown = true;
			break;
		case EDIT_STYLE2:	// Modplug
			Note.EffParam[EffectIndex] = ((Note.EffParam[EffectIndex] & 0x0F) << 4) | Value & 0x0F;
			break;
		case EDIT_STYLE3:	// IT
			Note.EffParam[EffectIndex] = (Note.EffParam[EffectIndex] & Mask) | Value << Shift;
			if (Mask == 0x0F)
				bMoveRight = true;
			else {
				bMoveLeft = true;
				bStepDown = true;
			}
			break;
	}

	m_iLastEffectParam = Note.EffParam[EffectIndex];

	return true;
}

void CFamiTrackerView::HandleKeyboardInput(char nChar)
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	stChanNote Note;

	int EditStyle = theApp.GetSettings()->General.iEditStyle;
	int Index = 0;

	int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();
	int Frame = m_pPatternEditor->GetFrame();
	int Row = m_pPatternEditor->GetRow();
	int Channel = m_pPatternEditor->GetChannel();
	int Column = m_pPatternEditor->GetColumn();

	bool bStepDown = false;
	bool bMoveRight = false;
	bool bMoveLeft = false;

	// Watch for repeating keys
	if (PreventRepeat(nChar, m_bEditEnable))
		return;

	// Get the note data
	pDoc->GetNoteData(Track, Frame, Channel, Row, &Note);

	// Make all effect columns look the same, save an index instead
	switch (Column) {
		case C_EFF_NUM:		Column = C_EFF_NUM;	Index = 0; break;
		case C_EFF2_NUM:	Column = C_EFF_NUM;	Index = 1; break;
		case C_EFF3_NUM:	Column = C_EFF_NUM;	Index = 2; break;
		case C_EFF4_NUM:	Column = C_EFF_NUM;	Index = 3; break;
		case C_EFF_PARAM1:	Column = C_EFF_PARAM1; Index = 0; break;
		case C_EFF2_PARAM1:	Column = C_EFF_PARAM1; Index = 1; break;
		case C_EFF3_PARAM1:	Column = C_EFF_PARAM1; Index = 2; break;
		case C_EFF4_PARAM1:	Column = C_EFF_PARAM1; Index = 3; break;
		case C_EFF_PARAM2:	Column = C_EFF_PARAM2; Index = 0; break;
		case C_EFF2_PARAM2:	Column = C_EFF_PARAM2; Index = 1; break;
		case C_EFF3_PARAM2:	Column = C_EFF_PARAM2; Index = 2; break;
		case C_EFF4_PARAM2:	Column = C_EFF_PARAM2; Index = 3; break;			
	}

	switch (Column) {
		// Note & octave column
		case C_NOTE:
			if (CheckRepeatKey(nChar)) {
				if (m_iLastNote == 0) {
					Note.Note = 0;
				}
				else if (m_iLastNote == NOTE_HALT) {
					Note.Note = HALT;
				}
				else if (m_iLastNote == NOTE_RELEASE) {
					Note.Note = RELEASE;
				}
				else {
					Note.Note = GET_NOTE(m_iLastNote);
					Note.Octave = GET_OCTAVE(m_iLastNote);
				}
				if (EditStyle != EDIT_STYLE2)		// // // 0CC-FT
					bStepDown = true;
			}
			else if (CheckClearKey(nChar)) {
				// Remove note
				Note.Note = 0;
				Note.Octave = 0;
				m_iLastNote = 0;
				if (EditStyle != EDIT_STYLE2)
					bStepDown = true;
			}
			else {
				// This is special
				HandleKeyboardNote(nChar, true);
				return;
			}
			break;
		// Instrument column
		case C_INSTRUMENT1:
		case C_INSTRUMENT2:
			if (!EditInstrumentColumn(Note, nChar, bStepDown, bMoveRight, bMoveLeft))
				return;
			break;
		// Volume column
		case C_VOLUME:
			if (!EditVolumeColumn(Note, nChar, bStepDown))
				return;
			break;
		// Effect number
		case C_EFF_NUM:
			if (!EditEffNumberColumn(Note, nChar, Index, bStepDown))
				return;
			break;
		// Effect parameter
		case C_EFF_PARAM1:
		case C_EFF_PARAM2:
			if (!EditEffParamColumn(Note, nChar, Index, bStepDown, bMoveRight, bMoveLeft))
				return;
			break;
	}

	// Something changed, store pattern data in document and update screen
	if (m_bEditEnable) {
		CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_EDIT_NOTE);
		pAction->SetNote(Note);
		if (AddAction(pAction)) {
			if (bMoveLeft)
				m_pPatternEditor->MoveLeft();
			if (bMoveRight)
				m_pPatternEditor->MoveRight();
			if (bStepDown)
				StepDown();
			InvalidateCursor();
		}
	}
}

bool CFamiTrackerView::DoRelease() const
{
	// Return true if there are a valid release sequence for selected instrument
	CInstrumentContainer<CInstrument> instContainer(GetDocument(), GetInstrument());
	CInstrument *pInstrument = instContainer();

	if (pInstrument == NULL)
		return false;
	
	return pInstrument->CanRelease();
}

void CFamiTrackerView::HandleKeyboardNote(char nChar, bool Pressed) 
{
	// Play a note from the keyboard
	int Note = TranslateKey(nChar);
	int Channel = m_pPatternEditor->GetChannel();
	
	if (Pressed) {	
		static int LastNote;

		if (CheckHaltKey(nChar))
			CutMIDINote(Channel, LastNote, true);
		else if (CheckReleaseKey(nChar))
			ReleaseMIDINote(Channel, LastNote, true);
		else {
			// Invalid key
			if (Note == -1)
				return;
			TriggerMIDINote(Channel, Note, 0x7F, m_bEditEnable);
			LastNote = Note;
		}
	}
	else {
		if (Note == -1)
			return;
		// IT doesn't cut the note when key is released
		if (theApp.GetSettings()->General.iEditStyle != EDIT_STYLE3) {
			// Find if note release should be used
			// TODO: make this an option instead?
			if (DoRelease())
				ReleaseMIDINote(Channel, Note, false);	
			else
				CutMIDINote(Channel, Note, false);
		}
		else {
			m_iActiveNotes[Channel] = 0;
			m_iAutoArpNotes[Note] = 2;
		}
	}
}

bool CFamiTrackerView::CheckClearKey(unsigned char Key) const
{
	return (Key == theApp.GetSettings()->Keys.iKeyClear);
}

bool CFamiTrackerView::CheckReleaseKey(unsigned char Key) const
{
	return (Key == theApp.GetSettings()->Keys.iKeyNoteRelease);
}

bool CFamiTrackerView::CheckHaltKey(unsigned char Key) const
{
	return (Key == theApp.GetSettings()->Keys.iKeyNoteCut);
}

bool CFamiTrackerView::CheckRepeatKey(unsigned char Key) const
{
	return (Key == theApp.GetSettings()->Keys.iKeyRepeat);
}

int CFamiTrackerView::TranslateKeyAzerty(unsigned char Key) const
{
	// Azerty conversion (TODO)
	int	KeyNote = 0, KeyOctave = 0;

	// Convert key to a note
	switch (Key) {
		case 50:	KeyNote = Cs;	KeyOctave = m_iOctave + 1;	break;	// 2
		case 51:	KeyNote = Ds;	KeyOctave = m_iOctave + 1;	break;	// 3
		case 53:	KeyNote = Fs;	KeyOctave = m_iOctave + 1;	break;	// 5
		case 54:	KeyNote = Gs;	KeyOctave = m_iOctave + 1;	break;	// 6
		case 55:	KeyNote = As;	KeyOctave = m_iOctave + 1;	break;	// 7
		case 57:	KeyNote = Cs;	KeyOctave = m_iOctave + 2;	break;	// 9
		case 48:	KeyNote = Ds;	KeyOctave = m_iOctave + 2;	break;	// 0
		case 81:	KeyNote = C;	KeyOctave = m_iOctave + 1;	break;	// Q
		case 87:	KeyNote = D;	KeyOctave = m_iOctave + 1;	break;	// W
		case 69:	KeyNote = E;	KeyOctave = m_iOctave + 1;	break;	// E
		case 82:	KeyNote = F;	KeyOctave = m_iOctave + 1;	break;	// R
		case 84:	KeyNote = G;	KeyOctave = m_iOctave + 1;	break;	// T
		case 89:	KeyNote = A;	KeyOctave = m_iOctave + 1;	break;	// Y
		case 85:	KeyNote = B;	KeyOctave = m_iOctave + 1;	break;	// U
		case 73:	KeyNote = C;	KeyOctave = m_iOctave + 2;	break;	// I
		case 79:	KeyNote = D;	KeyOctave = m_iOctave + 2;	break;	// O
		case 80:	KeyNote = E;	KeyOctave = m_iOctave + 2;	break;	// P
		case 221:	KeyNote = F;	KeyOctave = m_iOctave + 2;	break;	// �
		case 219:	KeyNote = Fs;	KeyOctave = m_iOctave + 2;	break;	// �
		//case 186:	KeyNote = G;	KeyOctave = m_iOctave + 2;	break;	// �
		case 83:	KeyNote = Cs;	KeyOctave = m_iOctave;		break;	// S
		case 68:	KeyNote = Ds;	KeyOctave = m_iOctave;		break;	// D
		case 71:	KeyNote = Fs;	KeyOctave = m_iOctave;		break;	// G
		case 72:	KeyNote = Gs;	KeyOctave = m_iOctave;		break;	// H
		case 74:	KeyNote = As;	KeyOctave = m_iOctave;		break;	// J
		case 76:	KeyNote = Cs;	KeyOctave = m_iOctave + 1;	break;	// L
		case 192:	KeyNote = Ds;	KeyOctave = m_iOctave + 1;	break;	// �
		case 90:	KeyNote = C;	KeyOctave = m_iOctave;		break;	// Z
		case 88:	KeyNote = D;	KeyOctave = m_iOctave;		break;	// X
		case 67:	KeyNote = E;	KeyOctave = m_iOctave;		break;	// C
		case 86:	KeyNote = F;	KeyOctave = m_iOctave;		break;	// V
		case 66:	KeyNote = G;	KeyOctave = m_iOctave;		break;	// B
		case 78:	KeyNote = A;	KeyOctave = m_iOctave;		break;	// N
		case 77:	KeyNote = B;	KeyOctave = m_iOctave;		break;	// M
		case 188:	KeyNote = C;	KeyOctave = m_iOctave + 1;	break;	// ,
		case 190:	KeyNote = D;	KeyOctave = m_iOctave + 1;	break;	// .
		case 189:	KeyNote = E;	KeyOctave = m_iOctave + 1;	break;	// -
	}

	// Invalid
	if (KeyNote == 0)
		return -1;

	// Return a MIDI note
	return MIDI_NOTE(KeyOctave, KeyNote);		
}

int CFamiTrackerView::TranslateKeyModplug(unsigned char Key) const
{
	// Modplug conversion
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int	KeyNote = 0, KeyOctave = 0;
	int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();

	stChanNote NoteData;
	pDoc->GetNoteData(Track, m_pPatternEditor->GetFrame(), m_pPatternEditor->GetChannel(), m_pPatternEditor->GetRow(), &NoteData);

	// Convert key to a note, Modplug style
	switch (Key) {
		case 49:	KeyNote = NoteData.Note;	KeyOctave = 0;	break;	// 1
		case 50:	KeyNote = NoteData.Note;	KeyOctave = 1;	break;	// 2
		case 51:	KeyNote = NoteData.Note;	KeyOctave = 2;	break;	// 3
		case 52:	KeyNote = NoteData.Note;	KeyOctave = 3;	break;	// 4
		case 53:	KeyNote = NoteData.Note;	KeyOctave = 4;	break;	// 5
		case 54:	KeyNote = NoteData.Note;	KeyOctave = 5;	break;	// 6
		case 55:	KeyNote = NoteData.Note;	KeyOctave = 6;	break;	// 7
		case 56:	KeyNote = NoteData.Note;	KeyOctave = 7;	break;	// 8
		case 57:	KeyNote = NoteData.Note;	KeyOctave = 7;	break;	// 9
		case 48:	KeyNote = NoteData.Note;	KeyOctave = 7;	break;	// 0

		case 81:	KeyNote = C;	KeyOctave = m_iOctave;	break;	// Q
		case 87:	KeyNote = Cs;	KeyOctave = m_iOctave;	break;	// W
		case 69:	KeyNote = D;	KeyOctave = m_iOctave;	break;	// E
		case 82:	KeyNote = Ds;	KeyOctave = m_iOctave;	break;	// R
		case 84:	KeyNote = E;	KeyOctave = m_iOctave;	break;	// T
		case 89:	KeyNote = F;	KeyOctave = m_iOctave;	break;	// Y
		case 85:	KeyNote = Fs;	KeyOctave = m_iOctave;	break;	// U
		case 73:	KeyNote = G;	KeyOctave = m_iOctave;	break;	// I
		case 79:	KeyNote = Gs;	KeyOctave = m_iOctave;	break;	// O
		case 80:	KeyNote = A;	KeyOctave = m_iOctave;	break;	// P
		case 221:	KeyNote = As;	KeyOctave = m_iOctave;	break;	// �
		case 219:	KeyNote = B;	KeyOctave = m_iOctave;	break;	// �

		case 65:	KeyNote = C;	KeyOctave = m_iOctave + 1;	break;	// A
		case 83:	KeyNote = Cs;	KeyOctave = m_iOctave + 1;	break;	// S
		case 68:	KeyNote = D;	KeyOctave = m_iOctave + 1;	break;	// D
		case 70:	KeyNote = Ds;	KeyOctave = m_iOctave + 1;	break;	// F
		case 71:	KeyNote = E;	KeyOctave = m_iOctave + 1;	break;	// G
		case 72:	KeyNote = F;	KeyOctave = m_iOctave + 1;	break;	// H
		case 74:	KeyNote = Fs;	KeyOctave = m_iOctave + 1;	break;	// J
		case 75:	KeyNote = G;	KeyOctave = m_iOctave + 1;	break;	// K
		case 76:	KeyNote = Gs;	KeyOctave = m_iOctave + 1;	break;	// L
		case 192:	KeyNote = A;	KeyOctave = m_iOctave + 1;	break;	// �
		case 222:	KeyNote = As;	KeyOctave = m_iOctave + 1;	break;	// �
		case 191:	KeyNote = B;	KeyOctave = m_iOctave + 1;	break;	// '

		case 90:	KeyNote = C;	KeyOctave = m_iOctave + 2;	break;	// Z
		case 88:	KeyNote = Cs;	KeyOctave = m_iOctave + 2;	break;	// X
		case 67:	KeyNote = D;	KeyOctave = m_iOctave + 2;	break;	// C
		case 86:	KeyNote = Ds;	KeyOctave = m_iOctave + 2;	break;	// V
		case 66:	KeyNote = E;	KeyOctave = m_iOctave + 2;	break;	// B
		case 78:	KeyNote = F;	KeyOctave = m_iOctave + 2;	break;	// N
		case 77:	KeyNote = Fs;	KeyOctave = m_iOctave + 2;	break;	// M
		case 188:	KeyNote = G;	KeyOctave = m_iOctave + 2;	break;	// ,
		case 190:	KeyNote = Gs;	KeyOctave = m_iOctave + 2;	break;	// .
		case 189:	KeyNote = A;	KeyOctave = m_iOctave + 2;	break;	// -
	}

	// Invalid
	if (KeyNote == 0)
		return -1;

	// Return a MIDI note
	return MIDI_NOTE(KeyOctave, KeyNote);
}

int CFamiTrackerView::TranslateKeyDefault(unsigned char Key) const
{
	// Default conversion
	int	KeyNote = 0, KeyOctave = 0;

	// Convert key to a note
	switch (Key) {
		case 50:	KeyNote = Cs;	KeyOctave = m_iOctave + 1;	break;	// 2
		case 51:	KeyNote = Ds;	KeyOctave = m_iOctave + 1;	break;	// 3
		case 53:	KeyNote = Fs;	KeyOctave = m_iOctave + 1;	break;	// 5
		case 54:	KeyNote = Gs;	KeyOctave = m_iOctave + 1;	break;	// 6
		case 55:	KeyNote = As;	KeyOctave = m_iOctave + 1;	break;	// 7
		case 57:	KeyNote = Cs;	KeyOctave = m_iOctave + 2;	break;	// 9
		case 48:	KeyNote = Ds;	KeyOctave = m_iOctave + 2;	break;	// 0
		case 81:	KeyNote = C;	KeyOctave = m_iOctave + 1;	break;	// Q
		case 87:	KeyNote = D;	KeyOctave = m_iOctave + 1;	break;	// W
		case 69:	KeyNote = E;	KeyOctave = m_iOctave + 1;	break;	// E
		case 82:	KeyNote = F;	KeyOctave = m_iOctave + 1;	break;	// R
		case 84:	KeyNote = G;	KeyOctave = m_iOctave + 1;	break;	// T
		case 89:	KeyNote = A;	KeyOctave = m_iOctave + 1;	break;	// Y
		case 85:	KeyNote = B;	KeyOctave = m_iOctave + 1;	break;	// U
		case 73:	KeyNote = C;	KeyOctave = m_iOctave + 2;	break;	// I
		case 79:	KeyNote = D;	KeyOctave = m_iOctave + 2;	break;	// O
		case 80:	KeyNote = E;	KeyOctave = m_iOctave + 2;	break;	// P
		case 221:	KeyNote = F;	KeyOctave = m_iOctave + 2;	break;	// �
		case 219:	KeyNote = Fs;	KeyOctave = m_iOctave + 2;	break;	// �
		//case 186:	KeyNote = G;	KeyOctave = m_iOctave + 2;	break;	// �
		case 83:	KeyNote = Cs;	KeyOctave = m_iOctave;		break;	// S
		case 68:	KeyNote = Ds;	KeyOctave = m_iOctave;		break;	// D
		case 71:	KeyNote = Fs;	KeyOctave = m_iOctave;		break;	// G
		case 72:	KeyNote = Gs;	KeyOctave = m_iOctave;		break;	// H
		case 74:	KeyNote = As;	KeyOctave = m_iOctave;		break;	// J
		case 76:	KeyNote = Cs;	KeyOctave = m_iOctave + 1;	break;	// L
		case 192:	KeyNote = Ds;	KeyOctave = m_iOctave + 1;	break;	// �
		case 90:	KeyNote = C;	KeyOctave = m_iOctave;		break;	// Z
		case 88:	KeyNote = D;	KeyOctave = m_iOctave;		break;	// X
		case 67:	KeyNote = E;	KeyOctave = m_iOctave;		break;	// C
		case 86:	KeyNote = F;	KeyOctave = m_iOctave;		break;	// V
		case 66:	KeyNote = G;	KeyOctave = m_iOctave;		break;	// B
		case 78:	KeyNote = A;	KeyOctave = m_iOctave;		break;	// N
		case 77:	KeyNote = B;	KeyOctave = m_iOctave;		break;	// M
		case 188:	KeyNote = C;	KeyOctave = m_iOctave + 1;	break;	// ,
		case 190:	KeyNote = D;	KeyOctave = m_iOctave + 1;	break;	// .
		case 189:	KeyNote = E;	KeyOctave = m_iOctave + 1;	break;	// -
	}

	// Invalid
	if (KeyNote == 0)
		return -1;

	// Return a MIDI note
	return MIDI_NOTE(KeyOctave, KeyNote);
}

int CFamiTrackerView::TranslateKey(unsigned char Key) const
{
	// Translates a keyboard character into a MIDI note

	// For modplug users
	if (theApp.GetSettings()->General.iEditStyle == EDIT_STYLE2)
		return TranslateKeyModplug(Key);

	// Default
	return TranslateKeyDefault(Key);
}

bool CFamiTrackerView::PreventRepeat(unsigned char Key, bool Insert)
{
	if (m_cKeyList[Key] == 0)
		m_cKeyList[Key] = 1;
	else {
		if ((!theApp.GetSettings()->General.bKeyRepeat || !Insert))
			return true;
	}

	return false;
}

void CFamiTrackerView::RepeatRelease(unsigned char Key)
{
	memset(m_cKeyList, 0, 256);
}

//
// Note preview
//

bool CFamiTrackerView::PreviewNote(unsigned char Key)
{
	if (PreventRepeat(Key, false))
		return false;

	int Note = TranslateKey(Key);

	TRACE0("View: Note preview\n");

	if (Note > 0) {
		TriggerMIDINote(m_pPatternEditor->GetChannel(), Note, 0x7F, false); 
		return true;
	}

	return false;
}

void CFamiTrackerView::PreviewRelease(unsigned char Key)
{
	memset(m_cKeyList, 0, 256);

	int Note = TranslateKey(Key);

	if (Note > 0) {
		if (DoRelease())
			ReleaseMIDINote(m_pPatternEditor->GetChannel(), Note, false);
		else 
			CutMIDINote(m_pPatternEditor->GetChannel(), Note, false);
	}
}

//
// MIDI in routines
//

void CFamiTrackerView::TranslateMidiMessage()
{
	// Check and handle MIDI messages

	CMIDI *pMIDI = theApp.GetMIDI();
	CFamiTrackerDoc* pDoc = GetDocument();
	CString Status;

	if (!pMIDI || !pDoc)
		return;

	unsigned char Message, Channel, Data1, Data2;
	while (pMIDI->ReadMessage(Message, Channel, Data1, Data2)) {

		if (Message != 0x0F) {
			if (!theApp.GetSettings()->Midi.bMidiChannelMap)
				Channel = m_pPatternEditor->GetChannel();
			if (Channel > pDoc->GetAvailableChannels() - 1)
				Channel = pDoc->GetAvailableChannels() - 1;
		}

		// Translate key releases to note off messages
		if (Message == MIDI_MSG_NOTE_ON && Data2 == 0) {
			Message = MIDI_MSG_NOTE_OFF;
		}

		if (Message == MIDI_MSG_NOTE_ON || Message == MIDI_MSG_NOTE_OFF) {
			// Remove two octaves from MIDI notes
			Data1 -= 24;
			if (Data1 > 127)
				return;
		}

		switch (Message) {
			case MIDI_MSG_NOTE_ON:
				TriggerMIDINote(Channel, Data1, Data2, true);
				AfxFormatString3(Status, IDS_MIDI_MESSAGE_ON_FORMAT, 
					MakeIntString(Data1 % 12), 
					MakeIntString(Data1 / 12),
					MakeIntString(Data2));
				break;

			case MIDI_MSG_NOTE_OFF:
				// MIDI key is released, don't input note break into pattern
				if (DoRelease())
					ReleaseMIDINote(Channel, Data1, false);
				else
					CutMIDINote(Channel, Data1, false);
				Status.Format(IDS_MIDI_MESSAGE_OFF);
				break;
			
			case MIDI_MSG_PITCH_WHEEL: 
				{
					CTrackerChannel *pChannel = pDoc->GetChannel(Channel);
					int PitchValue = 0x2000 - ((Data1 & 0x7F) | ((Data2 & 0x7F) << 7));
					pChannel->SetPitch(-PitchValue / 0x10);
				}
				break;

			case 0x0F:
				if (Channel == 0x08) {
					m_pPatternEditor->MoveDown(m_iInsertKeyStepping);
					InvalidateCursor();
				}
				break;
		}
	}

	if (Status.GetLength() > 0)
		GetParentFrame()->SetMessageText(Status);
}

//
// Effects menu
//

void CFamiTrackerView::OnTrackerToggleChannel()
{
	if (m_iMenuChannel == -1)
		m_iMenuChannel = m_pPatternEditor->GetChannel();

	ToggleChannel(m_iMenuChannel);

	m_iMenuChannel = -1;
}

void CFamiTrackerView::OnTrackerSoloChannel()
{
	if (m_iMenuChannel == -1)
		m_iMenuChannel = m_pPatternEditor->GetChannel();

	SoloChannel(m_iMenuChannel);

	m_iMenuChannel = -1;
}

void CFamiTrackerView::OnTrackerUnmuteAllChannels()
{
	UnmuteAllChannels();
}

void CFamiTrackerView::OnNextOctave()
{
	if (m_iOctave < 7)
		SetOctave(m_iOctave + 1);
}

void CFamiTrackerView::OnPreviousOctave()
{
	if (m_iOctave > 0)
		SetOctave(m_iOctave - 1);
}

void CFamiTrackerView::SetOctave(unsigned int iOctave)
{
	m_iOctave = iOctave;
	static_cast<CMainFrame*>(GetParentFrame())->DisplayOctave();
}

int CFamiTrackerView::GetSelectedChipType() const
{
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	return pDoc->GetChipType(m_pPatternEditor->GetChannel());
}

void CFamiTrackerView::OnIncreaseStepSize()
{
	if (m_iInsertKeyStepping < 256)
		SetStepping(m_iInsertKeyStepping + 1);
}

void CFamiTrackerView::OnDecreaseStepSize()
{
	if (m_iInsertKeyStepping > 0)
		SetStepping(m_iInsertKeyStepping - 1);
}

void CFamiTrackerView::SetStepping(int Step) 
{ 
	m_iInsertKeyStepping = Step;

	if (Step > 0 && !theApp.GetSettings()->General.bNoStepMove)
		m_iMoveKeyStepping = Step;
	else 
		m_iMoveKeyStepping = 1;

	static_cast<CMainFrame*>(GetParentFrame())->UpdateControls();
}

void CFamiTrackerView::OnEditInterpolate()
{
	AddAction(new CPatternAction(CPatternAction::ACT_INTERPOLATE));
}

void CFamiTrackerView::OnEditReverse()
{
	AddAction(new CPatternAction(CPatternAction::ACT_REVERSE));
}

void CFamiTrackerView::OnEditReplaceInstrument()
{
	CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_REPLACE_INSTRUMENT);
	pAction->SetInstrument(GetInstrument());
	AddAction(pAction);	
}

void CFamiTrackerView::OnNcMouseMove(UINT nHitTest, CPoint point)
{
	if (m_pPatternEditor->OnMouseNcMove())
		InvalidateHeader();

	CView::OnNcMouseMove(nHitTest, point);
}

void CFamiTrackerView::OnOneStepUp()
{
	m_pPatternEditor->MoveUp(SINGLE_STEP);
	InvalidateCursor();
}

void CFamiTrackerView::OnOneStepDown()
{
	m_pPatternEditor->MoveDown(SINGLE_STEP);
	InvalidateCursor();
}

void CFamiTrackerView::MakeSilent()
{
	m_iAutoArpPtr		= 0; 
	m_iLastAutoArpPtr	= 0;
	m_iAutoArpKeyCount	= 0;

	memset(m_iActiveNotes, 0, sizeof(int) * MAX_CHANNELS);
	memset(m_cKeyList, 0, sizeof(char) * 256);
	memset(m_iArpeggiate, 0, sizeof(int) * MAX_CHANNELS);
	memset(m_iAutoArpNotes, 0, sizeof(char) * 128);
}

bool CFamiTrackerView::IsSelecting() const
{
	return m_pPatternEditor->IsSelecting();
}

bool CFamiTrackerView::IsClipboardAvailable() const
{
	return ::IsClipboardFormatAvailable(m_iClipboard) == TRUE;
}

void CFamiTrackerView::OnBlockStart()
{
	m_pPatternEditor->SetBlockStart();
	InvalidateCursor();
}

void CFamiTrackerView::OnBlockEnd()
{
	m_pPatternEditor->SetBlockEnd();
	InvalidateCursor();
}

void CFamiTrackerView::OnPickupRow()
{
	// Get row info
	CFamiTrackerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	int Track = static_cast<CMainFrame*>(GetParentFrame())->GetSelectedTrack();
	int Frame = m_pPatternEditor->GetFrame();
	int Row = m_pPatternEditor->GetRow();
	int Channel = m_pPatternEditor->GetChannel();
	
	stChanNote Note;

	pDoc->GetNoteData(Track, Frame, Channel, Row, &Note);

	m_iLastVolume = Note.Vol;

	SetInstrument(Note.Instrument);
}

bool CFamiTrackerView::AddAction(CAction *pAction) const
{
	// Performs an action and adds it to the undo queue
	CMainFrame *pMainFrm = static_cast<CMainFrame*>(GetParentFrame());
	return (pMainFrm != NULL) ? pMainFrm->AddAction(pAction) : false;
}

// OLE support

DROPEFFECT CFamiTrackerView::OnDragEnter(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	TRACE0("OLE: OnDragEnter\n");

	if (pDataObject->IsDataAvailable(m_iClipboard)) {
		if (dwKeyState & (MK_CONTROL | MK_SHIFT)) {
			m_nDropEffect = DROPEFFECT_COPY;
			if (dwKeyState & MK_SHIFT)
				m_bDropMix = true;
			else
				m_bDropMix = false;
		}
		else {
			m_nDropEffect = DROPEFFECT_MOVE;
		}

		// Get drag rectangle
		std::unique_ptr<CPatternClipData> pDragData(new CPatternClipData());		// // //

		HGLOBAL hMem = pDataObject->GetGlobalData(m_iClipboard);
		pDragData->FromMem(hMem);

		// Begin drag operation
		m_pPatternEditor->BeginDrag(pDragData.get());
		m_pPatternEditor->UpdateDrag(point);

		InvalidateCursor();
	}

	return m_nDropEffect;
}

void CFamiTrackerView::OnDragLeave()
{
	TRACE0("OLE: OnDragLeave\n");

	if (m_nDropEffect != DROPEFFECT_NONE) {
		m_pPatternEditor->EndDrag();
		InvalidateCursor();
	}
	
	m_nDropEffect = DROPEFFECT_NONE;

	CView::OnDragLeave();
}

DROPEFFECT CFamiTrackerView::OnDragOver(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	TRACE0("OLE: OnDragOver\n");

	// Update drag'n'drop cursor
	if (m_nDropEffect != DROPEFFECT_NONE) {
		m_pPatternEditor->UpdateDrag(point);
		InvalidateCursor();
	}

	return m_nDropEffect;
}

BOOL CFamiTrackerView::OnDrop(COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	TRACE0("OLE: OnDrop\n");

	BOOL Result = FALSE;

	// Perform drop
	if (m_nDropEffect != DROPEFFECT_NONE) {

		bool bCopy = (dropEffect == DROPEFFECT_COPY) || (m_bDragSource == false);

		m_pPatternEditor->UpdateDrag(point);

		// Get clipboard data
		CPatternClipData *pClipData = new CPatternClipData();
		HGLOBAL hMem = pDataObject->GetGlobalData(m_iClipboard);
		pClipData->FromMem(hMem);
				
		// Paste into pattern
		if (!m_pPatternEditor->PerformDrop(pClipData, bCopy, m_bDropMix)) {
			SAFE_RELEASE(pClipData);
		}

		InvalidateCursor();
		m_bDropped = true;

		Result = TRUE;
	}

	m_nDropEffect = DROPEFFECT_NONE;
	
	return Result;
}

void CFamiTrackerView::BeginDragData(int ChanOffset, int RowOffset)
{
	TRACE0("OLE: BeginDragData\n");

	std::unique_ptr<COleDataSource> pSrc(new COleDataSource());		// // //
	std::unique_ptr<CPatternClipData> pClipData(m_pPatternEditor->Copy());
	SIZE_T Size = pClipData->GetAllocSize();

	pClipData->ClipInfo.OleInfo.ChanOffset = ChanOffset;
	pClipData->ClipInfo.OleInfo.RowOffset = RowOffset;

	HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, Size);

	if (hMem != NULL) {
		// Copy data
		pClipData->ToMem(hMem);

		m_bDragSource = true;
		m_bDropped = false;

		pSrc->CacheGlobalData(m_iClipboard, hMem);
		DROPEFFECT res = pSrc->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_MOVE);

		if (m_bDropped == false) {
			// Target was another window
			switch (res) {
				case DROPEFFECT_MOVE:
					// Delete data
					CPatternAction *pAction = new CPatternAction(CPatternAction::ACT_EDIT_DELETE);
					AddAction(pAction);
					break;
			}

			m_pPatternEditor->CancelSelection();
		}
	}

	m_bDragSource = false;

	::GlobalFree(hMem);
}

bool CFamiTrackerView::IsDragging() const 
{
	return m_bDragSource;
}
