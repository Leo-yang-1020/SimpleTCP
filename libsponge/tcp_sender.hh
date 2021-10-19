#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "./tcp_helpers/tcp_config.hh"
#include "./tcp_helpers/tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>
#include <set>
#include <optional>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initiail_retransmission_tmeout;

    //! current  RTO
    size_t _rto;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    //! to store the outgoing segment
    std::queue<TCPSegment> _outstanding_segment{};


    //!表示当接收到一个正确的报文时该报文的窗口大小
    uint16_t _window_size={1};

    //! the passed time after last ack
    size_t _time_passed={0};

    //!bytes sent but not acknowledged
    uint64_t _bytes_in_flight=0;


    unsigned int _consecutive_retransmissions={0};

    //! present the remaining space of receiver that should have after sending:
    //! More specifically, we assume that all segments sent will be received successfully
    //! by the receiver and the remaining space is _receiver_free_space
    //! _receiver_free_space!=window_size

    uint16_t _receiver_free_space={0};

    //!whether SYN segment sent
    bool _syn_sent=false;

    //!whether FIN segment sent
    bool _fin_sent=false;

    //!whether start the timer
    bool _timer_running=false;

    //! whether the zero_detect_window sent
    bool _zero_window_sent=false;


  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
    void send_segment(TCPSegment &segment);
    bool is_valid_ackno(uint64_t absolute_seg_ack,uint16_t window_size);
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
