// Implements seeking and rate control functionality.
#pragma once

#include <mfplay.h>

/*
    Usage:

    - Call SetTopology when you get the MF_TOPOSTATUS_READY session event.
    - Call SessionEvent for each session event.
    - Call Clear before closing the Media Session.
    - To coordinate rate-change requests with transport state, delegate all
      stop, pause, and play commands to the PlayerSeeking class.

*/

class PlayerSeeking
{
    class CritSec
    {
    private:
        CRITICAL_SECTION m_criticalSection;
    public:
        CritSec()
        {
            InitializeCriticalSection(&m_criticalSection);
        }
        ~CritSec()
        {
            DeleteCriticalSection(&m_criticalSection);
        }
        void Lock()
        {
            EnterCriticalSection(&m_criticalSection);
        }
        void Unlock()
        {
            LeaveCriticalSection(&m_criticalSection);
        }
    };

    class AutoLock
    {
    private:
        CritSec* m_pCriticalSection;
    public:
        AutoLock(CritSec& crit)
        {
            m_pCriticalSection = &crit;
            m_pCriticalSection->Lock();
        }
        ~AutoLock()
        {
            m_pCriticalSection->Unlock();
        }
    };

public:

    PlayerSeeking();
    virtual ~PlayerSeeking();

    HRESULT SetTopology(IMFMediaSession* pSession, IMFTopology* pTopology);
    HRESULT Clear();
    HRESULT SessionEvent(MediaEventType type, HRESULT hrStatus, IMFMediaEvent* pEvent);

    HRESULT CanSeek(BOOL* pbCanSeek);
    HRESULT GetDuration(MFTIME* phnsDuration);
    HRESULT GetCurrentPosition(MFTIME* phnsPosition);
    HRESULT SetPosition(MFTIME hnsPosition);

    HRESULT CanScrub(BOOL* pbCanScrub);
    HRESULT Scrub(BOOL bScrub);

    HRESULT CanFastForward(BOOL* pbCanFF);
    HRESULT CanRewind(BOOL* pbCanRewind);
    HRESULT SetRate(float fRate);
    HRESULT FastForward();
    HRESULT Rewind();

    HRESULT Start();
    HRESULT Pause();
    HRESULT Stop();

private:

    enum Command
    {
        CmdNone = 0,
        CmdStop,
        CmdStart,
        CmdPause,
        CmdSeek,
    };

    HRESULT SetPositionInternal(const MFTIME& hnsPosition);
    HRESULT CommitRateChange(float fRate, BOOL bThin);
    float   GetNominalRate();

    HRESULT OnSessionStart(HRESULT hr);
    HRESULT OnSessionStop(HRESULT hr);
    HRESULT OnSessionPause(HRESULT hr);
    HRESULT OnSessionEnded(HRESULT hr);

    HRESULT UpdatePendingCommands(Command cmd);

private:

    // Describes the current or requested state, with respect to seeking and 
    // playback rate.
    struct SeekState
    {
        Command command;
        float   fRate;      // Playback rate
        BOOL    bThin;      // Thinned playback?
        MFTIME  hnsStart;   // Start position
    };

    BOOL        m_bPending;     // Is a request pending?

    SeekState   m_state;        // Current nominal state.
    SeekState   m_request;      // Pending request.

    CritSec     m_critsec;      // Protects the seeking and rate-change states.

    DWORD       m_caps;         // Session caps.
    BOOL        m_bCanScrub;    // Does the current session support rate = 0.

    MFTIME      m_hnsDuration;  // Duration of the current presentation.
    float       m_fPrevRate;

    IMFMediaSession* m_pSession;
    IMFRateControl* m_pRate;
    IMFRateSupport* m_pRateSupport;
    IMFPresentationClock* m_pClock;
};