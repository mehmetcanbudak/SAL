#include "MidiHandler.hpp"
#include "IKeyEventListener.hpp"
#include "IPitchEventListener.hpp"

#include <cmath>

MidiHandler::MidiHandler() :
	m_WorkingStatusByte( 0 ),
	m_WorkingMessageLength( 0 ),
	m_WorkingDataByteIndex( 0 ),
	m_CurrentWriteIndex( 0 ),
	m_CurrentReadIndex( 0 ),
	m_SemitonesToPitchBend( 1 )
{
}

MidiHandler::~MidiHandler()
{
}

void MidiHandler::processByte (uint8_t byte)
{
	if ( (byte & MIDI_STATUS_BYTE) ) // if status byte
	{
		m_WorkingStatusByte = byte;
		uint8_t statusByteNybble = (m_WorkingStatusByte >> 4);

		uint8_t* midiMessageBytes = m_MsgBuffer[m_CurrentWriteIndex].getRawData();
		midiMessageBytes[0] = byte;
		m_WorkingMessageLength = 0;

		if ( statusByteNybble != MIDI_SYSTEM_COMMON ) // if MIDI channel message
		{
			switch ( statusByteNybble )
			{
				case MIDI_NOTE_OFF:
					m_WorkingMessageLength = MIDI_NOTE_OFF_NUM_DATA;
					break;
				case MIDI_NOTE_ON:
					m_WorkingMessageLength = MIDI_NOTE_OFF_NUM_DATA;
					break;
				case MIDI_AFTERTOUCH:
					m_WorkingMessageLength = MIDI_AFTERTOUCH_NUM_DATA;
					break;
				case MIDI_CONTROL_CHANGE:
					m_WorkingMessageLength = MIDI_CONTROL_CHANGE_NUM_DATA;
					break;
				case MIDI_PROGRAM_CHANGE:
					m_WorkingMessageLength = MIDI_PROGRAM_CHANGE_NUM_DATA;
					break;
				case MIDI_AFTERTOUCH_MONO:
					m_WorkingMessageLength = MIDI_AFTERTOUCH_MONO_NUM_DATA;
					break;
				case MIDI_PITCH_BEND:
					m_WorkingMessageLength = MIDI_PITCH_BEND_NUM_DATA;
					break;
				default:
					break;
			}
		}
		else // if MIDI System Common or Realtime Message
		{
			switch (m_WorkingStatusByte)
			{
				case MIDI_TIME_CODE:
					m_WorkingMessageLength = MIDI_TIME_CODE_NUM_DATA;
					break;
				case MIDI_SONG_POSITION:
					m_WorkingMessageLength = MIDI_SONG_POSITION_NUM_DATA;
					break;
				case MIDI_SONG_SELECT:
					m_WorkingMessageLength = MIDI_SONG_SELECT_NUM_DATA;
					break;
				case MIDI_TUNE_REQUEST:
					m_WorkingMessageLength = MIDI_TUNE_REQUEST_NUM_DATA;
					break;
				case MIDI_END_OF_EXCLUSIVE:
					m_WorkingMessageLength = MIDI_END_OF_EXCLUSIVE_NUM_DATA;
					break;
				case MIDI_TIMING_CLOCK:
					m_WorkingMessageLength = MIDI_TIMING_CLOCK_NUM_DATA;
					break;
				case MIDI_START:
					m_WorkingMessageLength = MIDI_START_NUM_DATA;
					break;
				case MIDI_CONTINUE:
					m_WorkingMessageLength = MIDI_CONTINUE_NUM_DATA;
					break;
				case MIDI_STOP:
					m_WorkingMessageLength = MIDI_STOP_NUM_DATA;
					break;
				case MIDI_ACTIVE_SENSING:
					m_WorkingMessageLength = MIDI_ACTIVE_SENSING_NUM_DATA;
					break;
				case MIDI_RESET:
					m_WorkingMessageLength = MIDI_RESET;
					break;
				default:
					break;
			}
		}

		m_WorkingDataByteIndex = 0;
		m_CurrentWriteIndex = (m_CurrentWriteIndex + 1) % MIDI_BUFFER_SIZE;
	}
	else // if data byte
	{
		uint8_t* midiMessageBytes = m_MsgBuffer[(m_CurrentWriteIndex - 1 + MIDI_BUFFER_SIZE) % MIDI_BUFFER_SIZE].getRawData();
		m_WorkingDataByteIndex++;

		if ( m_WorkingDataByteIndex <= m_WorkingMessageLength )
		{
			midiMessageBytes[m_WorkingDataByteIndex] = byte;
		}
		else
		{
			m_CurrentWriteIndex = (m_CurrentWriteIndex + 1) % MIDI_BUFFER_SIZE;
			midiMessageBytes[0] = m_WorkingStatusByte;
			m_WorkingDataByteIndex = 1;
			midiMessageBytes[m_WorkingDataByteIndex] = byte;
		}
	}
}

MidiEvent* MidiHandler::nextMidiMessage()
{
	MidiEvent* retVal = nullptr;

	if ( m_CurrentReadIndex != m_CurrentWriteIndex )
	{
		retVal = &(m_MsgBuffer[m_CurrentReadIndex]);
		m_CurrentReadIndex = (m_CurrentReadIndex + 1) % MIDI_BUFFER_SIZE;
	}

	return retVal;
}

void MidiHandler::dispatchEvents()
{
	MidiEvent* nextMidiEvent = this->nextMidiMessage();

	while ( nextMidiEvent != nullptr )
	{
		const MidiEvent& midiEvent = *(nextMidiEvent);
		const uint8_t* const midiRawData = midiEvent.getRawData();
		IMidiEventListener::PublishEvent( midiEvent );

		if ( midiEvent.isPitchBend() )
		{
			uint8_t lsb = midiRawData[1];
			uint8_t msb = midiRawData[2];
			short pitchBendVal = (msb << 7) | lsb;

			// get normalized range between -1.0 and 1.0
			float pitchBendNormalized = ( (static_cast<float>(pitchBendVal) / 16383.0f) * 2.0f ) - 1.0f;

			// multiply by the number of semitones in bend range then divide by 12, then raise 2 to that power
			float pitchBendFactor = powf( 2.0f, (pitchBendNormalized * static_cast<float>(m_SemitonesToPitchBend)) / 12.0f );

			IPitchEventListener::PublishEvent( PitchEvent(pitchBendFactor) );
		}
		else if ( midiEvent.isNoteOn() )
		{
			IKeyEventListener::PublishEvent( KeyEvent(KeyPressedEnum::PRESSED, midiRawData[1], midiRawData[2]) );
		}
		else if ( midiEvent.isNoteOff() )
		{
			IKeyEventListener::PublishEvent( KeyEvent(KeyPressedEnum::RELEASED, midiRawData[1], midiRawData[2]) );
		}

		// get next MIDI message in the queue and continue while loop
		nextMidiEvent = this->nextMidiMessage();
	}
}

void MidiHandler::setNumberOfSemitonesToPitchBend (unsigned int numSemitones)
{
	m_SemitonesToPitchBend = numSemitones;
}

unsigned int MidiHandler::getNumberOfSemitonesToPitchBend()
{
	return m_SemitonesToPitchBend;
}
