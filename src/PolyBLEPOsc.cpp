#include "PolyBLEPOsc.hpp"

#include "AudioConstants.hpp"
#include <limits>
#include <math.h>

PolyBLEPOsc::PolyBLEPOsc() :
	m_Phase( 0.0f ),
	m_PhaseIncr( 0.0f ),
	m_LastOutput( 0.0f ),
	m_LastLastOutput( 0.0f ),
	m_A0( 1.0f ),
	m_B1( 0.0f ),
	m_OscMode( OscillatorMode::SINE )
{
}

PolyBLEPOsc::~PolyBLEPOsc()
{
}

float PolyBLEPOsc::nextSample()
{
	float output = 0.0f;

	if ( m_OscMode != OscillatorMode::SINE ) // if not a sine wave, we need to anti-alias using PolyBLEP
	{
		// calculate PolyBLEP to be used for SAW, SQUARE, and TRIANGLE
		float t = std::abs(m_Phase) / (2.0f * M_PI); // using abs since a frequency of 0 can cause NAN
		float dt = std::abs(m_PhaseIncr) / (2.0f * M_PI);
		float polyBLEPOutput = 0.0f;
		float polyBLEPSquareOutput = 0.0f;
		if ( t < dt )
		{
			t /= dt;
			polyBLEPOutput = t+t - t*t - 1.0f;
		}
		else if ( t > 1.0f - dt )
		{
			t = (t - 1.0f) / dt;
			polyBLEPOutput = t*t + t+t + 1.0f;
		}

		if ( m_OscMode == OscillatorMode::SQUARE || m_OscMode == OscillatorMode::TRIANGLE )
		{
			// calculate PolyBLEP for square wave (which later can be integrated into a triangle wave)
			t = fmod( (std::abs(m_Phase) / (2.0f * M_PI)) + 0.5f, 1.0f );

			if ( t < dt )
			{
				t /= dt;
				polyBLEPSquareOutput = t+t - t*t - 1.0f;
			}
			else if ( t > 1.0f - dt )
			{
				t = (t - 1.0f) / dt;
				polyBLEPSquareOutput = t*t + t+t + 1.0f;
			}

			// generate square waveform
			if ( m_Phase < M_PI )
			{
				output = 1.0f;
			}
			else
			{
				output = -1.0f;
			}

			// apply PolyBLEPs to square
			output += polyBLEPOutput;
			output -= polyBLEPSquareOutput;;

			if ( m_OscMode == OscillatorMode::TRIANGLE )
			{
				// doing some filtering to make a triangle wave
				m_LastLastOutput = (output * m_A0) + (m_LastOutput * m_B1);
				m_LastOutput = (m_LastOutput * m_A0) + (m_LastLastOutput * m_B1);

				output = m_LastOutput;
			}
		}
		else if ( m_OscMode == OscillatorMode::SAWTOOTH )
		{
			// generate sawtooth waveform and apply PolyBLEP
			output = ( (2.0f * m_Phase) / (2.0f * M_PI) ) - 1.0f;
			output -= polyBLEPOutput;
		}
	}
	else
	{
		output = sin( m_Phase );
	}

	// increment and wrap phase
	m_Phase += m_PhaseIncr;
	if ( m_Phase > (2.0f * M_PI) )
	{
		m_Phase -= (2.0f * M_PI);
	}
	else if ( m_Phase < (-2.0f * M_PI) ) // we also wrap for negative frequency
	{
		m_Phase += (2.0f * M_PI);
	}

	return output;
}

void PolyBLEPOsc::setFrequency (float frequency)
{
	m_PhaseIncr = (frequency * 2.0f * M_PI) / SAMPLE_RATE;
	m_B1 = exp( -2.0f * M_PI * (std::abs(frequency) / SAMPLE_RATE / 2.0f) ); // we need the abs value, since freq can be negative
	m_A0 = 1.0f - m_B1;
}

void PolyBLEPOsc::setOscillatorMode (const OscillatorMode& mode)
{
	m_OscMode = mode;
}

OscillatorMode PolyBLEPOsc::getOscillatorMode()
{
	return m_OscMode;
}
