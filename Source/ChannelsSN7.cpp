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

// This file handles playing of 2A03 channels

#include <cmath>
#include "stdafx.h"
#include "FamiTracker.h"
#include "FamiTrackerDoc.h"
#include "ChannelHandler.h"
#include "ChannelsSN7.h"		// // //
#include "Settings.h"
#include "SoundGen.h"

int CChannelHandlerSN7::m_iRegisterPos[] = {		// // //
	CHANID_SQUARE1, CHANID_SQUARE2, CHANID_SQUARE3
};
uint8 CChannelHandlerSN7::m_cStereoFlag = 0xFFu;		// // //
uint8 CChannelHandlerSN7::m_cStereoFlagLast = 0xFFu;		// // //

CChannelHandlerSN7::CChannelHandlerSN7() : 
	CChannelHandler(0x3FF, 0x0F),		// // //
	m_bManualVolume(0),
	m_iInitVolume(0),
	// // //
	m_iPostEffect(0),
	m_iPostEffectParam(0)
{
}

void CChannelHandlerSN7::HandleNoteData(stChanNote *pNoteData, int EffColumns)
{
	m_iPostEffect = 0;
	m_iPostEffectParam = 0;
	// // //
	m_iInitVolume = 0x0F;
	m_bManualVolume = false;

	CChannelHandler::HandleNoteData(pNoteData, EffColumns);

	if (pNoteData->Note != NONE && pNoteData->Note != HALT && pNoteData->Note != RELEASE) {
		if (m_iPostEffect && (m_iEffect == EF_SLIDE_UP || m_iEffect == EF_SLIDE_DOWN))
			SetupSlide(m_iPostEffect, m_iPostEffectParam);
		else if (m_iEffect == EF_SLIDE_DOWN || m_iEffect == EF_SLIDE_UP)
			m_iEffect = EF_NONE;
	}
}

void CChannelHandlerSN7::HandleCustomEffects(int EffNum, int EffParam)
{
	#define GET_SLIDE_SPEED(x) (((x & 0xF0) >> 3) + 1)

	if (!CheckCommonEffects(EffNum, EffParam)) {
		// Custom effects
		switch (EffNum) {
		case EF_SN_CONTROL:		// // //
			//if (EffParam >= 0x00 && EffParam <= 0x1F) {
			//	SetStereo(EffParam && EffParam <= 0x10, EffParam >= 0x10);
			if (EffParam == 0x00 || EffParam == 2) SetStereo(true, true);	//sh8bit
			if (EffParam == 0x01) SetStereo(true, false);
			if (EffParam == 0x03) SetStereo(false,true);
			//	break;
			//}
			break;
		case EF_DUTY_CYCLE:
			m_iDefaultDuty = m_iDutyPeriod = EffParam;
			break;
		case EF_SLIDE_UP:
		case EF_SLIDE_DOWN:
			m_iPostEffect = EffNum;
			m_iPostEffectParam = EffParam;
			SetupSlide(EffNum, EffParam);
			break;
		}
	}
}

bool CChannelHandlerSN7::HandleInstrument(int Instrument, bool Trigger, bool NewInstrument)
{
	CFamiTrackerDoc *pDocument = m_pSoundGen->GetDocument();
	CInstrumentContainer<CInstrument2A03> instContainer(pDocument, Instrument);
	CInstrument2A03 *pInstrument = instContainer();

	if (pInstrument == NULL)
		return false;

	for (int i = 0; i < CInstrument2A03::SEQUENCE_COUNT; ++i) {
		const CSequence *pSequence = pDocument->GetSequence(SNDCHIP_NONE, pInstrument->GetSeqIndex(i), i);
		if (Trigger || !IsSequenceEqual(i, pSequence) || pInstrument->GetSeqEnable(i) > GetSequenceState(i)) {
			if (pInstrument->GetSeqEnable(i) == 1)
				SetupSequence(i, pSequence);
			else
				ClearSequence(i);
		}
	}

	return true;
}

void CChannelHandlerSN7::HandleEmptyNote()
{
	if (m_bManualVolume)
		m_iSeqVolume = m_iInitVolume;
	// // //
}

void CChannelHandlerSN7::HandleCut()
{
	CutNote();
}

void CChannelHandlerSN7::HandleRelease()
{
	if (!m_bRelease) {
		ReleaseNote();
		ReleaseSequences();
	}
	// // //
}

void CChannelHandlerSN7::HandleNote(int Note, int Octave)
{
	m_iNote			= RunNote(Octave, Note);
	m_iDutyPeriod	= m_iDefaultDuty;
	m_iSeqVolume	= m_iInitVolume;

	m_iArpState = 0;

	// // //
}

int CChannelHandlerSN7::CalculateVolume() const		// // //
{
	// Volume calculation
	int Volume = (m_iVolume >> VOL_COLUMN_SHIFT) + m_iSeqVolume - 15 - GetTremolo();

	if (m_iSeqVolume > 0 && m_iVolume > 0 && Volume <= 0)
		Volume = 1;

	if (!m_bGate || Volume < 0)
		Volume = 0;

	return Volume;
}

void CChannelHandlerSN7::SetStereo(bool Left, bool Right) const
{
	m_cStereoFlag &= ~(0x11 << m_iChannelID);
	if (Left)
		m_cStereoFlag |= ((uint8_t)0x10) << m_iChannelID;
	if (Right)
		m_cStereoFlag |= ((uint8_t)0x01) << m_iChannelID;
}

void CChannelHandlerSN7::ProcessChannel()
{
	// Default effects
	CChannelHandler::ProcessChannel();
	
	// // //

	// Sequences
	for (int i = 0; i < CInstrument2A03::SEQUENCE_COUNT; ++i)
		RunSequence(i);
}

void CChannelHandlerSN7::ResetChannel()
{
	CChannelHandler::ResetChannel();
}

void CChannelHandlerSN7::SwapChannels(int ID)		// // //
{
	m_iRegisterPos[CHANID_SQUARE1] = CHANID_SQUARE1;
	m_iRegisterPos[CHANID_SQUARE2] = CHANID_SQUARE2;
	m_iRegisterPos[CHANID_SQUARE3] = ID;
	m_iRegisterPos[ID] = CHANID_SQUARE3;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Square 
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void CSquareChan::RefreshChannel()
{
	// TODO: move this into an appropriate chip handler class
	// once this is done, do the same refactoring in 0CC-FT
	if (m_iChannelID == CHANID_SQUARE1 && m_cStereoFlagLast != m_cStereoFlag)
		WriteRegister(/* CSN76489::STEREO_PORT */ 0x4F, m_cStereoFlagLast = m_cStereoFlag);

	int Period = CalculatePeriod();
	int Volume = CalculateVolume();
	// // //
	unsigned char HiFreq = (Period >> 4) & 0x3F;
	unsigned char LoFreq = (Period & 0xF);

	const uint16 Base = m_iRegisterPos[m_iChannelID] * 2;		// // //
	WriteRegister(Base, LoFreq);
	WriteRegister(  -1, HiFreq); // double-byte
	WriteRegister(Base + 1, 0xF ^ Volume);
}

void CSquareChan::ClearRegisters()
{
	const uint16 Base = m_iRegisterPos[m_iChannelID] * 2;		// // //
	WriteRegister(Base, 0x00);
	WriteRegister(  -1, 0x00); // double-byte
	WriteRegister(Base + 1, 0xF);
	m_iRegisterPos[m_iChannelID] = m_iChannelID;
	SetStereo(true, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Noise
///////////////////////////////////////////////////////////////////////////////////////////////////////////

CNoiseChan::CNoiseChan() : CChannelHandlerSN7() 
{ 
	m_iDefaultDuty = 0;
}

void CNoiseChan::HandleNote(int Note, int Octave)
{
	int NewNote = MIDI_NOTE(Octave, Note);
	int NesFreq = TriggerNote(NewNote);

//	NewNote &= 0x0F;

	if (m_iPortaSpeed > 0 && m_iEffect == EF_PORTAMENTO) {
		if (m_iPeriod == 0)
			m_iPeriod = NesFreq;
		m_iPortaTo = NesFreq;
	}
	else
		m_iPeriod = NesFreq;

	m_bGate = true;
	m_bTrigger = true;		// // //

	m_iNote			= NewNote;
	m_iDutyPeriod	= m_iDefaultDuty;
	m_iSeqVolume	= m_iInitVolume;
}

void CNoiseChan::SetupSlide(int Type, int EffParam)
{
	CChannelHandler::SetupSlide(Type, EffParam);

	// Work-around for noise
	if (m_iEffect == EF_SLIDE_DOWN)
		m_iEffect = EF_SLIDE_UP;
	else
		m_iEffect = EF_SLIDE_DOWN;
}

/*
int CNoiseChan::CalculatePeriod() const
{
	return LimitPeriod(m_iPeriod - GetVibrato() + GetFinePitch() + GetPitch());
}
*/

void CNoiseChan::RefreshChannel()
{
	int Period = CalculatePeriod();
	int Volume = CalculateVolume();
	char NoiseMode = !(m_iDutyPeriod & 0x01);		// // //

	int newCtrl = (NoiseMode << 2) | Period;		// // //
	if ((m_bTrigger && m_bNoiseReset == 1) || (m_bNoiseReset == 2) || newCtrl != m_iLastCtrl) { //sh8bit
		WriteRegister(0x06, newCtrl);
		m_iLastCtrl = newCtrl;
	}
	WriteRegister(0x07, 0xF ^ Volume);

	m_bTrigger = false;
}

void CNoiseChan::HandleCustomEffects(int EffNum, int EffParam)
{
	switch (EffNum) {
	case EF_SN_CONTROL:
		switch (EffParam) {
		case 0xE0: m_bNoiseReset = 0; return;
		case 0xE1: m_bNoiseReset = 1;  m_bTrigger = true; return;  //sh8bit
		case 0xE2: m_bNoiseReset = 2; return;  //sh8bit
		}
	}

	return CChannelHandlerSN7::HandleCustomEffects(EffNum, EffParam);
}

void CNoiseChan::ClearRegisters()
{
	m_iLastCtrl = -1;		// // //
	WriteRegister(0x06, 0);
	WriteRegister(0x07, 0xF);
	SetStereo(true, true);
	m_bNoiseReset = false;
}

int CNoiseChan::TriggerNote(int Note)
{
	// Clip range to 0-15
	/*
	if (Note > 0x0F)
		Note = 0x0F;
	if (Note < 0)
		Note = 0;
		*/

	RegisterKeyState(Note);

//	Note &= 0x0F;

	return (2 - Note) & 0x03;		// // //
}

// // //
